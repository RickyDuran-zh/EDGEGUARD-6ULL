#include "mainwindow.h"
#include "loginpage.h"
#include "circulargauge.h"
#include "qtstackedwidget.h"
#include "sensorchart.h"

#include <QApplication>
#include <QBoxLayout>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
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
    QPushButton { outline: none; }
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

static const int kMaxFailures     = 3;
static const char *kCmdTmpPath    = "/tmp/edgeguard_cmd.json.ui.tmp";
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
      m_alsGauge(nullptr),
      m_psGauge(nullptr),
      m_motionGauge(nullptr),
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
      m_ipLabel(nullptr),
      m_uptimeLabel(nullptr),
      m_serviceLabel(nullptr),
      m_networkLabel(nullptr),
      m_svcStatusLabel(nullptr),
      m_camOnlineLabel(nullptr),
      m_camMotionLabel(nullptr),
      m_camFacesLabel(nullptr),
      m_camTotalFacesLabel(nullptr),
      m_camSnapshotLabel(nullptr),
      m_camFaceSnapLabel(nullptr),
      m_camInferenceLabel(nullptr),
      m_chart(nullptr),
      m_topBar(nullptr),
      m_logoutBtn(nullptr),
      m_loginPage(nullptr),
      m_bottomBar(nullptr),
      m_authenticated(false)
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
    QVBoxLayout *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ---- top bar (hidden on login) ----
    m_topBar = new QWidget(root);
    m_topBar->setFixedHeight(34);
    m_topBar->setStyleSheet("background: #0a1a2f; border-bottom: 1px solid #1a3050;");
    QHBoxLayout *topLayout = new QHBoxLayout(m_topBar);
    topLayout->setContentsMargins(14, 0, 10, 0);
    topLayout->setSpacing(8);

    QLabel *topBrand = new QLabel("EdgeGuard 6ULL  ·  RickyDuran", m_topBar);
    topBrand->setStyleSheet("font-size: 13px; font-weight: 700; color: #4a7fb5; border: none;");
    topLayout->addWidget(topBrand);
    topLayout->addStretch(1);

    m_logoutBtn = new QPushButton("退出登陆", m_topBar);
    m_logoutBtn->setFocusPolicy(Qt::NoFocus);
    m_logoutBtn->setFixedSize(72, 28);
    m_logoutBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #ff5c5c; border: 1px solid #5a3030; "
        "border-radius: 6px; font-size: 12px; font-weight: 700; padding: 0px 8px; } "
        "QPushButton:pressed { background: #3a2020; }");
    connect(m_logoutBtn, &QPushButton::clicked, this, [this]() {
        m_authenticated = false;
        sendCommand("vision_mode", "monitor");
        if (m_loginPage) m_loginPage->reset();
        switchPage(0, false);
    });
    topLayout->addWidget(m_logoutBtn);

    rootLayout->addWidget(m_topBar);
    m_topBar->hide();  // hidden until authenticated

    // ---- body: sidebar + stack ----
    QHBoxLayout *bodyLayout = new QHBoxLayout();
    bodyLayout->setContentsMargins(12, 10, 12, 12);
    bodyLayout->setSpacing(12);

    m_stack = new QtStackedWidget(root);
    bodyLayout->addWidget(m_stack, 1);
    m_loginPage = new LoginPage(root);
    m_stack->addWidget(m_loginPage);         // 0: Login
    m_stack->addWidget(buildDashboardPage());// 1: Dashboard
    m_stack->addWidget(buildSensorPage());   // 2: Sensors
    m_stack->addWidget(buildAlarmPage());    // 3: Alarms
    m_stack->addWidget(buildSystemPage());   // 4: System
    m_stack->addWidget(buildVisionPage());   // 5: Vision
    m_stack->addWidget(buildChartPage());    // 6: Chart

    rootLayout->addLayout(bodyLayout, 1);

    /* ---- bottom navigation bar (hidden on login) ---- */
    m_bottomBar = buildBottomBar();
    m_bottomBar->hide();
    rootLayout->addWidget(m_bottomBar);

    /* swipe gestures also update bottom bar highlight */
    connect(m_stack, &QtStackedWidget::currentChanged, this, [this](int idx) {
        updateNavStyle(idx - 1);  // page 1=Dashboard=nav[0]
    });

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
    if (pageIndex >= 0 && pageIndex + 2 < m_stack->count())
        switchPage(pageIndex + 2);  // 0→2(Dashboard) etc
}

QWidget *MainWindow::buildBottomBar()
{
    QWidget *bar = new QWidget(this);
    bar->setFixedHeight(42);
    bar->setStyleSheet("background: #0a1a2f; border-top: 1px solid #1a3050;");

    QHBoxLayout *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(6);

    const QStringList names = {"监控中心", "传感器", "报警", "系统", "摄像头", "图表"};
    for (int i = 0; i < names.size(); ++i) {
        QPushButton *btn = new QPushButton(names[i], bar);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        btn->setStyleSheet(
            "QPushButton { background: transparent; color: #9fb5cc; "
            "border: none; border-radius: 6px; font-size: 12px; font-weight: 600; } "
            "QPushButton:pressed { background: #2b80ff; }");
        connect(btn, &QPushButton::clicked, this, [this, i]() { switchPage(i + 1, false); });
        layout->addWidget(btn);
        m_bottomButtons.push_back(btn);
    }
    return bar;
}

QWidget *MainWindow::buildDashboardPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 6, 0, 0);
    layout->setSpacing(8);

    QHBoxLayout *top = new QHBoxLayout();
    top->addWidget(makeTitle("监控中心"));
    top->addStretch(1);
    m_modeBadge = new QLabel("状态", page);
    m_modeBadge->setObjectName("ModeBadge");
    top->addWidget(m_modeBadge);
    layout->addLayout(top);

    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(8);

    /* Row 0: text cards */
    grid->addWidget(makeCard("系统状态", &m_stateLabel), 0, 0);
    grid->addWidget(makeCard("报警原因", &m_alarmReasonLabel), 0, 1);

    /* Row 1: circular gauges */
    auto makeGaugeCard = [&](const QString &title, CircularGauge **g, int max, const QString &unit) {
        QFrame *card = new QFrame(page);
        card->setObjectName("Card");
        card->setMinimumHeight(120);
        QVBoxLayout *cl = new QVBoxLayout(card);
        cl->setContentsMargins(10, 8, 10, 4);
        cl->setSpacing(2);
        QLabel *t = new QLabel(title, card);
        t->setObjectName("CardTitle");
        cl->addWidget(t);
        *g = new CircularGauge(card);
        (*g)->setRange(0, max);
        (*g)->setUnit(unit);
        cl->addWidget(*g, 1);
        return card;
    };

    grid->addWidget(makeGaugeCard("环境光",     &m_alsGauge,    1000, "lx"),  1, 0);
    grid->addWidget(makeGaugeCard("接近感应",   &m_psGauge,      200, ""),    1, 1);
    grid->addWidget(makeGaugeCard("运动增量",   &m_motionGauge, 20000, ""),   1, 2);
    // color zones for ALS: very dim→red, dim→yellow, ok→green
    m_alsGauge->addZone(60, QColor("#4ade80"));
    m_alsGauge->addZone(30, QColor("#ffd166"));
    m_alsGauge->addZone(0,  QColor("#ff5c5c"));
    // PS: near→danger
    m_psGauge->addZone(60, QColor("#4ade80"));
    m_psGauge->addZone(30, QColor("#ffd166"));
    m_psGauge->addZone(0,  QColor("#ff5c5c"));
    // Motion: low→green, high→red
    m_motionGauge->addZone(60, QColor("#4ade80"));
    m_motionGauge->addZone(30, QColor("#ffd166"));
    m_motionGauge->addZone(0,  QColor("#ff5c5c"));

    /* Row 2: LED + time */
    grid->addWidget(makeCard("LED / 蜂鸣器", &m_ledLabel), 2, 0, 1, 3);

    layout->addLayout(grid, 1);

    return page;
}

QWidget *MainWindow::buildSensorPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setSpacing(8);
    layout->addWidget(makeTitle("传感器数据"));

    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(8);
    grid->addWidget(makeCard("MPU6050 加速度", &m_accelLabel), 0, 0);
    grid->addWidget(makeCard("MPU6050 陀螺仪", &m_gyroLabel), 0, 1);
    grid->addWidget(makeCard("AP3216C", &m_ap3216cLabel), 1, 0);
    grid->addWidget(makeCard("温度", &m_rawTempLabel), 1, 1);
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
    layout->setSpacing(8);
    layout->addWidget(makeTitle("报警中心"));

    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(8);
    grid->addWidget(makeCard("当前报警", &m_alarmStateLabel), 0, 0);
    grid->addWidget(makeCard("报警次数", &m_alarmCountLabel), 0, 1);
    grid->addWidget(makeCard("上次报警", &m_lastAlarmLabel), 1, 0, 1, 2);
    layout->addLayout(grid);

    /* Action buttons */
    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(16);

    m_muteBtn = new QPushButton("静音蜂鸣器", page);
    m_muteBtn->setObjectName("ActionButton");
    m_muteBtn->setFocusPolicy(Qt::NoFocus);
    m_muteBtn->setMinimumHeight(42);
    connect(m_muteBtn, &QPushButton::clicked, this, &MainWindow::onMuteClicked);
    btnRow->addWidget(m_muteBtn);

    m_ackBtn = new QPushButton("确认报警", page);
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

QWidget *MainWindow::buildSystemPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setSpacing(8);
    layout->addWidget(makeTitle("系统"));

    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(8);
    grid->addWidget(makeCard("板卡IP", &m_ipLabel), 0, 0);
    grid->addWidget(makeCard("运行时间", &m_uptimeLabel), 0, 1);
    grid->addWidget(makeCard("传感器服务", &m_serviceLabel), 1, 0);
    grid->addWidget(makeCard("网络连接", &m_networkLabel), 1, 1);
    layout->addLayout(grid);

    /* service status card */
    {
        QFrame *card = new QFrame(page);
        card->setObjectName("Card");
        QVBoxLayout *cl = new QVBoxLayout(card);
        cl->setContentsMargins(16, 12, 16, 12);
        cl->setSpacing(6);
        QLabel *t = new QLabel("服务状态", card);
        t->setObjectName("CardTitle");
        cl->addWidget(t);
        m_svcStatusLabel = new QLabel("正在检测...", card);
        m_svcStatusLabel->setObjectName("SmallText");
        m_svcStatusLabel->setWordWrap(true);
        cl->addWidget(m_svcStatusLabel);
        layout->addWidget(card);
    }

    layout->addStretch(1);
    return page;
}

QWidget *MainWindow::buildVisionPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setSpacing(12);
    layout->addWidget(makeTitle("摄像头"));

    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(12);
    grid->addWidget(makeCard("摄像头",      &m_camOnlineLabel,    "OFFLINE"), 0, 0);
    grid->addWidget(makeCard("运动检测",      &m_camMotionLabel,    "--"),      0, 1);
    grid->addWidget(makeCard("人脸数量",  &m_camFacesLabel,     "0"),       1, 0);
    grid->addWidget(makeCard("推理耗时",   &m_camInferenceLabel, "--"),      1, 1);
    grid->addWidget(makeCard("累计人脸",  &m_camTotalFacesLabel, "0"),       2, 0);
    {
        QWidget *snapCard = makeCard("人脸快照", &m_camFaceSnapLabel, "--");
        grid->addWidget(snapCard, 2, 1);
    }
    {
        QWidget *snapCard = makeCard("最新快照", &m_camSnapshotLabel, "--");
        grid->addWidget(snapCard, 3, 0, 1, 2);
    }
    layout->addLayout(grid, 1);

    /* Snapshot preview hint */
    m_camSnapshotLabel->setWordWrap(true);
    m_camSnapshotLabel->setStyleSheet("color: #8fb3d9; font-size: 13px;");

    return page;
}

QWidget *MainWindow::buildChartPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    m_chart = new SensorChart(page);
    m_chart->setTitle("传感器实时波形");
    m_chart->setLabels("时间 (每个点 500ms)", "数值");
    m_chart->setRange(0, 1000);
    m_chart->setVisiblePoints(60);  // 30 seconds visible
    m_chartMode = 0;  // default ALS

    layout->addWidget(m_chart, 1);

    // Sensor selector row
    QHBoxLayout *selRow = new QHBoxLayout();
    selRow->setSpacing(8);
    selRow->setContentsMargins(12, 0, 12, 6);

    auto makeSelBtn = [page, this](const QString &label, int mode, double min, double max) {
        QPushButton *btn = new QPushButton(label, page);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setFixedHeight(26);
        btn->setStyleSheet(
            "QPushButton { background: #101d2f; color: #8fb3d9; border: 1px solid #263b58; "
            "border-radius: 6px; font-size: 11px; padding: 2px 10px; } "
            "QPushButton:checked { background: #1f6feb; color: #ffffff; } "
            "QPushButton:pressed { background: #2b80ff; }");
        btn->setCheckable(true);
        btn->setAutoExclusive(true);
        connect(btn, &QPushButton::clicked, this, [this, mode, min, max]() {
            m_chartMode = mode;
            if (m_chart) {
                m_chart->clearData();
                m_chart->setRange(min, max);
            }
        });
        return btn;
    };

    QPushButton *btnALS    = makeSelBtn("环境光",   0, 0,    1000);
    QPushButton *btnPS     = makeSelBtn("接近感应", 1, 0,    200);
    QPushButton *btnMotion = makeSelBtn("运动增量", 2, 0,    20000);
    QPushButton *btnTemp   = makeSelBtn("温度",     3, -10, 60);
    btnALS->setChecked(true);

    selRow->addWidget(btnALS);
    selRow->addWidget(btnPS);
    selRow->addWidget(btnMotion);
    selRow->addWidget(btnTemp);
    selRow->addStretch(1);

    layout->addLayout(selRow);
    return page;
}

QWidget *MainWindow::makeCard(const QString &title, QLabel **valueLabel, const QString &initial)
{
    QFrame *card = new QFrame(this);
    card->setObjectName("Card");
    card->setMinimumHeight(80);

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

void MainWindow::switchPage(int index, bool animated)
{
    if (!m_stack || index < 0 || index >= m_stack->count())
        return;
    // auth gate: page 0 (Login) is pre-auth
    if (!m_authenticated && index > 0 && !m_demoMode)
        return;
    // swipe guard: block navigating back to login page once authenticated
    if (m_authenticated && index <= 0)
        return;
    if (m_stack->currentIndex() == index) return;

    // top bar: hidden on login, shown on authenticated pages
    if (m_topBar)
        m_topBar->setVisible(index > 0);

    // bottom bar: hidden on login, shown on authenticated pages
    if (m_bottomBar)
        m_bottomBar->setVisible(index > 0);

    // Login page: disable swipe; authenticated pages: enable swipe
    m_stack->setPressMove(index > 0);

    // transitions from pre-auth page (0) are instant;
    // swipe-driven transitions on authenticated pages are animated
    if (!animated || m_stack->currentIndex() <= 0)
        m_stack->setCurrentIndexNoAnim(index);
    else
        m_stack->setCurrentIndex(index);

    // highlight button after page actually changes (nav[0]=Dashboard=page2)
    updateNavStyle(index - 1);
}

void MainWindow::updateNavStyle(int visual)
{
    for (int i = 0; i < m_bottomButtons.size(); ++i) {
        if (i == visual) {
            m_bottomButtons[i]->setStyleSheet(
                "QPushButton { background: #1f6feb; color: #ffffff; "
                "border: none; border-radius: 6px; font-size: 12px; font-weight: 600; } "
                "QPushButton:pressed { background: #2b80ff; }");
        } else {
            m_bottomButtons[i]->setStyleSheet(
                "QPushButton { background: transparent; color: #9fb5cc; "
                "border: none; border-radius: 6px; font-size: 12px; font-weight: 600; } "
                "QPushButton:pressed { background: #2b80ff; }");
        }
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_1: switchPage(1); break;  // Dashboard
    case Qt::Key_2: switchPage(2); break;  // Sensors
    case Qt::Key_3: switchPage(3); break;  // Alarms
    case Qt::Key_4: switchPage(4); break;  // System
    case Qt::Key_5: switchPage(5); break;  // Vision
    case Qt::Key_6: switchPage(6); break;  // Chart
    case Qt::Key_Escape: close(); break;
    default: QMainWindow::keyPressEvent(event); break;
    }
}

/* ---- Login slots ---- */
void MainWindow::onLoginSuccess()
{
    m_authenticated = true;
    sendCommand("vision_mode", "monitor");
    switchPage(1);  // Dashboard
}

void MainWindow::onDemoRequested()
{
    m_demoMode = true;
    m_authenticated = true;
    m_demoTimer.start();
    sendCommand("vision_mode", "monitor");
    switchPage(1);  // Dashboard
}

/* ---- Command channel ---- */

void MainWindow::sendCommand(const QString &cmd, const QString &mode)
{
    QFile file(kCmdTmpPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    QJsonObject obj;
    obj["cmd"] = cmd;
    if (!mode.isEmpty())
        obj["mode"] = mode;
    QJsonDocument doc(obj);
    file.write(doc.toJson(QJsonDocument::Compact));
    file.close();

    ::rename(kCmdTmpPath, kCmdPath);
    qDebug("Command sent: %s mode=%s", qPrintable(cmd),
           mode.isEmpty() ? "(none)" : qPrintable(mode));
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
    alarm["last"] = phase > 18
        ? QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")
        : "无";
    alarm["muted"] = false;
    alarm["acknowledged"] = false;

    QJsonObject sys;
    sys["uptime_sec"] = (double)(m_demoTimer.elapsed() / 1000);
    sys["ip"] = "192.168.10.2";
    sys["sensor_hubd"] = "running";

    obj["state"] = state;
    obj["alarm_reason"] = reason;
    obj["timestamp_ms"] = (double)(m_demoCounter * 500);
    /* Vision demo data */
    QJsonObject vision;
    vision["mode"] = "tamper";
    vision["camera_online"] = true;
    vision["motion_detected"] = (phase > 26);
    vision["face_count"] = (phase > 27) ? 1 : 0;
    vision["total_face_count"] = (phase > 27) ? (3 + m_demoCounter / 30) : 0;
    vision["tamper_detected"] = (phase > 28);
    vision["preview_path"] = "null";
    vision["inference_ms"] = (phase > 27) ? 350 + (m_demoCounter % 200) : 8;
    vision["snapshot_path"] = (phase > 26) ? "/var/log/edgeguard/snapshots/demo_snap.jpg" : "null";
    vision["last_face_snapshot"] = (phase > 27) ? "/var/log/edgeguard/snapshots/face_demo.jpg" : "";

    obj["mpu6050"] = mpu;
    obj["ap3216c"] = ap3216;
    obj["vision"] = vision;
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
        m_modeBadge->setText("服务断开");
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
    m_modeBadge->setText(demo ? "演示" : "运行中");
    m_modeBadge->setStyleSheet("");  /* reset to default */

    /* translate state / reason to Chinese */
    auto trState = [](const QString &s) -> QString {
        if (s.contains("FAULT"))   return "故障";
        if (s.contains("ALARM"))   return "报警";
        if (s.contains("WARNING")) return "警告";
        if (s == "NORMAL" || s == "normal") return "正常";
        return s;
    };
    auto trReason = [](const QString &r) -> QString {
        if (r == "none" || r.isEmpty())    return "无";
        if (r.contains("motion"))          return "运动量超阈值";
        if (r.contains("ambient") || r.contains("light")) return "环境光过低";
        if (r.contains("proximity") || r.contains("ps")) return "接近感应触发";
        if (r.contains("camera") || r.contains("tamper")) return "摄像头被遮挡";
        if (r.contains("temperature") || r.contains("temp")) return "温度异常";
        return r;
    };
    QString cnState  = trState(state);
    QString cnReason = trReason(reason);

    m_stateLabel->setText(cnState);
    m_alarmReasonLabel->setText(cnReason);
    if (m_alsGauge)    m_alsGauge->setValue(valueToInt(ap3216, "als"));
    if (m_psGauge)     m_psGauge->setValue(valueToInt(ap3216, "ps"));
    if (m_motionGauge) m_motionGauge->setValue(valueToInt(mpu, "motion_delta"));
    {
        QString led = valueToString(device, "led", "--");
        QString buz = valueToString(device, "buzzer", "--");
        if (led == "green") led = "绿灯"; else if (led == "yellow") led = "黄灯";
        else if (led == "red") led = "红灯";
        if (buz == "off") buz = "关闭"; else if (buz == "beep") buz = "蜂鸣";
        m_ledLabel->setText(led + " / " + buz);
    }
    if (m_timeLabel) {
        qint64 ts_ms = obj.value("timestamp_ms").toVariant().toLongLong();
        if (ts_ms > 0) {
            qint64 secs = ts_ms / 1000;
            m_timeLabel->setText(QString("数据延迟: %1 秒前")
                                 .arg(QDateTime::currentSecsSinceEpoch() - secs));
        } else {
            m_timeLabel->setText("最后更新: " +
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
                              .arg(mpu.value("online").toBool() ? "在线" : "离线"));
    m_apOnlineLabel->setText(QString("AP3216C: %1")
                              .arg(ap3216.value("online").toBool() ? "在线" : "离线"));

    /* ---- Alarms ---- */
    m_alarmStateLabel->setText(cnState + "\n" + cnReason);
    m_alarmCountLabel->setText(valueToString(alarm, "count", "0"));
    m_lastAlarmLabel->setText(valueToString(alarm, "last", "无"));

    /* ---- Chart data push ---- */

    /* ---- Chart data push ---- */
    if (m_chart) {
        double v = 0;
        switch (m_chartMode) {
        case 0: v = valueToInt(ap3216, "als"); break;
        case 1: v = valueToInt(ap3216, "ps");  break;
        case 2: v = valueToInt(mpu, "motion_delta"); break;
        case 3: v = mpu.value("temp").toDouble(); break;
        }
        m_chart->pushData(v);
    }

    /* ---- System ---- */
    m_ipLabel->setText(valueToString(sys, "ip", "--"));
    {
        QJsonValue uv = sys.value("uptime_sec");
        int secs = 0;
        if (uv.isDouble()) secs = (int)uv.toDouble();
        int days  = secs / 86400;
        int hours = (secs % 86400) / 3600;
        int mins  = (secs % 3600) / 60;
        int s     = secs % 60;
        QString up;
        if (days > 0)  up += QString("%1天").arg(days);
        if (hours > 0) up += QString("%1小时").arg(hours);
        if (mins > 0)  up += QString("%1分").arg(mins);
        if (s > 0 || up.isEmpty()) up += QString("%1秒").arg(s);
        m_uptimeLabel->setText(up.isEmpty() ? valueToString(sys, "uptime_sec", "--") : up);
    }
    {
        QString sv = valueToString(sys, "sensor_hubd", "unknown");
        if (sv == "running") sv = "运行中";
        else if (sv == "stopped" || sv == "unknown") sv = "未运行";
        m_serviceLabel->setText(sv);
    }
    {
        /* detect active eth interface (try eth2 first, then eth0, eth1) */
        static const char *ifaces[] = {"eth2", "eth0", "eth1"};
        QString netText;
        for (int i = 0; i < 3; i++) {
            QString path = QString("/sys/class/net/%1/operstate").arg(ifaces[i]);
            QFile nf(path);
            if (nf.open(QIODevice::ReadOnly)) {
                netText = QString("%1 %2").arg(ifaces[i],
                    QString::fromUtf8(nf.readAll()).trimmed());
                nf.close();
                break;
            }
        }
        m_networkLabel->setText(netText.isEmpty() ? "无连接" : netText);
    }

    /* ---- Service status (throttled to once per 5 s) ---- */
    if (m_svcStatusLabel) {
        static int svcTick = 0;
        static QString svcCached;
        if (++svcTick >= 10) {
            svcTick = 0;
            /* check /proc for running daemons */
            auto procAlive = [](const char *name) -> bool {
                QDir proc("/proc");
                QStringList ents = proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                for (const QString &e : ents) {
                    bool ok; e.toInt(&ok);
                    if (!ok) continue;
                    QFile f("/proc/" + e + "/comm");
                    if (f.open(QIODevice::ReadOnly)) {
                        QString pn = QString::fromUtf8(f.readAll()).trimmed();
                        if (pn == QLatin1String(name)) return true;
                    }
                }
                return false;
            };
            bool ui  = true;  // this process IS the UI
            bool web = procAlive("edgeguard_httpd");
            bool cam = procAlive("edgeguard_visio");   /* truncated from edgeguard_visiond */
            bool mq  = procAlive("edgeguard_mqttd");
            svcCached = QString("UI界面: ●  |  Web: %1  |  摄像头: %2  |  MQTT: %3")
                .arg(web ? "●" : "○")
                .arg(cam ? "●" : "○")
                .arg(mq  ? "●" : "○");
        }
        m_svcStatusLabel->setText(svcCached.isEmpty()
            ? "UI界面: ●  |  Web: --  |  摄像头: --  |  MQTT: --" : svcCached);
    }

    /* ---- Vision ---- */
    {
        QJsonObject vision = obj.value("vision").toObject();
        if (m_camOnlineLabel) {
            bool online = vision.value("camera_online").toBool();
            bool tamper = vision.value("tamper_detected").toBool();
            if (tamper)
                m_camOnlineLabel->setText("被遮挡");
            else
                m_camOnlineLabel->setText(online ? "在线" : "离线");
            m_camOnlineLabel->setStyleSheet(
                tamper ? "color: #ff5c5c; font-size: 26px; font-weight: 900;"
                       : online ? "color: #4ade80; font-size: 26px; font-weight: 700;"
                                : "color: #ff5c5c; font-size: 26px; font-weight: 700;");
        }
        if (m_camMotionLabel)
            m_camMotionLabel->setText(vision.value("motion_detected").toBool()
                                       ? "是" : "否");
        if (m_camFacesLabel)
            m_camFacesLabel->setText(QString::number(vision.value("face_count").toInt()));
        if (m_camTotalFacesLabel)
            m_camTotalFacesLabel->setText(QString::number(vision.value("total_face_count").toInt()));
        if (m_camInferenceLabel) {
            int ms = vision.value("inference_ms").toInt(0);
            m_camInferenceLabel->setText(ms > 0 ? QString("%1 ms").arg(ms) : "--");
        }
        if (m_camFaceSnapLabel) {
            QString fs = vision.value("last_face_snapshot").toString();
            m_camFaceSnapLabel->setText(fs.isEmpty() ? "--" : fs);
            m_camFaceSnapLabel->setWordWrap(true);
            m_camFaceSnapLabel->setStyleSheet("color: #8fb3d9; font-size: 11px;");
        }
        if (m_camSnapshotLabel) {
            QString sp = vision.value("snapshot_path").toString();
            m_camSnapshotLabel->setText(sp.isEmpty() || sp == "null" ? "--" : sp);
        }
    }

    /* ---- Color-coding ---- */
    if (state.contains("FAULT") || state.contains("fault")) {
        m_stateLabel->setStyleSheet("color: #ff5c5c; font-size: 26px; font-weight: 900;");
    } else if (state.contains("ALARM") || state.contains("alarm")) {
        m_stateLabel->setStyleSheet("color: #ff5c5c; font-size: 26px; font-weight: 900;");
    } else if (state.contains("WARNING") || state.contains("warning")) {
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
        /* staleness check: if status.json hasn't been updated in 3+ seconds,
           sensor_hubd may have crashed or be stuck — show service lost */
        QFileInfo fi(m_statusPath);
        if (!fi.exists() || fi.lastModified().secsTo(QDateTime::currentDateTime()) > 3) {
            applyServiceLost();
            return;
        }
        if (!loadStatusFromFile(&obj)) {
            applyServiceLost();
            return;
        }
    } else {
        obj = makeDemoStatus();
    }

    m_consecutiveFailures = 0;  /* reset on successful read */
    applyStatus(obj, demo);
}
