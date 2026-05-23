#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QJsonObject>
#include <QVector>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void refreshStatus();
    void switchPage(int index);

private:
    void parseArguments();
    void buildUi();
    QWidget *buildSidebar();
    QWidget *buildDashboardPage();
    QWidget *buildSensorPage();
    QWidget *buildAlarmPage();
    QWidget *buildSettingsPage();
    QWidget *buildNetworkPage();
    QWidget *makeCard(const QString &title, QLabel **valueLabel, const QString &initial = "--");
    QLabel *makeTitle(const QString &text);
    QLabel *makeSmallText(const QString &text);
    bool loadStatusFromFile(QJsonObject *obj);
    QJsonObject makeDemoStatus();
    void applyStatus(const QJsonObject &obj, bool demo);
    void updateNavStyle();
    QString valueToString(const QJsonObject &obj, const QString &key, const QString &fallback = "--") const;
    int valueToInt(const QJsonObject &obj, const QString &key, int fallback = 0) const;

private:
    QStackedWidget *m_stack;
    QVector<QPushButton *> m_navButtons;
    QTimer *m_timer;
    QString m_statusPath;
    bool m_demoMode;
    int m_demoCounter;

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

    QLabel *m_alarmStateLabel;
    QLabel *m_alarmCountLabel;
    QLabel *m_lastAlarmLabel;

    QLabel *m_intervalLabel;
    QLabel *m_thresholdLabel;

    QLabel *m_ipLabel;
    QLabel *m_uploadLabel;
    QLabel *m_networkLabel;
};

#endif // MAINWINDOW_H
