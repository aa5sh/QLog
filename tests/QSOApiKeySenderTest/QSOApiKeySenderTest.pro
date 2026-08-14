QT += testlib core network sql
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_qsoapikeysender

DEFINES += VERSION=\\\"test\\\"

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_qsoapikeysender.cpp \
    test_stubs.cpp \
    ../../core/QSOApiKeySender.cpp \
    ../../core/QSOApiKeyQuery.cpp \
    ../../logformat/AdiFormat.cpp \
    ../../core/LogLocale.cpp \
    ../../data/Accents.cpp

HEADERS += \
    ../../core/QSOApiKeySender.h \
    ../../core/QSOApiKeyQuery.h \
    ../../core/CredentialStore.h \
    ../../logformat/AdiFormat.h \
    ../../logformat/LogFormat.h \
    ../../data/Data.h \
    ../../core/LogLocale.h
