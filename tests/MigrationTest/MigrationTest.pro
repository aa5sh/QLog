QT += testlib sql core widgets network
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_migration

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_migration.cpp

RESOURCES += \
    ../../res/res.qrc

HEADERS += \
    ../../core/Migration.h
