#ifndef QLOG_DATA_WSJTXSTATUS_H
#define QLOG_DATA_WSJTXSTATUS_H

#include <QString>

class WsjtxStatus
{
public:
    QString id, mode, tx_mode, sub_mode;
    QString dx_call, dx_grid, de_call, de_grid;
    QString report;
    quint64 dial_freq;
    qint32 rx_df, tx_df;
    bool tx_enabled, transmitting, decoding;
    bool tx_watchdog, fast_mode;
    quint8 special_op_mode;
    quint32 freq_tolerance, tr_period;
    QString conf_name, tx_message;

    operator QString() const
    {
        return QString("WsjtxStatus: ")
                + "("
                + "ID: "           + id + "; "
                + "Dial: "         + QString::number(dial_freq) + "; "
                + "Mode: "         + mode + "; "
                + "DXCall: "       + dx_call + "; "
                + "Report: "       + report + "; "
                + "TXMode: "       + tx_mode + "; "
                + "TXEnabled: "    + QString::number(tx_enabled) + "; "
                + "Transmitting: " + QString::number(transmitting) + "; "
                + "Decoding: "     + QString::number(decoding) + "; "
                + "RxDF: "         + QString::number(rx_df) + "; "
                + "TxDF: "         + QString::number(tx_df) + "; "
                + "DECall: "       + de_call + "; "
                + "DEGrid: "       + de_grid + "; "
                + "DXGrid: "       + dx_grid + "; "
                + "TXWatchdog: "   + QString::number(tx_watchdog) + "; "
                + "SubMode: "      + sub_mode + "; "
                + "FastMode: "     + QString::number(fast_mode) + "; "
                + "SpecOpMode: "   + QString::number(special_op_mode) + "; "
                + "FreqTolerance: "+ QString::number(freq_tolerance) + "; "
                + "TRPeriod: "     + QString::number(tr_period) + "; "
                + "ConfName: "     + conf_name + "; "
                + "TXMessage: "    + tx_message + "; "
                + ")";
    }
};

#endif // QLOG_DATA_WSJTXSTATUS_H
