#ifndef QLOG_RIG_RC28DEVICE_H
#define QLOG_RIG_RC28DEVICE_H

#include <QObject>
#include <QByteArray>

// Native HID transport for one Icom RC-28 (IOKit, hid.dll or hidraw).
// It lives in the GUI thread; input arrives through that thread's event loop.
class Rc28Device : public QObject
{
    Q_OBJECT

public:
    explicit Rc28Device(QObject *parent = nullptr);
    ~Rc28Device() override;

    // Opens the first connected RC-28. Returns false when none is present or
    // it cannot be opened, so callers can simply retry later.
    bool open();
    void close();
    bool isOpen() const;
    // Sends an output report without report ID (see Rc28Protocol).
    bool write(const QByteArray &report);

signals:
    // Input report payload without report ID.
    void reportReceived(const QByteArray &report);
    // The device was unplugged or failed; it is already closed.
    void removed();

private:
    // Platform-specific handle state, defined in Rc28Device.cpp.
    struct Handle;

    void fail();

    Handle *handle;
};

#endif // QLOG_RIG_RC28DEVICE_H
