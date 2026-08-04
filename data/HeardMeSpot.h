#ifndef QLOG_DATA_HEARDMESPOT_H
#define QLOG_DATA_HEARDMESPOT_H

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
    qint32 report = 0;
    DxccStatus status = DxccStatus::UnknownStatus;
    qulonglong dupeCount = 0;
    DisplayGroup displayGroup = DisplayGroup::PskReporter;
};

#endif // QLOG_DATA_HEARDMESPOT_H
