#ifndef QLOG_DATA_POTAENTITY_H
#define QLOG_DATA_POTAENTITY_H

#include <QtCore>

class POTAEntity {
public:
    QString reference;
    QString name;
    bool active = false;
    qint16 entityID = 0;
    QString locationDesc;
    double longitude = 0.0;
    double latitude = 0.0;
    QString grid;
};

#endif // QLOG_DATA_POTAENTITY_H
