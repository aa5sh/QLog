#ifndef QLOG_DATA_WSJTXLOGADIF_H
#define QLOG_DATA_WSJTXLOGADIF_H

#include <QString>

class WsjtxLogADIF
{
public:
    QString id, log_adif;

    operator QString() const
    {
        return QString("WsjtxLogADIF")
                 + "("
                 + "ID: "  + id + ";"
                 + "ADIF: " + log_adif + ";"
                 + ")";
    }
};

#endif // QLOG_DATA_WSJTXLOGADIF_H
