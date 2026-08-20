#ifndef QLOG_DATA_WWFFENTITY_H
#define QLOG_DATA_WWFFENTITY_H

#include <QtCore>

class WWFFEntity {
public:
    QString reference;
    QString status;
    QString name;
    QString program;
    QString dxcc;
    QString state;
    QString county;
    QString continent;
    QString iota;
    QString iaruLocator;
    double latitude = 0.0;
    double longitude = 0.0;
    QString iucncat;
    QDate validFrom;
    QDate validTo;
};


#endif // QLOG_DATA_WWFFENTITY_H
