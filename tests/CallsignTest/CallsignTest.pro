QT += testlib core
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_callsign

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_callsign.cpp \
    ../../data/Callsign.cpp

HEADERS += \
    ../../data/Callsign.h \
    generated_cases.h
