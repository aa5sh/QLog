QT += testlib core
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_quadkeycache

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_quadkeycache.cpp

HEADERS += \
    ../../core/QuadKeyCache.h
