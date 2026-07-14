QT += testlib core sql
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_stationprofile

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_stationprofile.cpp \
    ../../data/StationProfile.cpp

HEADERS += \
    ../../data/StationProfile.h \
    ../../data/ProfileManager.h
