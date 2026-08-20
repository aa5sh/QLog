#ifndef RIG_MACROS_H
#define RIG_MACROS_H

#include <QtGlobal>

#define BANDWIDTH_UNKNOWN 0
#define QSTRING_FREQ(f) (QString::number((f), 'f', 5))
#define Hz2MHz(f) ((double)((f)/1e6))
#define Hz2kHz(f) ((double)((f)/1e3))
#define mW2W(f) ((double)((f)/1000.0))

inline qint64 MHz2Hz(double frequencyMHz)
{
    return qRound64(frequencyMHz * 1000000.0);
}

#endif // RIG_MACROS_H
