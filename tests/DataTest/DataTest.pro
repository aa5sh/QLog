QT += testlib core sql
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_data

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_data.cpp \
    ../../data/Accents.cpp
