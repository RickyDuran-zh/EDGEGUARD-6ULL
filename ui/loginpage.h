#ifndef LOGINPAGE_H
#define LOGINPAGE_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QGridLayout>
#include <QVector>

class LoginPage : public QWidget
{
    Q_OBJECT

public:
    explicit LoginPage(QWidget *parent = nullptr);
    bool isAuthenticated() const { return m_authenticated; }
    void reset();

signals:
    void loginSuccess();
    void demoRequested();

private slots:
    void onKeyClicked();
    void onLoginClicked();
    void onDemoClicked();
    void onUnlockTimer();

private:
    void buildUi();
    void setShift(bool on);
    void highlightActiveField();
    bool checkCredentials(const QString &user, const QString &pass);

    QLineEdit *m_userEdit;
    QLineEdit *m_passEdit;
    QLineEdit *m_activeField;
    QPushButton *m_loginBtn;
    QLabel *m_errorLabel;
    QLabel *m_lockLabel;
    QTimer *m_lockTimer;
    QVector<QPushButton*> m_letterKeys;
    bool m_shifted;
    int m_attempts;
    bool m_authenticated;
    bool m_locked;

    static const int kMaxAttempts = 3;
    static const int kLockSeconds = 30;
};

#endif // LOGINPAGE_H
