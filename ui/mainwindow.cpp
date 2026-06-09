#include "mainwindow.h"
#include "loginpage.h"

#include <QApplication>
#include <QBoxLayout>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFrame>
#include <QGridLayout>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QRandomGenerator>
#include <QTextStream>
#include <QDebug>

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
    QPushButton#ActionButton {
        background: #1f6feb;
        color: #ffffff;
        border: none;
        border-radius: 10px;
        padding: 10px 18px;
        font-size: 14px;
        font-weight: 700;
    }
    QPushButton#ActionButton:pressed {
        background: #2b80ff;
    }
)";

static const int kSidebarWidth    = 166;
static const int kMaxFailures     = 3;
static const char *kCmdTmpPath    = "/tmp/edgeguard_cmd.json.tmp";
static const char *kCmdPath       = "/tmp/edgeguard_cmd.json";

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_stack(nullptr),
      m_timer(new QTimer(this)),
      m_statusPath("/tmp/edgeguard_status.json"),
      m_demoMode(false),
      m_demoCounter(0),
      m_consecutiveFailures(0),
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
      m_mpuOnlineLabel(nullptr),
      m_apOnlineLabel(nullptr),
      m_alarmStateLabel(nullptr),
      m_alarmCountLabel(nullptr),
      m_lastAlarmLabel(nullptr),
      m_muteBtn(nullptr),
      m_ackBtn(nullptr),
      m_intervalLabel(nullptr),
      m_thresholdLabel(nullptr),
      m_ipLabel(nullptr),
      m_uptimeLabel(nullptr),
      m_serviceLabel(nullptr),
      m_networkLabel(nullptr),
      m_loginPage(nullptr),
      m_sidebar(nullptr),
      m_authenticated(false),
      m_pressing(false),
      m_swiped(false)
{
    parseArguments();
    buildUi();

    if (m_loginPage) {
        connect(m_loginPage, &LoginPage::loginSuccess, this, &MainWindow::onLoginSuccess);
        connect(m_loginPage, &LoginPage::demoRequested, this, &MainWindow::onDemoRequested);
    }
    if (m_demoMode)
        onDemoRequested();

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
            // Applied after UI creation in buildUi().
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

    m_sidebar = buildSidebar();
    m_sidebar->setFixedWidth(0);  // collapsed on login
    mainLayout->addWidget(m_sidebar, 0);

    m_stack = new QStackedWidget(root);
    m_loginPage = new LoginPage(root);
    m_stack->addWidget(m_loginPage);         // 0: Login
    m_stack->addWidget(buildDashboardPage());// 1: Dashboard
    m_stack->addWidget(buildSensorPage());   // 2: Sensors
    m_stack->addWidget(buildAlarmPage());    // 3: Alarms
    m_stack->addWidget(buildSettingsPage()); // 4: Settings
    m_stack->addWidget(buildSystemPage());   // 5: System
    mainLayout->addWidget(m_stack, 1);

    setCentralWidget(root);
    switchPage(0);  // start on login

    const QStringList args = QApplication::arguments();
    int pageIndex = -1;
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == "--page" && i + 1 < args.size()) {
            pageIndex = args[++i].toInt();
            break;
        }
    }
    if (pageIndex >= 0 && pageIndex + 1 < m_stack->count())
        switchPage(pageIndex + 1);  // 0→1(Dashboard) etc
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

    const QStringList names = {"1  Dashboard", "2  Sensors", "3  Alarms", "4  Settings", "5  System"};
    for (int i = 0; i < names.size(); ++i) {
        QPushButton *btn = new QPushButton(names[i], side);
        btn->setObjectName("NavButton");
        btn->setCheckable(true);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setMinimumHeight(44);
        connect(btn, &QPushButton::clicked, this, [this, i]() { switchPage(i + 1); });
        layout->addWidget(btn);
        m_navButtons.push_back(btn);
    }

    layout->addStretch(1);

    QPushButton *logoutBtn = new QPushButton("Logout", side);
    logoutBtn->setObjectName("NavButton");
    logoutBtn->setFocusPolicy(Qt::NoFocus);
    logoutBtn->setMinimumHeight(44);
    logoutBtn->setStyleSheet("QPushButton#NavButton { color: #ff5c5c; } QPushButton#NavButton:pressed { background: #2b80ff; color: #ffffff; }");
    connect(logoutBtn, &QPushButton::clicked, this, [this]() {
        m_authenticated = false;
        if (m_loginPage) m_loginPage->reset();
        switchPage(0);
    });
    layout->addWidget(logoutBtn);

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
    grid->addWidget(makeCard("Temperature", &m_rawTempLabel), 1, 1);
    layout->addLayout(grid, 1);

    QHBoxLayout *onlineRow = new QHBoxLayout();
    m_mpuOnlineLabel = makeSmallText("MPU6050: --");
    m_apOnlineLabel  = makeSmallText("AP3216C: --");
    onlineRow->addWidget(m_mpuOnlineLabel);
    onlineRow->addWidget(m_apOnlineLabel);
    onlineRow->addStretch(1);
    layout->addLayout(onlineRow);

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
    layout->addLayout(grid);

    /* Action buttons */
    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(16);

    m_muteBtn = new QPushButton("Mute Buzzer", page);
    m_muteBtn->setObjectName("ActionButton");
    m_muteBtn->setFocusPolicy(Qt::NoFocus);
    m_muteBtn->setMinimumHeight(42);
    connect(m_muteBtn, &QPushButton::clicked, this, &MainWindow::onMuteClicked);
    btnRow->addWidget(m_muteBtn);

    m_ackBtn = new QPushButton("Acknowledge", page);
    m_ackBtn->setObjectName("ActionButton");
    m_ackBtn->setFocusPolicy(Qt::NoFocus);
    m_ackBtn->setMinimumHeight(42);
    connect(m_ackBtn, &QPushButton::clicked, this, &MainWindow::onAckClicked);
    btnRow->addWidget(m_ackBtn);

    btnRow->addStretch(1);
    layout->addLayout(btnRow);
    layout->addStretch(1);
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

QWidget *MainWindow::buildSystemPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setSpacing(12);
    layout->addWidget(makeTitle("System"));

    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(12);
    grid->addWidget(makeCard("Board IP", &m_ipLabel), 0, 0);
    grid->addWidget(makeCard("Uptime (sec)", &m_uptimeLabel), 0, 1);
    grid->addWidget(makeCard("sensor_hubd", &m_serviceLabel), 1, 0);
    grid->addWidget(makeCard("Network Link", &m_networkLabel), 1, 1);
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
    // auth gate: block navigation away from login
    if (!m_authenticated && index > 0 && !m_demoMode)
        return;
    if (m_stack->currentIndex() == index) return;

    // sidebar: collapsed on login, visible on dashboard+
    if (m_sidebar)
        m_sidebar->setFixedWidth(index == 0 ? 0 : kSidebarWidth);

    m_stack->setCurrentIndex(index);
    updateNavStyle();
}

void MainWindow::updateNavStyle()
{
    int visual = m_stack->currentIndex() - 1;  // page 1→nav 0, page 2→nav 1, etc
    for (int i = 0; i < m_navButtons.size(); ++i)
        m_navButtons[i]->setChecked(i == visual);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_1: switchPage(1); break;  // Dashboard
    case Qt::Key_2: switchPage(2); break;  // Sensors
    case Qt::Key_3: switchPage(3); break;  // Alarms
    case Qt::Key_4: switchPage(4); break;  // Settings
    case Qt::Key_5: switchPage(5); break;  // System
    case Qt::Key_Escape: close(); break;
    default: QMainWindow::keyPressEvent(event); break;
    }
}

/* ---- Login slots ---- */
void MainWindow::onLoginSuccess()
{
    m_authenticated = true;
    switchPage(1);
}

void MainWindow::onDemoRequested()
{
    m_demoMode = true;
    m_authenticated = true;
    switchPage(1);
}

/* ---- Swipe via Qt mouse events ---- */

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    m_pressPos = event->pos();
    m_pressing = true;
    m_swiped = false;
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_pressing || m_swiped) {
        QMainWindow::mouseMoveEvent(event);
        return;
    }

    int dx = event->pos().x() - m_pressPos.x();
    int dy = event->pos().y() - m_pressPos.y();

    // swipe UP (dy < 0) → next page; swipe DOWN (dy > 0) → prev page
    if (abs(dy) > kSwipeThresh && abs(dx) < abs(dy)) {
        m_swiped = true;
        int cur = m_stack ? m_stack->currentIndex() : 0;
        int cnt = m_stack ? m_stack->count() : 0;
        qDebug("[swipe] dy=%d page=%d/%d", dy, cur, cnt);
        if (dy < 0 && cur < cnt - 1)
            switchPage(cur + 1);
        else if (dy > 0 && cur > 1)           // never swipe back to login (page 0)
            switchPage(cur - 1);
    }

    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    m_pressing = false;
    QMainWindow::mouseReleaseEvent(event);
}

/* ---- Command channel ---- */

void MainWindow::sendCommand(const QString &cmd)
{
    QFile file(kCmdTmpPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    QJsonObject obj;
    obj["cmd"] = cmd;
    QJsonDocument doc(obj);
    file.write(doc.toJson(QJsonDocument::Compact));
    file.close();

    ::rename(kCmdTmpPath, kCmdPath);
    qDebug("Command sent: %s", qPrintable(cmd));
}

void MainWindow::onMuteClicked()
{
    sendCommand("mute_buzzer");
}

void MainWindow::onAckClicked()
{
    sendCommand("ack_alarm");
}

/* ---- Data loading ---- */

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
        state = "WARNING";
        reason = "low ambient light";
        led = "yellow";
        als = 45;
    } else if (phase > 24) {
        state = "ALARM";
        reason = "motion threshold exceeded";
        led = "red";
        buzzer = "beep";
        motion = 15600;
    }

    /* Build nested demo JSON matching new protocol */
    QJsonObject mpu;
    mpu["ax"] = 100 + phase * 3;
    mpu["ay"] = -30 + phase;
    mpu["az"] = 16320 - phase * 2;
    mpu["gx"] = 3 + phase;
    mpu["gy"] = -1;
    mpu["gz"] = phase / 2;
    mpu["temp"] = 25.0 + phase * 0.1;
    mpu["motion_delta"] = motion;
    mpu["online"] = true;

    QJsonObject ap3216;
    ap3216["ir"] = 15 + phase;
    ap3216["als"] = als;
    ap3216["ps"] = ps;
    ap3216["online"] = true;

    QJsonObject device;
    device["led"] = led;
    device["buzzer"] = buzzer;
    device["key"] = "released";

    QJsonObject alarm;
    alarm["count"] = phase > 24 ? 3 : 2;
    alarm["last"] = phase > 18 ? "2026-05-26 18:20:00" : "none";
    alarm["muted"] = false;
    alarm["acknowledged"] = false;

    QJsonObject sys;
    sys["uptime_sec"] = (double)(m_demoCounter * 30);
    sys["ip"] = "192.168.10.2";
    sys["sensor_hubd"] = "running";

    obj["state"] = state;
    obj["alarm_reason"] = reason;
    obj["timestamp_ms"] = (double)(m_demoCounter * 500);
    obj["mpu6050"] = mpu;
    obj["ap3216c"] = ap3216;
    obj["device"] = device;
    obj["alarm"] = alarm;
    obj["system"] = sys;

    return obj;
}

/* ---- Status application (nested JSON) ---- */

void MainWindow::applyServiceLost()
{
    m_consecutiveFailures++;
    if (m_consecutiveFailures > kMaxFailures) {
        m_modeBadge->setText("SERVICE LOST");
        m_modeBadge->setStyleSheet("background: #ff5c5c; color: #ffffff; "
                                   "border-radius: 12px; padding: 5px 12px; "
                                   "font-size: 12px; font-weight: 700;");
    }
}

void MainWindow::applyStatus(const QJsonObject &obj, bool demo)
{
    m_consecutiveFailures = 0;

    /* Top-level */
    const QString state   = valueToString(obj, "state", "UNKNOWN");
    const QString reason  = valueToString(obj, "alarm_reason", "none");

    /* Nested groups */
    QJsonObject mpu    = obj.value("mpu6050").toObject();
    QJsonObject ap3216 = obj.value("ap3216c").toObject();
    QJsonObject device = obj.value("device").toObject();
    QJsonObject alarm  = obj.value("alarm").toObject();
    QJsonObject sys    = obj.value("system").toObject();

    /* ---- Dashboard ---- */
    m_modeBadge->setText(demo ? "DEMO" : "LIVE");
    m_modeBadge->setStyleSheet("");  /* reset to default */

    m_stateLabel->setText(state);
    m_alarmReasonLabel->setText(reason);
    m_alsLabel->setText(QString::number(valueToInt(ap3216, "als")));
    m_psLabel->setText(QString::number(valueToInt(ap3216, "ps")));
    m_motionLabel->setText(QString::number(valueToInt(mpu, "motion_delta")));
    m_ledLabel->setText(QString("%1 / %2")
                        .arg(valueToString(device, "led", "--"),
                             valueToString(device, "buzzer", "--")));
    {
        qint64 ts_ms = obj.value("timestamp_ms").toVariant().toLongLong();
        if (ts_ms > 0) {
            qint64 secs = ts_ms / 1000;
            m_timeLabel->setText(QString("Data age: %1 s ago")
                                 .arg(QDateTime::currentSecsSinceEpoch() - secs));
        } else {
            m_timeLabel->setText("Last update: " +
                                 QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
        }
    }

    /* ---- Sensors ---- */
    m_accelLabel->setText(QString("%1, %2, %3")
                          .arg(valueToInt(mpu, "ax"))
                          .arg(valueToInt(mpu, "ay"))
                          .arg(valueToInt(mpu, "az")));
    m_gyroLabel->setText(QString("%1, %2, %3")
                          .arg(valueToInt(mpu, "gx"))
                          .arg(valueToInt(mpu, "gy"))
                          .arg(valueToInt(mpu, "gz")));
    m_ap3216cLabel->setText(QString("IR=%1\nALS=%2\nPS=%3")
                            .arg(valueToInt(ap3216, "ir"))
                            .arg(valueToInt(ap3216, "als"))
                            .arg(valueToInt(ap3216, "ps")));
    /* temp is a double in new protocol */
    {
        QJsonValue tv = mpu.value("temp");
        if (tv.isDouble())
            m_rawTempLabel->setText(QString::number(tv.toDouble(), 'f', 1) + " C");
        else
            m_rawTempLabel->setText(valueToString(mpu, "temp", "--"));
    }
    m_mpuOnlineLabel->setText(QString("MPU6050: %1")
                              .arg(mpu.value("online").toBool() ? "ONLINE" : "OFFLINE"));
    m_apOnlineLabel->setText(QString("AP3216C: %1")
                              .arg(ap3216.value("online").toBool() ? "ONLINE" : "OFFLINE"));

    /* ---- Alarms ---- */
    m_alarmStateLabel->setText(state + "\n" + reason);
    m_alarmCountLabel->setText(valueToString(alarm, "count", "0"));
    m_lastAlarmLabel->setText(valueToString(alarm, "last", "none"));

    /* ---- Settings — read from config block in status JSON ---- */
    {
        QJsonObject cfg = obj.value("config").toObject();
        if (!cfg.isEmpty()) {
            int iv = cfg.value("sample_interval_ms").toInt(500);
            m_intervalLabel->setText(QString("%1 ms").arg(iv));
            QString th;
            th += QString("ALS low: %1  ").arg(cfg.value("als_low_threshold").toInt(80));
            th += QString("PS warn: %1  ").arg(cfg.value("ps_warning_threshold").toInt(120));
            th += QString("PS alarm: %1\n").arg(cfg.value("ps_alarm_threshold").toInt(220));
            th += QString("Motion warn: %1  ").arg(cfg.value("motion_warning_threshold").toInt(8000));
            th += QString("Motion alarm: %1").arg(cfg.value("motion_alarm_threshold").toInt(15000));
            m_thresholdLabel->setText(th);
        } else {
            m_intervalLabel->setText("500 ms (default)");
            m_thresholdLabel->setText("Waiting for sensor_hubd...");
        }
    }

    /* ---- System ---- */
    m_ipLabel->setText(valueToString(sys, "ip", "--"));
    {
        QJsonValue uv = sys.value("uptime_sec");
        if (uv.isDouble())
            m_uptimeLabel->setText(QString::number((int)uv.toDouble()));
        else
            m_uptimeLabel->setText(valueToString(sys, "uptime_sec", "--"));
    }
    m_serviceLabel->setText(valueToString(sys, "sensor_hubd", "unknown"));
    {
        QFile nf("/sys/class/net/eth0/operstate");
        if (nf.open(QIODevice::ReadOnly)) {
            m_networkLabel->setText("eth0 " + QString::fromUtf8(nf.readAll()).trimmed());
        } else {
            m_networkLabel->setText("eth0 unknown");
        }
    }

    /* ---- Color-coding ---- */
    if (state.contains("FAULT")) {
        m_stateLabel->setStyleSheet("color: #ff5c5c; font-size: 26px; font-weight: 900;");
    } else if (state.contains("ALARM")) {
        m_stateLabel->setStyleSheet("color: #ff5c5c; font-size: 26px; font-weight: 900;");
    } else if (state.contains("WARNING")) {
        m_stateLabel->setStyleSheet("color: #ffd166; font-size: 26px; font-weight: 900;");
    } else {
        m_stateLabel->setStyleSheet("color: #4ade80; font-size: 26px; font-weight: 900;");
    }
}

QString MainWindow::valueToString(const QJsonObject &obj, const QString &key, const QString &fallback) const
{
    const QJsonValue v = obj.value(key);
    if (v.isString())  return v.toString();
    if (v.isDouble())  return QString::number(v.toDouble(), 'f', 1);
    if (v.isBool())    return v.toBool() ? "true" : "false";
    return fallback;
}

int MainWindow::valueToInt(const QJsonObject &obj, const QString &key, int fallback) const
{
    const QJsonValue v = obj.value(key);
    if (v.isDouble())  return v.toInt();
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
            applyServiceLost();
            return;
        }
    } else {
        obj = makeDemoStatus();
    }

    applyStatus(obj, demo);
}
