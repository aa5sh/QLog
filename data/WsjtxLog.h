#ifndef QLOG_DATA_WSJTXLOG_H
#define QLOG_DATA_WSJTXLOG_H

#include <QString>
#include <QDateTime>

class WsjtxLog
{
public:
    QString id, dx_call, dx_grid, mode, rprt_sent, rprt_rcvd;
    QString tx_pwr, comments, name, op_call, my_call, my_grid, prop_mode;
    QString exch_sent, exch_rcvd;
    QDateTime time_on, time_off;
    quint64 tx_freq;
    operator QString() const
    {
        return QString("WsjtxLog: ")
                + "("
                + "ID: "           + id + "; "
                + "DateTimeOff: "  + time_off.toString() + "; "
                + "DXCall: "       + dx_call + "; "
                + "DXGrid: "       + dx_grid + "; "
                + "TXFreq: "       + QString::number(tx_freq) + "; "
                + "Mode: "         + mode + "; "
                + "RrpSent: "      + rprt_sent + "; "
                + "RrpRcvd: "      + rprt_rcvd + "; "
                + "TxPower: "      + tx_pwr + "; "
                + "Comments: "     + comments + "; "
                + "Name: "         + name + "; "
                + "DateTimeOn: "   + time_on.toString() + "; "
                + "OpCall: "       + op_call + "; "
                + "MyCall: "       + my_call + "; "
                + "MyGrid: "       + my_grid + "; "
                + "ExchSent: "     + exch_sent + "; "
                + "ExchRcvd: "     + exch_rcvd + "; "
                + "ADIFPropMode: " + prop_mode + "; "
                + ")";
    }
};

#endif // QLOG_DATA_WSJTXLOG_H
