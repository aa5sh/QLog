#ifndef QLOG_TESTS_RC28TEST_STUBS_H
#define QLOG_TESTS_RC28TEST_STUBS_H

#include <QByteArray>
#include <QList>

#include "rig/Rc28Device.h"

struct Rc28DeviceStub
{
    // Whether open() finds an RC-28.
    static bool present;
    // Output reports written while open.
    static QList<QByteArray> writes;
};

#endif // QLOG_TESTS_RC28TEST_STUBS_H
