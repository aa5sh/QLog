QT += testlib core sql
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_bandplan

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_bandplan.cpp \
    ../../data/BandPlan.cpp

HEADERS += \
    ../../data/BandPlan.h \
    ../../data/Band.h
