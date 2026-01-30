#ifndef QLOG_DATA_WSJTXDECODE_H
#define QLOG_DATA_WSJTXDECODE_H

#include <QString>
#include <QTime>

class WsjtxDecode
{
public:
    QString id, mode, message;
    bool is_new, low_confidence, off_air;
    QTime time;
    qint32 snr;
    quint32 df;
    double dt;
    operator QString() const
    {
        return QString("WsjtxDecode: ")
                + "("
                + "ID: "            + id + "; "
                + "IsNew: "         + QString::number(is_new) + "; "
                + "Time: "          + time.toString() + "; "
                + "SNR: "           + QString::number(snr) + "; "
                + "DeltaTime: "     + QString::number(dt) + "; "
                + "DeltaFreq: "     + QString::number(df) + "; "
                + "Mode: "          + mode + "; "
                + "Message: "       + message + "; "
                + "LowConfidence: " + QString::number(low_confidence) + "; "
                + "OffAir: "        + QString::number(off_air) + "; "
                + ")";
    }
};

#endif // QLOG_DATA_WSJTXDECODE_H
