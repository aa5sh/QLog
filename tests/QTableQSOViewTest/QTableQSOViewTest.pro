QT += core gui sql widgets testlib

CONFIG += testcase c++11
TEMPLATE = app
TARGET = tst_qtableqsoview

INCLUDEPATH += ../..

SOURCES += \
    tst_qtableqsoview.cpp \
    ../../ui/QTableQSOView.cpp

HEADERS += \
    ../../ui/QTableQSOView.h
