#ifndef QLOG_RIG_RC28PROTOCOL_H
#define QLOG_RIG_RC28PROTOCOL_H

#include <QByteArray>
#include <QString>

// Hardware-independent part of the Icom RC-28 USB HID protocol.
// Reports are handled without a HID report ID; the RC-28 does not number them.
class Rc28Protocol
{
public:
    struct Report
    {
        // Encoder detents since the previous report; positive is clockwise.
        int steps = 0;
        bool txPressed = false;
        bool f1Pressed = false;
    };

    // Output report bits. The device lights an LED when its bit is cleared.
    enum Led
    {
        LED_TX = 0x01,
        LED_F1 = 0x02,
        LED_LINK = 0x08
    };

    static const quint16 VENDOR_ID = 0x0C26;
    static const quint16 PRODUCT_ID = 0x001E;
    static const int REPORT_SIZE = 32;

    static bool parseReport(const QByteArray &data, Report &report);
    static QByteArray ledReport(quint8 litLeds);
    static int tuningStepHz(const QString &mode, bool fast);
    static qint64 tunedFrequencyHz(qint64 currentHz, int steps, int stepHz);
};

#endif // QLOG_RIG_RC28PROTOCOL_H
