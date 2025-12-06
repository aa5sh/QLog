QT += testlib core
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_dxserverstring

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_dxserverstring.cpp \
    ../../data/DxServerString.cpp

HEADERS += \
    ../../data/DxServerString.h
