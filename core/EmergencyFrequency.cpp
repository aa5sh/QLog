#include "EmergencyFrequency.h"
#include "core/debug.h"
#include "rig/macros.h"

MODULE_IDENTIFICATION("qlog.core.emergencyfrequency");

double EmergencyFrequency::TOLERANCE_MHZ = 0.001;

const QList<EmergencyFreqEntry> &EmergencyFrequency::list()
{
    FCT_IDENTIFICATION;

    static const QList<EmergencyFreqEntry> freqs =
    {
        {   3.760, "LSB"},
        {   7.110, "LSB"},
        {  14.300, "USB"},
        {  18.160, "USB"},
        {  21.360, "USB"},
        { 145.550, "FM"},
        { 433.550, "FM"},
    };
    return freqs;
}

const EmergencyFreqEntry *EmergencyFrequency::inBand(double startMHz, double endMHz)
{
    FCT_IDENTIFICATION;

    const qint64 startHz = MHz2Hz(startMHz);
    const qint64 endHz = MHz2Hz(endMHz);
    for ( const EmergencyFreqEntry &entry : list() )
    {
        const qint64 frequencyHz = MHz2Hz(entry.frequency);
        if ( frequencyHz >= startHz && frequencyHz <= endHz )
            return &entry;
    }

    return nullptr;
}

const EmergencyFreqEntry *EmergencyFrequency::findEmergency(double freqMHz)
{
    FCT_IDENTIFICATION;

    const QList<EmergencyFreqEntry> &freqs = EmergencyFrequency::list();
    const qint64 frequencyHz = MHz2Hz(freqMHz);
    const qint64 toleranceHz = MHz2Hz(EmergencyFrequency::TOLERANCE_MHZ);

    for ( const EmergencyFreqEntry &entry : freqs )
        if ( qAbs(frequencyHz - MHz2Hz(entry.frequency)) <= toleranceHz )
            return &entry;

    return nullptr;
}
