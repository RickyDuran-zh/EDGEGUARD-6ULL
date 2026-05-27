#include <QApplication>
#include <QLabel>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QLabel label("Qt linuxfb test OK");
    label.resize(800, 480);
    label.setAlignment(Qt::AlignCenter);
    label.setStyleSheet("background:black; color:white; font-size:36px;");
    label.showFullScreen();

    return app.exec();
}