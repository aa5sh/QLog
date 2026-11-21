#include "rig/Rc28Device.h"

#if defined(Q_OS_MACOS)
#include <QVarLengthArray>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDDevice.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDManager.h>
#elif defined(Q_OS_WIN)
#include <QWinEventNotifier>
#include <windows.h>
#include <setupapi.h>
extern "C" {
#include <hidsdi.h>
}
#elif defined(Q_OS_LINUX)
#include <QDir>
#include <QFile>
#include <QSocketNotifier>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "rig/Rc28Protocol.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.rig.rc28device");

#if defined(Q_OS_MACOS)

struct Rc28Device::Handle
{
    IOHIDManagerRef manager = nullptr;
    IOHIDDeviceRef device = nullptr;
    uint8_t inputBuffer[64];

    static void inputReport(void *context, IOReturn result, void *, IOHIDReportType,
                            uint32_t, uint8_t *report, CFIndex length)
    {
        FCT_IDENTIFICATION;

        if ( result != kIOReturnSuccess || length <= 0 )
            return;

        Rc28Device *owner = static_cast<Rc28Device *>(context);
        emit owner->reportReceived(QByteArray(reinterpret_cast<const char *>(report),
                                              static_cast<int>(length)));
    }

    static void deviceRemoved(void *context, IOReturn, void *)
    {
        FCT_IDENTIFICATION;

        Rc28Device *owner = static_cast<Rc28Device *>(context);
        const IOHIDDeviceRef removedDevice = owner->handle ? owner->handle->device : nullptr;

        // Release the device only after IOKit has returned from this callback.
        QMetaObject::invokeMethod(owner, [owner, removedDevice]()
        {
            if ( owner->handle && owner->handle->device == removedDevice )
                owner->fail();
        }, Qt::QueuedConnection);
    }
};

bool Rc28Device::open()
{
    FCT_IDENTIFICATION;

    if ( handle )
        return true;

    IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);

    if ( !manager )
        return false;

    const int vendorId = Rc28Protocol::VENDOR_ID;
    const int productId = Rc28Protocol::PRODUCT_ID;
    CFNumberRef vendor = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &vendorId);
    CFNumberRef product = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &productId);
    const void *keys[] = { CFSTR(kIOHIDVendorIDKey), CFSTR(kIOHIDProductIDKey) };
    const void *values[] = { vendor, product };
    CFDictionaryRef matching = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 2,
                                                  &kCFTypeDictionaryKeyCallBacks,
                                                  &kCFTypeDictionaryValueCallBacks);
    CFRelease(vendor);
    CFRelease(product);
    IOHIDManagerSetDeviceMatching(manager, matching);
    CFRelease(matching);

    IOHIDDeviceRef device = nullptr;
    CFSetRef devices = IOHIDManagerCopyDevices(manager);

    if ( devices )
    {
        const CFIndex count = CFSetGetCount(devices);

        if ( count > 0 )
        {
            QVarLengthArray<const void *, 4> found(static_cast<int>(count));
            CFSetGetValues(devices, found.data());
            device = static_cast<IOHIDDeviceRef>(const_cast<void *>(found.at(0)));
            CFRetain(device);
        }
        CFRelease(devices);
    }

    if ( !device )
    {
        CFRelease(manager);
        return false;
    }

    const IOReturn result = IOHIDDeviceOpen(device, kIOHIDOptionsTypeNone);

    if ( result != kIOReturnSuccess )
    {
        qCWarning(runtime) << "Cannot open RC-28" << QString::number(static_cast<quint32>(result), 16);
        CFRelease(device);
        CFRelease(manager);
        return false;
    }

    handle = new Handle;
    handle->manager = manager;
    handle->device = device;
    IOHIDDeviceRegisterInputReportCallback(device, handle->inputBuffer, sizeof(handle->inputBuffer),
                                           &Handle::inputReport, this);
    IOHIDDeviceRegisterRemovalCallback(device, &Handle::deviceRemoved, this);
    IOHIDDeviceScheduleWithRunLoop(device, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
    return true;
}

void Rc28Device::close()
{
    FCT_IDENTIFICATION;

    if ( !handle )
        return;

    IOHIDDeviceRegisterInputReportCallback(handle->device, handle->inputBuffer,
                                           sizeof(handle->inputBuffer), nullptr, nullptr);
    IOHIDDeviceRegisterRemovalCallback(handle->device, nullptr, nullptr);
    IOHIDDeviceUnscheduleFromRunLoop(handle->device, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
    IOHIDDeviceClose(handle->device, kIOHIDOptionsTypeNone);
    CFRelease(handle->device);
    CFRelease(handle->manager);
    delete handle;
    handle = nullptr;
}

bool Rc28Device::write(const QByteArray &report)
{
    FCT_IDENTIFICATION;

    if ( !handle )
        return false;

    // Unnumbered reports use report ID 0 and carry no ID byte on macOS.
    const IOReturn result = IOHIDDeviceSetReport(handle->device, kIOHIDReportTypeOutput, 0,
                                                 reinterpret_cast<const uint8_t *>(report.constData()),
                                                 report.size());
    if ( result != kIOReturnSuccess )
    {
        qCWarning(runtime) << "RC-28 output report failed" << QString::number(static_cast<quint32>(result), 16);
        return false;
    }
    return true;
}

#elif defined(Q_OS_WIN)

struct Rc28Device::Handle
{
    HANDLE file = INVALID_HANDLE_VALUE;
    OVERLAPPED readOverlapped;
    bool readPending = false;
    QWinEventNotifier *readNotifier = nullptr;
    // Windows prefixes every report with its report ID byte (0 here).
    QByteArray readBuffer;
    int outputLength = 0;

    bool startRead()
    {
        FCT_IDENTIFICATION;

        // ReadFile resets the manual-reset event; it is signaled on completion,
        // including when the read completes immediately.
        readPending = ReadFile(file, readBuffer.data(), static_cast<DWORD>(readBuffer.size()),
                               nullptr, &readOverlapped)
                      || GetLastError() == ERROR_IO_PENDING;
        return readPending;
    }
};

bool Rc28Device::open()
{
    FCT_IDENTIFICATION;

    if ( handle )
        return true;

    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO deviceInfo = SetupDiGetClassDevsW(&hidGuid, nullptr, nullptr,
                                               DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if ( deviceInfo == INVALID_HANDLE_VALUE )
        return false;

    const QString idTag = QString::asprintf("vid_%04x&pid_%04x",
                                            Rc28Protocol::VENDOR_ID, Rc28Protocol::PRODUCT_ID);
    HANDLE file = INVALID_HANDLE_VALUE;
    SP_DEVICE_INTERFACE_DATA interfaceData;
    interfaceData.cbSize = sizeof(interfaceData);

    for ( DWORD index = 0;
          file == INVALID_HANDLE_VALUE
          && SetupDiEnumDeviceInterfaces(deviceInfo, nullptr, &hidGuid, index, &interfaceData);
          ++index )
    {
        DWORD size = 0;
        SetupDiGetDeviceInterfaceDetailW(deviceInfo, &interfaceData, nullptr, 0, &size, nullptr);
        if ( size == 0 )
            continue;

        QByteArray detailBuffer(static_cast<int>(size), '\0');
        SP_DEVICE_INTERFACE_DETAIL_DATA_W *detail
            = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(detailBuffer.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        if ( !SetupDiGetDeviceInterfaceDetailW(deviceInfo, &interfaceData, detail, size, nullptr, nullptr) )
            continue;

        if ( !QString::fromWCharArray(detail->DevicePath).contains(idTag, Qt::CaseInsensitive) )
            continue;

        file = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                           FILE_FLAG_OVERLAPPED, nullptr);
        if ( file == INVALID_HANDLE_VALUE )
            qCWarning(runtime) << "Cannot open RC-28" << GetLastError();
    }
    SetupDiDestroyDeviceInfoList(deviceInfo);

    if ( file == INVALID_HANDLE_VALUE )
        return false;

    PHIDP_PREPARSED_DATA preparsed = nullptr;
    HIDP_CAPS caps;
    bool capsValid = false;

    if ( HidD_GetPreparsedData(file, &preparsed) )
    {
        capsValid = HidP_GetCaps(preparsed, &caps) == HIDP_STATUS_SUCCESS
                    && caps.InputReportByteLength > 1 && caps.OutputReportByteLength > 1;
        HidD_FreePreparsedData(preparsed);
    }

    HANDLE readEvent = capsValid ? CreateEventW(nullptr, TRUE, FALSE, nullptr) : nullptr;

    if ( !readEvent )
    {
        qCWarning(runtime) << "Cannot prepare RC-28" << GetLastError();
        CloseHandle(file);
        return false;
    }

    handle = new Handle;
    handle->file = file;
    ZeroMemory(&handle->readOverlapped, sizeof(handle->readOverlapped));
    handle->readOverlapped.hEvent = readEvent;
    handle->readBuffer = QByteArray(caps.InputReportByteLength, '\0');
    handle->outputLength = caps.OutputReportByteLength;
    handle->readNotifier = new QWinEventNotifier(readEvent, this);

    connect(handle->readNotifier, &QWinEventNotifier::activated, this, [this]()
    {
        if ( !handle )
            return;

        DWORD length = 0;
        handle->readPending = false;

        if ( !GetOverlappedResult(handle->file, &handle->readOverlapped, &length, FALSE) )
        {
            qCDebug(runtime) << "RC-28 read failed" << GetLastError();
            fail();
            return;
        }

        if ( length > 1 )
            emit reportReceived(handle->readBuffer.mid(1, static_cast<int>(length) - 1));

        // A receiver may have closed the device while handling the report.
        if ( handle && !handle->startRead() )
            fail();
    });

    if ( !handle->startRead() )
    {
        qCWarning(runtime) << "Cannot read RC-28" << GetLastError();
        close();
        return false;
    }
    return true;
}

void Rc28Device::close()
{
    FCT_IDENTIFICATION;

    if ( !handle )
        return;

    handle->readNotifier->setEnabled(false);
    handle->readNotifier->deleteLater();

    if ( handle->readPending )
    {
        // Wait for the cancelled read so the kernel no longer owns readBuffer.
        DWORD ignored = 0;
        CancelIo(handle->file);
        GetOverlappedResult(handle->file, &handle->readOverlapped, &ignored, TRUE);
    }

    CloseHandle(handle->readOverlapped.hEvent);
    CloseHandle(handle->file);
    delete handle;
    handle = nullptr;
}

bool Rc28Device::write(const QByteArray &report)
{
    FCT_IDENTIFICATION;

    if ( !handle )
        return false;

    // Report ID 0 first, padded to the length the device descriptor declares.
    QByteArray buffer(handle->outputLength, '\0');
    const int length = qMin(report.size(), handle->outputLength - 1);
    memcpy(buffer.data() + 1, report.constData(), static_cast<size_t>(length));

    if ( !HidD_SetOutputReport(handle->file, buffer.data(), static_cast<ULONG>(buffer.size())) )
    {
        qCWarning(runtime) << "RC-28 output report failed" << GetLastError();
        return false;
    }
    return true;
}

#elif defined(Q_OS_LINUX)

struct Rc28Device::Handle
{
    int fd = -1;
    QSocketNotifier *notifier = nullptr;
};

bool Rc28Device::open()
{
    FCT_IDENTIFICATION;

    if ( handle )
        return true;

    const QString hidId = QString::asprintf("HID_ID=0003:%08X:%08X",
                                            Rc28Protocol::VENDOR_ID, Rc28Protocol::PRODUCT_ID);
    const QDir hidraw(QStringLiteral("/sys/class/hidraw"));

    for ( const QString &name : hidraw.entryList(QDir::Dirs | QDir::NoDotAndDotDot) )
    {
        QFile uevent(hidraw.filePath(name + QStringLiteral("/device/uevent")));

        if ( !uevent.open(QIODevice::ReadOnly)
             || !QString::fromLatin1(uevent.readAll()).contains(hidId, Qt::CaseInsensitive) )
            continue;

        const QByteArray path = QFile::encodeName(QStringLiteral("/dev/") + name);
        const int fd = ::open(path.constData(), O_RDWR | O_NONBLOCK | O_CLOEXEC);

        if ( fd < 0 )
        {
            // Usually missing udev permissions for the hidraw node.
            qCWarning(runtime) << "Cannot open RC-28" << path << strerror(errno);
            continue;
        }

        handle = new Handle;
        handle->fd = fd;
        handle->notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);

        connect(handle->notifier, &QSocketNotifier::activated, this, [this]()
        {
            char buffer[64];

            while ( handle )
            {
                const ssize_t length = ::read(handle->fd, buffer, sizeof(buffer));

                if ( length > 0 )
                {
                    emit reportReceived(QByteArray(buffer, static_cast<int>(length)));
                    continue;
                }

                if ( length < 0 && ( errno == EAGAIN || errno == EWOULDBLOCK ) )
                    return;

                if ( length < 0 && errno == EINTR )
                    continue;

                qCDebug(runtime) << "RC-28 read failed" << strerror(errno);
                fail();
                return;
            }
        });
        return true;
    }
    return false;
}

void Rc28Device::close()
{
    FCT_IDENTIFICATION;

    if ( !handle )
        return;

    handle->notifier->setEnabled(false);
    handle->notifier->deleteLater();
    ::close(handle->fd);
    delete handle;
    handle = nullptr;
}

bool Rc28Device::write(const QByteArray &report)
{
    FCT_IDENTIFICATION;

    if ( !handle )
        return false;

    // hidraw expects the report ID (0 for unnumbered reports) as first byte.
    const QByteArray buffer = QByteArray(1, '\0') + report;
    const ssize_t written = ::write(handle->fd, buffer.constData(), static_cast<size_t>(buffer.size()));

    if ( written != buffer.size() )
    {
        qCWarning(runtime) << "RC-28 output report failed" << strerror(errno);
        return false;
    }
    return true;
}

#else

struct Rc28Device::Handle
{
};

bool Rc28Device::open()
{
    FCT_IDENTIFICATION;

    return false;
}

void Rc28Device::close()
{
    FCT_IDENTIFICATION;
}

bool Rc28Device::write(const QByteArray &)
{
    FCT_IDENTIFICATION;

    return false;
}

#endif

Rc28Device::Rc28Device(QObject *parent) :
    QObject(parent),
    handle(nullptr)
{
    FCT_IDENTIFICATION;
}

Rc28Device::~Rc28Device()
{
    FCT_IDENTIFICATION;

    close();
}

bool Rc28Device::isOpen() const
{
    FCT_IDENTIFICATION;

    return handle != nullptr;
}

void Rc28Device::fail()
{
    FCT_IDENTIFICATION;

    close();
    emit removed();
}
