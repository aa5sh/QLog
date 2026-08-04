#ifndef QLOG_DATA_PSKDECODE_H
#define QLOG_DATA_PSKDECODE_H

#include <QString>
#include <QDateTime>

#include "data/Dxcc.h"

class PskDecode
{
public:
    quint64 sequenceNumber = 0;
    double frequency = 0.0;
    QString mode;
    qint32 report = 0;
    QDateTime timestamp;
    QDateTime transmitterTimestamp;
    QString senderCallsign;
    QString senderLocator;
    QString receiverCallsign;
    QString receiverLocator;
    qint32 senderDxcc = 0;
    qint32 receiverDxcc = 0;
    QString band;
    DxccStatus status = DxccStatus::UnknownStatus;
    qulonglong dupeCount = 0;

    operator QString() const
    {
        return QStringLiteral("PskDecode: ")
                + QStringLiteral("(")
                + QStringLiteral("SequenceNumber: ")     + QString::number(sequenceNumber) + QStringLiteral("; ")
                + QStringLiteral("Frequency: ")          + QString::number(frequency, 'f', 6) + QStringLiteral("; ")
                + QStringLiteral("Mode: ")               + mode + QStringLiteral("; ")
                + QStringLiteral("Report: ")             + QString::number(report) + QStringLiteral("; ")
                + QStringLiteral("Timestamp: ")          + timestamp.toString() + QStringLiteral("; ")
                + QStringLiteral("TransmitterTimestamp: ") + transmitterTimestamp.toString() + QStringLiteral("; ")
                + QStringLiteral("SenderCallsign: ")     + senderCallsign + QStringLiteral("; ")
                + QStringLiteral("SenderLocator: ")      + senderLocator + QStringLiteral("; ")
                + QStringLiteral("ReceiverCallsign: ")   + receiverCallsign + QStringLiteral("; ")
                + QStringLiteral("ReceiverLocator: ")    + receiverLocator + QStringLiteral("; ")
                + QStringLiteral("SenderDxcc: ")         + QString::number(senderDxcc) + QStringLiteral("; ")
                + QStringLiteral("ReceiverDxcc: ")       + QString::number(receiverDxcc) + QStringLiteral("; ")
                + QStringLiteral("Band: ")               + band + QStringLiteral("; ")
                + QStringLiteral("Status: ")             + QString::number(status) + QStringLiteral("; ")
                + QStringLiteral("DupeCount: ")          + QString::number(dupeCount) + QStringLiteral("; ")
                + QStringLiteral(")");
    }
};

#endif // QLOG_DATA_PSKDECODE_H
