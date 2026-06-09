#include "loginpage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QApplication>
#include <QDebug>

static QString gUser = "rickyduran";
static QString gPass = "123456";

static void loadUsersFromFile()
{
    QFile f("/etc/edgeguard/users.json");
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;
    QJsonArray users = doc.object().value("users").toArray();
    if (users.isEmpty()) return;
    QJsonObject u = users.first().toObject();
    QString name = u.value("name").toString();
    QString pass = u.value("pass").toString();
    if (!name.isEmpty() && !pass.isEmpty()) { gUser = name; gPass = pass; }
}

LoginPage::LoginPage(QWidget *parent)
    : QWidget(parent),
      m_userEdit(nullptr), m_passEdit(nullptr), m_activeField(nullptr),
      m_loginBtn(nullptr), m_errorLabel(nullptr), m_lockLabel(nullptr),
      m_lockTimer(new QTimer(this)),
      m_shifted(false), m_attempts(0), m_authenticated(false), m_locked(false)
{
    loadUsersFromFile();
    buildUi();
    m_lockTimer->setSingleShot(true);
    connect(m_lockTimer, &QTimer::timeout, this, &LoginPage::onUnlockTimer);
}

void LoginPage::reset()
{
    m_attempts = 0;
    m_authenticated = false;
    m_locked = false;
    m_shifted = false;
    setShift(false);
    m_userEdit->clear();
    m_passEdit->clear();
    m_errorLabel->clear();
    m_lockLabel->hide();
    m_loginBtn->setEnabled(true);
    m_lockTimer->stop();
    m_activeField = m_userEdit;
    highlightActiveField();
}

bool LoginPage::checkCredentials(const QString &user, const QString &pass)
{
    return (user == gUser && pass == gPass);
}

void LoginPage::setShift(bool on)
{
    m_shifted = on;
    for (QPushButton *b : m_letterKeys) {
        QString t = b->text();
        if (t.size() == 1)
            b->setText(on ? t.toUpper() : t.toLower());
    }
}

void LoginPage::highlightActiveField()
{
    const QString onStyle = R"(
        QLineEdit { background:#101d2f; border:2px solid #1f6feb; border-radius:6px;
                    color:#ffffff; font-size:15px; padding:4px 10px; }
    )";
    const QString offStyle = R"(
        QLineEdit { background:#101d2f; border:1px solid #263b58; border-radius:6px;
                    color:#ffffff; font-size:15px; padding:4px 10px; }
    )";
    if (m_userEdit)
        m_userEdit->setStyleSheet(m_activeField == m_userEdit ? onStyle : offStyle);
    if (m_passEdit)
        m_passEdit->setStyleSheet(m_activeField == m_passEdit ? onStyle : offStyle);
}

/* ---- slots ---- */

void LoginPage::onKeyClicked()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    QString key = btn->text();
    QLineEdit *target = m_activeField ? m_activeField : m_userEdit;

    if (key == "⌫" || key == "Bksp") {
        target->backspace();
    } else if (key == "⇧" || key == "Shift") {
        setShift(!m_shifted);
    } else if (key == "⇩") {
        // toggle active field + move cursor
        m_activeField = (m_activeField == m_userEdit) ? m_passEdit : m_userEdit;
        highlightActiveField();
        if (m_activeField) m_activeField->setFocus();
    } else if (key == "⏎" || key == "Done" || key == "Enter") {
        if (m_activeField == m_userEdit) {
            m_activeField = m_passEdit;
            highlightActiveField();
            if (m_passEdit) m_passEdit->setFocus();
        } else {
            onLoginClicked();
        }
    } else if (key == ".com") {
        target->insert(".com");
    } else if (key == "Space") {
        target->insert(" ");
    } else if (key.size() == 1) {
        target->insert(key);
    }
}

void LoginPage::onLoginClicked()
{
    if (m_locked) return;
    if (checkCredentials(m_userEdit->text(), m_passEdit->text())) {
        m_authenticated = true;
        m_errorLabel->clear();
        emit loginSuccess();
    } else {
        m_attempts++;
        m_errorLabel->setText(QString("Login failed (%1/%2)").arg(m_attempts).arg(kMaxAttempts));
        m_passEdit->clear();
        if (m_attempts >= kMaxAttempts) {
            m_locked = true;
            m_loginBtn->setEnabled(false);
            m_lockLabel->setText(QString("Locked %1s...").arg(kLockSeconds));
            m_lockLabel->show();
            m_lockTimer->start(kLockSeconds * 1000);
        }
    }
}

void LoginPage::onDemoClicked() { emit demoRequested(); }
void LoginPage::onUnlockTimer()
{
    m_locked = false; m_attempts = 0;
    m_loginBtn->setEnabled(true); m_lockLabel->hide(); m_errorLabel->clear();
}

/* ---- UI ---- */

void LoginPage::buildUi()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(40, 8, 40, 6);
    root->setSpacing(2);

    QLabel *title = new QLabel("EdgeGuard 6ULL", this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size:22px; font-weight:800; color:#ffffff;");
    root->addWidget(title);
    root->addSpacing(4);

    // Username
    root->addWidget(new QLabel("Username", this));
    m_userEdit = new QLineEdit(this);
    m_userEdit->setPlaceholderText("rickyduran");
    m_userEdit->setFixedHeight(32);
    root->addWidget(m_userEdit);
    root->addSpacing(2);

    // Password
    root->addWidget(new QLabel("Password", this));
    m_passEdit = new QLineEdit(this);
    m_passEdit->setEchoMode(QLineEdit::Password);
    m_passEdit->setPlaceholderText("••••");
    m_passEdit->setFixedHeight(32);
    root->addWidget(m_passEdit);
    root->addSpacing(2);

    // Error / lock
    m_errorLabel = new QLabel(this);
    m_errorLabel->setStyleSheet("font-size:11px; color:#ff5c5c;");
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->setFixedHeight(16);
    root->addWidget(m_errorLabel);
    m_lockLabel = new QLabel(this);
    m_lockLabel->setStyleSheet("font-size:11px; color:#ffd166;");
    m_lockLabel->setAlignment(Qt::AlignCenter);
    m_lockLabel->setFixedHeight(16);
    m_lockLabel->hide();
    root->addWidget(m_lockLabel);

    // ---- Keyboard ----
    const int KH = 28;
    QGridLayout *kb = new QGridLayout();
    kb->setSpacing(2);

    const QString ks = R"(QPushButton{background:#1a2d45;border:1px solid #263b58;border-radius:4px;color:#e6eef8;font-size:13px;font-weight:600;}QPushButton:pressed{background:#1f6feb;})";
    const QString ms = R"(QPushButton{background:#243b58;border:1px solid #263b58;border-radius:4px;color:#cde7ff;font-size:13px;font-weight:700;}QPushButton:pressed{background:#1f6feb;})";

    auto add = [&](int r, int c, int cs, const QString &t, const QString &s, bool letter) {
        QPushButton *b = new QPushButton(t, this);
        b->setFixedHeight(KH);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        b->setFocusPolicy(Qt::NoFocus);
        b->setStyleSheet(s);
        connect(b, &QPushButton::clicked, this, &LoginPage::onKeyClicked);
        kb->addWidget(b, r, c, 1, cs);
        if (letter) m_letterKeys.append(b);
        return b;
    };

    const char *row0[] = {"1","2","3","4","5","6","7","8","9","0"};
    for (int c=0;c<10;c++) add(0,c,1,row0[c],ks,false);
    const char *row1[] = {"q","w","e","r","t","y","u","i","o","p"};
    for (int c=0;c<10;c++) add(1,c,1,row1[c],ks,true);
    const char *row2[] = {"a","s","d","f","g","h","j","k","l","⌫"};
    for (int c=0;c<10;c++) add(2,c,1,row2[c],ks,c<9);
    add(3,0,1,"⇧",ms,false);
    const char *row3[] = {"z","x","c","v","b","n","m",".","-"};
    for (int c=0;c<9;c++) add(3,c+1,1,row3[c],ks,c<7);
    add(4,0,1,"⇩",ms,false);  // toggle field
    add(4,1,1,"@",ms,false);
    add(4,2,4,"Space",ms,false);
    add(4,6,1,"_",ms,false);
    add(4,7,1,"/",ms,false);
    add(4,8,1,".com",ms,false);

    QPushButton *done = new QPushButton("⏎", this);
    done->setFixedHeight(KH);
    done->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    done->setFocusPolicy(Qt::NoFocus);
    done->setStyleSheet(R"(QPushButton{background:#1f6feb;border:none;border-radius:4px;color:#fff;font-size:16px;font-weight:700;}QPushButton:pressed{background:#2b80ff;})");
    connect(done, &QPushButton::clicked, this, &LoginPage::onKeyClicked);
    kb->addWidget(done, 4, 9);

    root->addLayout(kb);
    root->addSpacing(4);

    // Buttons row
    QHBoxLayout *btnRow = new QHBoxLayout(); btnRow->setSpacing(10);
    m_loginBtn = new QPushButton("Sign In", this);
    m_loginBtn->setFixedHeight(36);
    m_loginBtn->setStyleSheet(R"(QPushButton{background:#1f6feb;border:none;border-radius:8px;color:#fff;font-size:16px;font-weight:700;}QPushButton:pressed{background:#2b80ff;}QPushButton:disabled{background:#243b58;color:#6a7d94;})");
    connect(m_loginBtn, &QPushButton::clicked, this, &LoginPage::onLoginClicked);
    btnRow->addWidget(m_loginBtn, 3);

    QPushButton *demoBtn = new QPushButton("Demo", this);
    demoBtn->setFixedHeight(36);
    demoBtn->setFocusPolicy(Qt::NoFocus);
    demoBtn->setStyleSheet(R"(QPushButton{background:transparent;border:1px solid #263b58;border-radius:8px;color:#8fb3d9;font-size:13px;font-weight:600;}QPushButton:pressed{background:#1a2d45;})");
    connect(demoBtn, &QPushButton::clicked, this, &LoginPage::onDemoClicked);
    btnRow->addWidget(demoBtn, 1);
    root->addLayout(btnRow);

    m_activeField = m_userEdit;
    highlightActiveField();
}
