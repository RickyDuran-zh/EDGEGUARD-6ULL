#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QJsonObject>
#include <QVector>
#include <QPoint>
#include <QMouseEvent>

class LoginPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void refreshStatus();
    void switchPage(int index);
    void onMuteClicked();
    void onAckClicked();
    void onLoginSuccess();
    void onDemoRequested();

private:
    void parseArguments();
    void buildUi();
    QWidget *buildSidebar();
    QWidget *buildDashboardPage();
    QWidget *buildSensorPage();
    QWidget *buildAlarmPage();
    QWidget *buildSettingsPage();
    QWidget *buildSystemPage();
    QWidget *buildVisionPage();
    QWidget *makeCard(const QString &title, QLabel **valueLabel, const QString &initial = "--");
    QLabel *makeTitle(const QString &text);
    QLabel *makeSmallText(const QString &text);
    bool loadStatusFromFile(QJsonObject *obj);
    QJsonObject makeDemoStatus();
    void applyStatus(const QJsonObject &obj, bool demo);
    void applyServiceLost();
    void updateNavStyle();
    void sendCommand(const QString &cmd);
    QString valueToString(const QJsonObject &obj, const QString &key, const QString &fallback = "--") const;
    int valueToInt(const QJsonObject &obj, const QString &key, int fallback = 0) const;

private:
    QStackedWidget *m_stack;
    QVector<QPushButton *> m_navButtons;
    QTimer *m_timer;
    QString m_statusPath;
    bool m_demoMode;
    int m_demoCounter;
    int m_consecutiveFailures;

    QLabel *m_modeBadge;
    QLabel *m_stateLabel;
    QLabel *m_alarmReasonLabel;
    QLabel *m_alsLabel;
    QLabel *m_psLabel;
    QLabel *m_motionLabel;
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

    QLabel *m_intervalLabel;
    QLabel *m_thresholdLabel;

    QLabel *m_ipLabel;
    QLabel *m_uptimeLabel;
    QLabel *m_serviceLabel;
    QLabel *m_networkLabel;

    // Vision
    QLabel *m_camOnlineLabel;
    QLabel *m_camMotionLabel;
    QLabel *m_camFacesLabel;
    QLabel *m_camSnapshotLabel;
    QLabel *m_camInferenceLabel;

    // Top bar (logout)
    QWidget *m_topBar;
    QPushButton *m_logoutBtn;

    // Login
    LoginPage *m_loginPage;
    QWidget *m_sidebar;
    bool m_authenticated;

    // Swipe (Qt mouse events)
    QPoint m_pressPos;
    bool   m_pressing;
    bool   m_swiped;
    static const int kSwipeThresh = 70;
};

#endif // MAINWINDOW_H
