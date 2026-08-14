QT += testlib core sql
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_qsoapikeyquery

DEFINES += VERSION=\\\"test\\\"

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_qsoapikeyquery.cpp \
    test_stubs.cpp \
    ../../core/LogLocale.cpp \
    ../../data/Accents.cpp \
    ../../logformat/AdiFormat.cpp \
    ../../core/QSOApiKeyQuery.cpp

HEADERS += \
    ../../core/LogLocale.h \
    ../../data/Data.h \
    ../../logformat/AdiFormat.h \
    ../../logformat/LogFormat.h \
    ../../core/QSOApiKeyQuery.h
