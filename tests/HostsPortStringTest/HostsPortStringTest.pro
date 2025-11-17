QT += testlib core network
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_hostsportstring

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_hostsportstring.cpp \
    ../../data/HostsPortString.cpp

HEADERS += \
    ../../data/HostsPortString.h
