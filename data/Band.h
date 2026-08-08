#ifndef QLOG_DATA_BAND_H
#define QLOG_DATA_BAND_H

#include <QtCore>
#include "rig/macros.h"

class Band {
public:
    QString name;
    double start = 0.0;
    double end = 0.0;
    QString satDesignator;
    bool contains(double frequency) const
    {
        const qint64 frequencyHz = MHz2Hz(frequency);
        return frequencyHz >= MHz2Hz(start) && frequencyHz <= MHz2Hz(end);
    }
    bool operator==(const Band &band) const
    {
        return ( this->name == band.name
                 && MHz2Hz(this->start) == MHz2Hz(band.start)
                 && MHz2Hz(this->end) == MHz2Hz(band.end)
                 && this->satDesignator == band.satDesignator );
    }
};

Q_DECLARE_METATYPE(Band);

#endif // QLOG_DATA_BAND_H
