#include "mainwindow.h"

#include <QApplication>
#include <QBoxLayout>
#include <QDateTime>
#include <QFile>
#include <QFrame>
#include <QGridLayout>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QRandomGenerator>
#include <QTextStream>

static const char *kBaseStyle = R"(
    QMainWindow { background: #07111f; }
    QWidget { background: #07111f; color: #e6eef8; }
    QLabel { color: #e6eef8; }
    QFrame#Card {
        background: #101d2f;
        border: 1px solid #263b58;
        border-radius: 14px;
    }
    QLabel#CardTitle {
        color: #8fb3d9;
        font-size: 13px;
        font-weight: 500;
    }
    QLabel#CardValue {
        color: #ffffff;
        font-size: 26px;
        font-weight: 700;
    }
    QLabel#SmallText {
        color: #aab9c9;
        font-size: 12px;
    }
    QLabel#PageTitle {
        color: #ffffff;
        font-size: 25px;
        font-weight: 800;
    }
    QLabel#ModeBadge {
        background: #243b58;
        color: #cde7ff;
        border-radius: 12px;
        padding: 5px 12px;
        font-size: 12px;
        font-weight: 700;
    }
    QPushButton#NavButton {
        background: transparent;
        color: #9fb5cc;
        border: none;
        border-radius: 10px;
        padding: 12px 10px;
        text-align: left;
        font-size: 15px;
        font-weight: 600;
    }
    QPushButton#NavButton:checked {
        background: #1f6feb;
        color: #ffffff;
    }
    QPushButton#NavButton:pressed {
        background: #2b80ff;
    }
)";

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_stack(nullptr),
      m_timer(new QTimer(this)),
      m_statusPath("/tmp/edgeguard_status.json"),
      m_demoMode(false),
      m_demoCounter(0),
      m_modeBadge(nullptr),
      m_stateLabel(nullptr),
      m_alarmReasonLabel(nullptr),
      m_alsLabel(nullptr),
      m_psLabel(nullptr),
      m_motionLabel(nullptr),
      m_ledLabel(nullptr),
      m_buzzerLabel(nullptr),
      m_timeLabel(nullptr),
      m_accelLabel(nullptr),
      m_gyroLabel(nullptr),
      m_ap3216cLabel(nullptr),
      m_rawTempLabel(nullptr),
      m_alarmStateLabel(nullptr),
      m_alarmCountLabel(nullptr),
      m_lastAlarmLabel(nullptr),
      m_intervalLabel(nullptr),
      m_thresholdLabel(nullptr),
      m_ipLabel(nullptr),
      m_uploadLabel(nullptr),
      m_networkLabel(nullptr)
{
    parseArguments();
    buildUi();

    connect(m_timer, &QTimer::timeout, this, &MainWindow::refreshStatus);
    m_timer->start(500);
    refreshStatus();
}

void MainWindow::parseArguments()
{
    const QStringList args = QApplication::arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == "--demo") {
            m_demoMode = true;
        } else if (args[i] == "--status" && i + 1 < args.size()) {
            m_statusPath = args[++i];
        } else if (args[i] == "--page" && i + 1 < args.size()) {
            // Applied after UI creation in buildUi(). Kept simple here.
        }
    }
}

void MainWindow::buildUi()
{
    setStyleSheet(kBaseStyle);
    resize(800, 480);

    QWidget *root = new QWidget(this);
    QHBoxLayout *mainLayout = new QHBoxLayout(root);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    QWidget *sidebar = buildSidebar();
    mainLayout->addWidget(sidebar, 0);

    m_stack = new QStackedWidget(root);
    m_stack->addWidget(buildDashboardPage());
    m_stack->addWidget(buildSensorPage());
    m_stack->addWidget(buildAlarmPage());
    m_stack->addWidget(buildSettingsPage());
    m_stack->addWidget(buildNetworkPage());
    mainLayout->addWidget(m_stack, 1);

    setCentralWidget(root);
    switchPage(0);

    const QStringList args = QApplication::arguments();
    int pageIndex = -1;
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == "--page" && i + 1 < args.size()) {
            pageIndex = args[++i].toInt();
            break;
        }
    }
    if (pageIndex >= 0 && pageIndex < m_stack->count())
        switchPage(pageIndex);
}

QWidget *MainWindow::buildSidebar()
{
    QFrame *side = new QFrame(this);
    side->setObjectName("Card");
    side->setFixedWidth(166);

    QVBoxLayout *layout = new QVBoxLayout(side);
    layout->setContentsMargins(14, 16, 14, 16);
    layout->setSpacing(8);

    QLabel *brand = new QLabel("EdgeGuard\n6ULL", side);
    brand->setStyleSheet("font-size: 24px; font-weight: 900; color: #ffffff; line-height: 1.1;");
    layout->addWidget(brand);

    QLabel *sub = makeSmallText("Local Monitor UI");
    layout->addWidget(sub);
    layout->addSpacing(14);

    const QStringList names = {"1  Dashboard", "2  Sensors", "3  Alarms", "4  Settings", "5  Network"};
    for (int i = 0; i < names.size(); ++i) {
        QPushButton *btn = new QPushButton(names[i], side);
        btn->setObjectName("NavButton");
        btn->setCheckable(true);
        btn->setMinimumHeight(44);
        connect(btn, &QPushButton::clicked, this, [this, i]() { switchPage(i); });
        layout->addWidget(btn);
        m_navButtons.push_back(btn);
    }

    layout->addStretch(1);
    QLabel *hint = makeSmallText("No touch yet:\nuse --page N or\nkeyboard 1-5.");
    layout->addWidget(hint);

    return side;
}

QWidget *MainWindow::buildDashboardPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    QHBoxLayout *top = new QHBoxLayout();
    top->addWidget(makeTitle("Dashboard"));
    top->addStretch(1);
    m_modeBadge = new QLabel("STATUS", page);
    m_modeBadge->setObjectName("ModeBadge");
    top->addWidget(m_modeBadge);
    layout->addLayout(top);

    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(12);

    grid->addWidget(makeCard("System State", &m_stateLabel), 0, 0);
    grid->addWidget(makeCard("Alarm Reason", &m_alarmReasonLabel), 0, 1);
    grid->addWidget(makeCard("ALS Light", &m_alsLabel), 1, 0);
    grid->addWidget(makeCard("Proximity", &m_psLabel), 1, 1);
    grid->addWidget(makeCard("Motion Delta", &m_motionLabel), 2, 0);
    grid->addWidget(makeCard("LED / Buzzer", &m_ledLabel), 2, 1);

    layout->addLayout(grid, 1);

    m_timeLabel = makeSmallText("Last update: --");
    layout->addWidget(m_timeLabel);

    return page;
}

QWidget *MainWindow::buildSensorPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setSpacing(12);
    layout->addWidget(makeTitle("Sensor Data"));

    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(12);
    grid->addWidget(makeCard("MPU6050 Accel", &m_accelLabel), 0, 0);
    grid->addWidget(makeCard("MPU6050 Gyro", &m_gyroLabel), 0, 1);
    grid->addWidget(makeCard("AP3216C", &m_ap3216cLabel), 1, 0);
    grid->addWidget(makeCard("Temp Raw", &m_rawTempLabel), 1, 1);
    layout->addLayout(grid, 1);
    return page;
}

QWidget *MainWindow::buildAlarmPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setSpacing(12);
    layout->addWidget(makeTitle("Alarm Center"));

    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(12);
    grid->addWidget(makeCard("Current Alarm", &m_alarmStateLabel), 0, 0);
    grid->addWidget(makeCard("Alarm Count", &m_alarmCountLabel), 0, 1);
    grid->addWidget(makeCard("Last Alarm", &m_lastAlarmLabel), 1, 0, 1, 2);
    layout->addLayout(grid, 1);
    return page;
}

QWidget *MainWindow::buildSettingsPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setSpacing(12);
    layout->addWidget(makeTitle("Settings Preview"));

    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(12);
    grid->addWidget(makeCard("Sample Interval", &m_intervalLabel), 0, 0);
    grid->addWidget(makeCard("Thresholds", &m_thresholdLabel), 0, 1);
    layout->addLayout(grid, 1);
    return page;
}

QWidget *MainWindow::buildNetworkPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setSpacing(12);
    layout->addWidget(makeTitle("Network"));

    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(12);
    grid->addWidget(makeCard("Board IP", &m_ipLabel), 0, 0);
    grid->addWidget(makeCard("Upload Status", &m_uploadLabel), 0, 1);
    grid->addWidget(makeCard("Link", &m_networkLabel), 1, 0, 1, 2);
    layout->addLayout(grid, 1);
    return page;
}

QWidget *MainWindow::makeCard(const QString &title, QLabel **valueLabel, const QString &initial)
{
    QFrame *card = new QFrame(this);
    card->setObjectName("Card");
    card->setMinimumHeight(105);

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(8);

    QLabel *t = new QLabel(title, card);
    t->setObjectName("CardTitle");
    layout->addWidget(t);

    QLabel *v = new QLabel(initial, card);
    v->setObjectName("CardValue");
    v->setWordWrap(true);
    layout->addWidget(v, 1);

    *valueLabel = v;
    return card;
}

QLabel *MainWindow::makeTitle(const QString &text)
{
    QLabel *label = new QLabel(text, this);
    label->setObjectName("PageTitle");
    return label;
}

QLabel *MainWindow::makeSmallText(const QString &text)
{
    QLabel *label = new QLabel(text, this);
    label->setObjectName("SmallText");
    label->setWordWrap(true);
    return label;
}

void MainWindow::switchPage(int index)
{
    if (!m_stack || index < 0 || index >= m_stack->count())
        return;
    m_stack->setCurrentIndex(index);
    updateNavStyle();
}

void MainWindow::updateNavStyle()
{
    for (int i = 0; i < m_navButtons.size(); ++i)
        m_navButtons[i]->setChecked(i == m_stack->currentIndex());
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_1: switchPage(0); break;
    case Qt::Key_2: switchPage(1); break;
    case Qt::Key_3: switchPage(2); break;
    case Qt::Key_4: switchPage(3); break;
    case Qt::Key_5: switchPage(4); break;
    case Qt::Key_Escape: close(); break;
    default: QMainWindow::keyPressEvent(event); break;
    }
}

bool MainWindow::loadStatusFromFile(QJsonObject *obj)
{
    QFile file(m_statusPath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    *obj = doc.object();
    return true;
}

QJsonObject MainWindow::makeDemoStatus()
{
    ++m_demoCounter;
    QJsonObject obj;
    const int phase = m_demoCounter % 30;

    QString state = "NORMAL";
    QString reason = "none";
    QString led = "green";
    QString buzzer = "off";
    int motion = 3200 + (phase * 190);
    int als = 420 - phase * 8;
    int ps = 18 + phase * 3;

    if (phase > 18 && phase <= 24) {
        state = "LIGHT_ALARM";
        reason = "low light / blocked";
        led = "yellow";
        als = 45;
    } else if (phase > 24) {
        state = "MOTION_ALARM";
        reason = "motion threshold exceeded";
        led = "red";
        buzzer = "beep";
        motion = 15600;
    }

    obj["state"] = state;
    obj["alarm_reason"] = reason;
    obj["motion_delta"] = motion;
    obj["ir"] = 15 + phase;
    obj["als"] = als;
    obj["ps"] = ps;
    obj["ax"] = 100 + phase * 3;
    obj["ay"] = -30 + phase;
    obj["az"] = 16320 - phase * 2;
    obj["gx"] = 3 + phase;
    obj["gy"] = -1;
    obj["gz"] = phase / 2;
    obj["temp_raw"] = 5200 + phase;
    obj["led"] = led;
    obj["buzzer"] = buzzer;
    obj["alarm_count"] = phase > 24 ? 3 : 2;
    obj["last_alarm"] = phase > 18 ? "recent" : "none";
    obj["sample_interval_ms"] = 500;
    obj["als_low_threshold"] = 80;
    obj["ps_high_threshold"] = 200;
    obj["motion_threshold"] = 12000;
    obj["ip"] = "192.168.10.2";
    obj["upload"] = "disabled";
    obj["network"] = "eth0 ready";
    return obj;
}

QString MainWindow::valueToString(const QJsonObject &obj, const QString &key, const QString &fallback) const
{
    const QJsonValue v = obj.value(key);
    if (v.isString())
        return v.toString();
    if (v.isDouble())
        return QString::number(v.toInt());
    if (v.isBool())
        return v.toBool() ? "true" : "false";
    return fallback;
}

int MainWindow::valueToInt(const QJsonObject &obj, const QString &key, int fallback) const
{
    const QJsonValue v = obj.value(key);
    if (v.isDouble())
        return v.toInt();
    if (v.isString()) {
        bool ok = false;
        int n = v.toString().toInt(&ok);
        return ok ? n : fallback;
    }
    return fallback;
}

void MainWindow::refreshStatus()
{
    QJsonObject obj;
    bool demo = m_demoMode;

    if (!demo) {
        if (!loadStatusFromFile(&obj)) {
            demo = true;
            obj = makeDemoStatus();
        }
    } else {
        obj = makeDemoStatus();
    }

    applyStatus(obj, demo);
}

void MainWindow::applyStatus(const QJsonObject &obj, bool demo)
{
    const QString state = valueToString(obj, "state", "UNKNOWN");
    const QString reason = valueToString(obj, "alarm_reason", "none");
    const QString led = valueToString(obj, "led", "--");
    const QString buzzer = valueToString(obj, "buzzer", "--");
    const int ax = valueToInt(obj, "ax");
    const int ay = valueToInt(obj, "ay");
    const int az = valueToInt(obj, "az");
    const int gx = valueToInt(obj, "gx");
    const int gy = valueToInt(obj, "gy");
    const int gz = valueToInt(obj, "gz");
    const int ir = valueToInt(obj, "ir");
    const int als = valueToInt(obj, "als");
    const int ps = valueToInt(obj, "ps");
    const int motion = valueToInt(obj, "motion_delta");

    m_modeBadge->setText(demo ? "DEMO / NO STATUS FILE" : "LIVE");
    m_stateLabel->setText(state);
    m_alarmReasonLabel->setText(reason);
    m_alsLabel->setText(QString::number(als));
    m_psLabel->setText(QString::number(ps));
    m_motionLabel->setText(QString::number(motion));
    m_ledLabel->setText(QString("%1 / %2").arg(led, buzzer));
    m_buzzerLabel = nullptr;
    m_timeLabel->setText("Last update: " + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));

    m_accelLabel->setText(QString("%1, %2, %3").arg(ax).arg(ay).arg(az));
    m_gyroLabel->setText(QString("%1, %2, %3").arg(gx).arg(gy).arg(gz));
    m_ap3216cLabel->setText(QString("IR=%1\nALS=%2\nPS=%3").arg(ir).arg(als).arg(ps));
    m_rawTempLabel->setText(valueToString(obj, "temp_raw", "--"));

    m_alarmStateLabel->setText(state + "\n" + reason);
    m_alarmCountLabel->setText(valueToString(obj, "alarm_count", "0"));
    m_lastAlarmLabel->setText(valueToString(obj, "last_alarm", "none"));

    m_intervalLabel->setText(valueToString(obj, "sample_interval_ms", "500") + " ms");
    m_thresholdLabel->setText(QString("ALS < %1\nPS > %2\nMotion > %3")
                              .arg(valueToString(obj, "als_low_threshold", "80"),
                                   valueToString(obj, "ps_high_threshold", "200"),
                                   valueToString(obj, "motion_threshold", "12000")));

    m_ipLabel->setText(valueToString(obj, "ip", "--"));
    m_uploadLabel->setText(valueToString(obj, "upload", "disabled"));
    m_networkLabel->setText(valueToString(obj, "network", "unknown"));

    if (state.contains("MOTION") || state.contains("ALARM")) {
        m_stateLabel->setStyleSheet("color: #ff5c5c; font-size: 26px; font-weight: 900;");
    } else if (state.contains("WARNING") || state.contains("LIGHT") || state.contains("PROXIMITY")) {
        m_stateLabel->setStyleSheet("color: #ffd166; font-size: 26px; font-weight: 900;");
    } else {
        m_stateLabel->setStyleSheet("color: #4ade80; font-size: 26px; font-weight: 900;");
    }
}
