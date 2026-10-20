QT += testlib core sql network widgets
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_alertevaluator

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_alertevaluator.cpp \
    ../../core/AlertEvaluator.cpp \
    ../../data/DxccLogStatusCondition.cpp \
    ../../data/BandPlan.cpp \
    ../../notification/AlertNotification.cpp

HEADERS += \
    ../../core/AlertEvaluator.h \
    ../../data/DxSpot.h \
    ../../data/WsjtxEntry.h \
    ../../data/SpotAlert.h \
    ../../notification/DesktopNotificationCategory.h \
    ../../notification/AlertNotification.h \
    ../../data/BandPlan.h

SOURCES += ../../data/StationProfile.cpp ../../data/Gridsquare.cpp
HEADERS += ../../data/StationProfile.h ../../data/ProfileManager.h
SOURCES += ../../core/LogLocale.cpp
HEADERS += ../../core/LogLocale.h
