// faceloginpage.cpp — Face login page implementation
// Displays live camera preview via shared JPEG file from visiond

#include "faceloginpage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QFile>
#include <QDebug>

static const char *kFaceStyle = R"(
    QWidget#FaceLoginRoot { background: #07111f; }
    QLabel#FaceTitle {
        color: #ffffff;
        font-size: 22px;
        font-weight: 800;
    }
    QLabel#PreviewBox {
        background: #101d2f;
        border: 2px solid #263b58;
        border-radius: 12px;
        min-width: 320px;
        min-height: 240px;
    }
    QLabel#StatusHint {
        color: #ffd166;
        font-size: 14px;
        font-weight: 600;
    }
    QLabel#TimeoutHint {
        color: #8fb3d9;
        font-size: 12px;
    }
    QPushButton#CancelBtn {
        background: transparent;
        color: #ff5c5c;
        border: 1px solid #5a3030;
        border-radius: 8px;
        padding: 10px 24px;
        font-size: 14px;
        font-weight: 700;
    }
    QPushButton#CancelBtn:pressed {
        background: #3a2020;
    }
)";

FaceLoginPage::FaceLoginPage(QWidget *parent)
    : QWidget(parent),
      m_previewLabel(nullptr),
      m_titleLabel(nullptr),
      m_statusLabel(nullptr),
      m_timeoutLabel(nullptr),
      m_cancelBtn(nullptr),
      m_previewTimer(new QTimer(this)),
      m_timeoutTimer(new QTimer(this)),
      m_secondsLeft(kTimeoutSeconds)
{
    setStyleSheet(kFaceStyle);
    buildUi();

    connect(m_previewTimer, &QTimer::timeout, this, &FaceLoginPage::refreshPreview);
    connect(m_timeoutTimer, &QTimer::timeout, this, &FaceLoginPage::onTimeoutTick);
    connect(m_cancelBtn, &QPushButton::clicked, this, &FaceLoginPage::onCancelClicked);
}

void FaceLoginPage::startPreview()
{
    m_secondsLeft = kTimeoutSeconds;
    m_timeoutLabel->setText(QString("%1 秒后自动返回").arg(m_secondsLeft));
    m_statusLabel->setText("⌛ 正在检测人脸...");
    m_previewTimer->start(250);   // 4 fps refresh
    m_timeoutTimer->start(1000);  // 1s countdown tick
}

void FaceLoginPage::stopPreview()
{
    m_previewTimer->stop();
    m_timeoutTimer->stop();
}

void FaceLoginPage::refreshPreview()
{
    QPixmap px("/tmp/edgeguard_camera_preview.jpg");
    if (!px.isNull()) {
        m_previewLabel->setPixmap(px.scaled(m_previewLabel->size(),
                                   Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    // If file doesn't exist yet (visiond not ready), keep showing nothing
}

void FaceLoginPage::onTimeoutTick()
{
    if (m_secondsLeft > 0) {
        m_secondsLeft--;
        m_timeoutLabel->setText(QString("%1 秒后自动返回").arg(m_secondsLeft));
    }
    if (m_secondsLeft <= 0) {
        stopPreview();
        m_statusLabel->setText("⚠ 检测超时，请使用密码登录");
        emit faceLoginCancel();
    }
}

void FaceLoginPage::onCancelClicked()
{
    stopPreview();
    emit faceLoginCancel();
}

void FaceLoginPage::buildUi()
{
    setObjectName("FaceLoginRoot");

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 10, 20, 10);
    root->setSpacing(8);

    // Title
    m_titleLabel = new QLabel("人脸识别登录", this);
    m_titleLabel->setObjectName("FaceTitle");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    root->addWidget(m_titleLabel);

    root->addSpacing(4);

    // Camera preview — centered in a card-like frame
    QVBoxLayout *previewWrap = new QVBoxLayout();
    previewWrap->setAlignment(Qt::AlignCenter);

    m_previewLabel = new QLabel(this);
    m_previewLabel->setObjectName("PreviewBox");
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setFixedSize(400, 300);
    m_previewLabel->setText("摄像头未就绪");  // "摄像头未就绪"
    m_previewLabel->setStyleSheet(
        "QLabel#PreviewBox {"
        "  background: #101d2f;"
        "  border: 2px solid #263b58;"
        "  border-radius: 12px;"
        "  color: #8fb3d9;"
        "  font-size: 16px;"
        "}");
    previewWrap->addWidget(m_previewLabel);
    root->addLayout(previewWrap);

    root->addSpacing(6);

    // Status hint
    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName("StatusHint");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setText("功能开发中，请使用密码登录");
    root->addWidget(m_statusLabel);

    // Countdown
    m_timeoutLabel = new QLabel(this);
    m_timeoutLabel->setObjectName("TimeoutHint");
    m_timeoutLabel->setAlignment(Qt::AlignCenter);
    m_timeoutLabel->setText(QString("%1 秒后自动返回").arg(kTimeoutSeconds));
    root->addWidget(m_timeoutLabel);

    root->addSpacing(8);

    // Cancel button
    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setAlignment(Qt::AlignCenter);

    m_cancelBtn = new QPushButton("返回密码登录", this);  // "返回密码登录"
    m_cancelBtn->setObjectName("CancelBtn");
    m_cancelBtn->setFocusPolicy(Qt::NoFocus);
    m_cancelBtn->setFixedHeight(42);
    m_cancelBtn->setMinimumWidth(200);
    btnRow->addWidget(m_cancelBtn);

    root->addLayout(btnRow);
    root->addStretch(1);
}
