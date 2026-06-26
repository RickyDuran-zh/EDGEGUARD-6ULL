#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QElapsedTimer>
#include <QTimer>
#include <QJsonObject>
#include <QVector>

class LoginPage;
class FaceLoginPage;
class CircularGauge;
class QtStackedWidget;
class SensorChart;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void refreshStatus();
    void switchPage(int index, bool animated = true);
    void onMuteClicked();
    void onAckClicked();
    void onLoginSuccess();
    void onDemoRequested();
    void onFaceLoginRequested();
    void onFaceLoginCancel();

private:
    void parseArguments();
    void buildUi();
    QWidget *buildBottomBar();
    QWidget *buildDashboardPage();
    QWidget *buildSensorPage();
    QWidget *buildAlarmPage();
    QWidget *buildSystemPage();
    QWidget *buildVisionPage();
    QWidget *buildChartPage();
    QWidget *makeCard(const QString &title, QLabel **valueLabel, const QString &initial = "--");
    QLabel *makeTitle(const QString &text);
    QLabel *makeSmallText(const QString &text);
    bool loadStatusFromFile(QJsonObject *obj);
    QJsonObject makeDemoStatus();
    void applyStatus(const QJsonObject &obj, bool demo);
    void applyServiceLost();
    void updateNavStyle(int visual);
    void sendCommand(const QString &cmd, const QString &mode = QString());
    QString valueToString(const QJsonObject &obj, const QString &key, const QString &fallback = "--") const;
    int valueToInt(const QJsonObject &obj, const QString &key, int fallback = 0) const;

private:
    QtStackedWidget *m_stack;
    QVector<QPushButton *> m_bottomButtons;
    QTimer *m_timer;
    QElapsedTimer m_demoTimer;
    QString m_statusPath;
    bool m_demoMode;
    int m_demoCounter;
    int m_consecutiveFailures;

    QLabel *m_modeBadge;
    QLabel *m_stateLabel;
    QLabel *m_alarmReasonLabel;
    CircularGauge *m_alsGauge;
    CircularGauge *m_psGauge;
    CircularGauge *m_motionGauge;
    QLabel *m_ledLabel;
    QLabel *m_buzzerLabel;
    QLabel *m_timeLabel;

    QLabel *m_accelLabel;
    QLabel *m_gyroLabel;
    QLabel *m_ap3216cLabel;
    QLabel *m_rawTempLabel;
    QLabel *m_mpuOnlineLabel;
    QLabel *m_apOnlineLabel;

    QLabel *m_alarmStateLabel;
    QLabel *m_alarmCountLabel;
    QLabel *m_lastAlarmLabel;
    QPushButton *m_muteBtn;
    QPushButton *m_ackBtn;

    QLabel *m_ipLabel;
    QLabel *m_uptimeLabel;
    QLabel *m_serviceLabel;
    QLabel *m_networkLabel;
    QLabel *m_svcStatusLabel;

    // Vision
    QLabel *m_camOnlineLabel;
    QLabel *m_camMotionLabel;
    QLabel *m_camFacesLabel;
    QLabel *m_camTotalFacesLabel;
    QLabel *m_camSnapshotLabel;
    QLabel *m_camFaceSnapLabel;
    QLabel *m_camInferenceLabel;

    // Chart
    SensorChart *m_chart;
    int m_chartMode;  // 0=ALS, 1=PS, 2=Motion, 3=Temp

    // Top bar (logout)
    QWidget *m_topBar;
    QPushButton *m_logoutBtn;

    // Login
    LoginPage *m_loginPage;
    FaceLoginPage *m_faceLoginPage;
    QWidget *m_bottomBar;
    bool m_authenticated;
};

#endif // MAINWINDOW_H
