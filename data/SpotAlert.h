#ifndef QLOG_DATA_SPOTALERT_H
#define QLOG_DATA_SPOTALERT_H

#include <QMetaType>
#include "data/WsjtxEntry.h"
#include "data/DxSpot.h"

class SpotAlert
{

public:

    enum ALERTSOURCETYPE
    {
        DXSPOT = 0b1,
        WSJTXCQSPOT = 0b10
    };

    ALERTSOURCETYPE source;
    QStringList ruleNameList;
    WsjtxEntry spot;

    SpotAlert() : source(DXSPOT) {};

    SpotAlert(const QStringList &ruleList, const DxSpot &sourceSpot) :
        spot(sourceSpot)
    {
        source = SpotAlert::ALERTSOURCETYPE::DXSPOT;
        ruleNameList = ruleList;
    };

    SpotAlert(const QStringList &ruleList, const WsjtxEntry &sourceWsjtx) :
        spot(sourceWsjtx)
    {
        source = SpotAlert::ALERTSOURCETYPE::WSJTXCQSPOT;
        ruleNameList = ruleList;
    };

    const DxSpot& getDxSpot() const {return spot;};

private:

};

Q_DECLARE_METATYPE(SpotAlert);

#endif // QLOG_DATA_SPOTALERT_H
