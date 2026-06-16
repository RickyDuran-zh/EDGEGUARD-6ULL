#include <QApplication>
#include <QFont>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QFont font;
    font.setFamily("Source Han Serif SC");
    font.setPointSize(10);
    app.setFont(font);

    MainWindow w;
    w.showFullScreen();

    return app.exec();
}
