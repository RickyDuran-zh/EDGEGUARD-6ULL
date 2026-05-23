#include <QApplication>
#include <QFont>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QFont font;
    font.setFamily("DejaVu Sans");
    font.setPointSize(10);
    app.setFont(font);

    MainWindow w;
    w.showFullScreen();

    return app.exec();
}
