#ifndef QLOG_DATA_BAND_H
#define QLOG_DATA_BAND_H

#include <QtCore>

class Band {
public:
    QString name;
    double start;
    double end;
    QString satDesignator;
    bool operator==(const Band &band) const
    {
        return ( this->name == band.name
                 && this->start == band.start
                 && this->end == band.end
                 && this->satDesignator == band.satDesignator );
    }
};

#endif // QLOG_DATA_BAND_H
