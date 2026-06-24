// faceloginpage.h — Face login page with live camera preview
// Displays real-time camera feed from /tmp/edgeguard_camera_preview.jpg
// Face recognition is in development — shows placeholder until P2 JPEG fix

#ifndef FACELOGINPAGE_H
#define FACELOGINPAGE_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

class FaceLoginPage : public QWidget
{
    Q_OBJECT

public:
    explicit FaceLoginPage(QWidget *parent = nullptr);
    void startPreview();
    void stopPreview();

signals:
    void faceLoginCancel();
    void faceLoginSuccess();  // reserved for P2 fix

private slots:
    void refreshPreview();
    void checkVerifyResult();
    void onTimeoutTick();
    void onCancelClicked();

private:
    void buildUi();

    QLabel *m_previewLabel;
    QLabel *m_titleLabel;
    QLabel *m_statusLabel;
    QLabel *m_timeoutLabel;
    QPushButton *m_cancelBtn;
    QTimer *m_previewTimer;
    QTimer *m_timeoutTimer;
    QTimer *m_verifyTimer;
    int m_secondsLeft;

    static const int kTimeoutSeconds = 30;
};

#endif // FACELOGINPAGE_H
