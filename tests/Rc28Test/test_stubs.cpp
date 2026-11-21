#include "test_stubs.h"

// Hardware-free Rc28Device: presence and output reports are controlled by the test.
bool Rc28DeviceStub::present = false;
QList<QByteArray> Rc28DeviceStub::writes;

struct Rc28Device::Handle
{
};

Rc28Device::Rc28Device(QObject *parent) :
    QObject(parent),
    handle(nullptr)
{
}

Rc28Device::~Rc28Device()
{
    close();
}

bool Rc28Device::open()
{
    if ( !handle && Rc28DeviceStub::present )
        handle = new Handle;
    return handle != nullptr;
}

void Rc28Device::close()
{
    delete handle;
    handle = nullptr;
}

bool Rc28Device::isOpen() const
{
    return handle != nullptr;
}

bool Rc28Device::write(const QByteArray &report)
{
    if ( !handle )
        return false;
    Rc28DeviceStub::writes << report;
    return true;
}

void Rc28Device::fail()
{
    close();
    emit removed();
}
