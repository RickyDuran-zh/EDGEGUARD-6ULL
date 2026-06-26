QT += core gui widgets
CONFIG += c++11

TARGET = edgeguard-ui
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    loginpage.cpp \
    circulargauge.cpp \
    qtstackedwidget.cpp \
    sensorchart.cpp

HEADERS += \
    mainwindow.h \
    loginpage.h \
    circulargauge.h \
    qtstackedwidget.h \
    sensorchart.h
