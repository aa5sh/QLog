#include "rig/Rc28Protocol.h"

#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.rig.rc28protocol");

bool Rc28Protocol::parseReport(const QByteArray &data, Report &report)
{
    FCT_IDENTIFICATION;

    // Byte 0 is 0x01 for input state; other values are firmware replies.
    // Byte 5 holds the active-low buttons: bit 0 TX, bit 1 F1, bit 2 F2.
    if ( data.size() < REPORT_SIZE
         || static_cast<quint8>(data.at(0)) != 0x01
         || ( static_cast<quint8>(data.at(5)) & ~0x07 ) != 0 )
        return false;

    const int count = static_cast<quint8>(data.at(1));
    const quint8 direction = static_cast<quint8>(data.at(3));
    const quint8 buttons = static_cast<quint8>(data.at(5));

    report.steps = ( direction == 0x01 ) ? count
                 : ( direction == 0x02 ) ? -count
                 : 0;
    report.txPressed = ( buttons & 0x01 ) == 0;
    report.f1Pressed = ( buttons & 0x02 ) == 0;
    return true;
}

QByteArray Rc28Protocol::ledReport(quint8 litLeds)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << litLeds;

    QByteArray report(REPORT_SIZE, '\0');
    report[0] = 0x01;
    report[1] = static_cast<char>(0x0F & ~litLeds);
    return report;
}

int Rc28Protocol::tuningStepHz(const QString &mode, bool fast)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << mode << fast;

    // 10 Hz per detent matches the radios' own fine tuning for CW, SSB and
    // data modes. AM and FM use coarser steps so the knob stays usable there.
    int stepHz = 10;

    if ( mode == QLatin1String("AM") )
        stepHz = 100;
    else if ( mode == QLatin1String("FM") )
        stepHz = 1000;

    return fast ? stepHz * 10 : stepHz;
}

qint64 Rc28Protocol::tunedFrequencyHz(qint64 currentHz, int steps, int stepHz)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << currentHz << steps << stepHz;

    if ( currentHz <= 0 || stepHz <= 0 || steps == 0 )
        return currentHz;

    // Snap to the step grid in the turning direction, so the first detent from
    // an off-grid frequency lands on the next grid point, as Icom radios do.
    qint64 gridHz = ( currentHz / stepHz ) * stepHz;

    if ( steps < 0 && gridHz != currentHz )
        gridHz += stepHz;

    const qint64 targetHz = gridHz + static_cast<qint64>(steps) * stepHz;
    return targetHz > 0 ? targetHz : currentHz;
}
