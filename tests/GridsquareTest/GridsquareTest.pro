QT += testlib core
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_gridsquare

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_gridsquare.cpp \
    ../../data/Gridsquare.cpp \
    ../../core/LogLocale.cpp

HEADERS += \
    ../../data/Gridsquare.h \
    ../../core/LogLocale.h
