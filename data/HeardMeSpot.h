#ifndef QLOG_DATA_HEARDMESPOT_H
#define QLOG_DATA_HEARDMESPOT_H

#include <QDateTime>
#include <QString>

#include "data/Dxcc.h"

class HeardMeSpot
{
public:
    enum class DisplayGroup
    {
        PskReporter,
        CW,
        RTTY
    };

    QString callsign;
    QString locator;
    double frequency = 0.0;
    QString mode;
    qint32 report = 0;
    QDateTime timestamp;
    DxccStatus status = DxccStatus::UnknownStatus;
    qulonglong dupeCount = 0;
    bool strongSignal = false;
    DisplayGroup displayGroup = DisplayGroup::PskReporter;
    qint32 fadeAfterMs = 30 * 60 * 1000;
    qint32 removeAfterMs = 45 * 60 * 1000;
};

#endif // QLOG_DATA_HEARDMESPOT_H
