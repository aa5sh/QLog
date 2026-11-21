QT += testlib core gui sql network serialport websockets
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_rc28

INCLUDEPATH += $$PWD/../..
include(../hamlib.pri)

SOURCES += \
    tst_rc28.cpp \
    test_stubs.cpp \
    ../../rig/Rc28Controller.cpp \
    ../../rig/Rc28Protocol.cpp

HEADERS += \
    ../../rig/Rc28Controller.h \
    ../../rig/Rc28Device.h \
    ../../rig/Rc28Protocol.h
