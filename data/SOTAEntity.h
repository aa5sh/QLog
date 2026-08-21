#ifndef QLOG_DATA_SOTAENTITY_H
#define QLOG_DATA_SOTAENTITY_H

#include <QtCore>

class SOTAEntity {
public:
    QString summitCode;
    QString associationName;
    QString regionName;
    QString summitName;
    qint32 altm = 0;
    qint32 altft = 0;
    double gridref1 = 0.0;
    double gridref2 = 0.0;
    double longitude = 0.0;
    double latitude = 0.0;
    qint16 points = 0;
    qint16 bonusPoints = 0;
    QDate validFrom;
    QDate validTo;
};

#endif // QLOG_DATA_SOTAENTITY_H
