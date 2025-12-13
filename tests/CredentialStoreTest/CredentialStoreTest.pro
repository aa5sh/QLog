QT += testlib core widgets
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = tst_credentialstore

INCLUDEPATH += $$PWD/../..

SOURCES += \
    tst_credentialstore.cpp \
    ../../core/CredentialStore.cpp

!isEmpty(QTKEYCHAININCLUDEPATH) {
   INCLUDEPATH += $$QTKEYCHAININCLUDEPATH
}

!isEmpty(QTKEYCHAINLIBPATH) {
   LIBS += -L$$QTKEYCHAINLIBPATH
}

equals(QT_MAJOR_VERSION, 6): LIBS += -lqt6keychain
equals(QT_MAJOR_VERSION, 5): LIBS += -lqt5keychain

HEADERS += \
    ../../core/CredentialStore.h
