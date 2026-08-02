#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
#include <QTimeZone>
#endif
#include "PSKReporter.h"
#include "core/debug.h"
#include "rig/macros.h"
#include "data/BandPlan.h"
#include "data/Data.h"

MODULE_IDENTIFICATION("qlog.core.pskreporter");

PSKReporter::PSKReporter(QObject *parent)
    : QObject(parent)
{
    FCT_IDENTIFICATION;
}

void PSKReporter::setSubscription(const QString &callsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign;

    const QString newCallsign = callsign.trimmed().toUpper();

    if ( subscriptionCallsign == newCallsign )
        return;

    subscriptionCallsign = newCallsign;
    emit subscriptionChanged(subscriptionCallsign);
}

void PSKReporter::clearSubscription()
{
    FCT_IDENTIFICATION;

    setSubscription(QString());
}

void PSKReporter::processMessage(PSKReporter::Direction direction,
                                 const QString &topic,
                                 const QString &payload)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << ((direction == Direction::ReceivedBy) ? "ReceivedBy" : "SentBy")
                                 << topic
                                 << payload;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload.toUtf8(),
                                                            &parseError);
    if ( parseError.error != QJsonParseError::NoError
         || !document.isObject() )
    {
        qCWarning(runtime) << "Invalid PSK Reporter message:" << parseError.errorString();
        return;
    }

    const QJsonObject message = document.object();
    PskDecode decode;
    decode.sequenceNumber = message.value(QStringLiteral("sq")).toVariant().toULongLong();
    decode.frequency = Hz2MHz(message.value(QStringLiteral("f")).toVariant().toULongLong());
    decode.mode = message.value(QStringLiteral("md")).toString();
    decode.report = message.value(QStringLiteral("rp")).toInt();    
    decode.timestamp = QDateTime::fromSecsSinceEpoch(message.value(QStringLiteral("t")).toVariant().toLongLong(),
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
                                                     QTimeZone::UTC);
#else
                                                     Qt::UTC);
#endif

    decode.transmitterTimestamp = QDateTime::fromSecsSinceEpoch(message.value(QStringLiteral("t_tx")).toVariant().toLongLong(),
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
                                                                QTimeZone::UTC);
#else
                                                                Qt::UTC);
#endif

    decode.senderCallsign = message.value(QStringLiteral("sc")).toString();
    decode.senderLocator = message.value(QStringLiteral("sl")).toString();
    decode.receiverCallsign = message.value(QStringLiteral("rc")).toString();
    decode.receiverLocator = message.value(QStringLiteral("rl")).toString();
    decode.senderDxcc = message.value(QStringLiteral("sa")).toInt();
    decode.receiverDxcc = message.value(QStringLiteral("ra")).toInt();
    decode.band = message.value(QStringLiteral("b")).toString();

#if 0 // Normal operation: show stations which heard this callsign.
    if ( direction != Direction::SentBy )
        return;
#else // Test operation: show stations heard by this callsign.
    if ( direction != Direction::ReceivedBy )
        return;
#endif

    const Band bandDecoded = BandPlan::freq2Band(decode.frequency);
    const bool sentBy = (direction == Direction::SentBy);
    const QString &remoteCallsign = sentBy ? decode.receiverCallsign
                                           : decode.senderCallsign;
    const qint32 remoteDxcc = sentBy ? decode.receiverDxcc
                                     : decode.senderDxcc;

    if ( bandDecoded.name.isEmpty()
         || remoteCallsign.isEmpty() )
        return;

    decode.dupeCount = Data::countDupe(remoteCallsign,
                                       bandDecoded.name,
                                       BandPlan::MODE_GROUP_STRING_DIGITAL);
    decode.status = Data::instance()->dxccStatus(remoteDxcc,
                                                 bandDecoded.name,
                                                 BandPlan::MODE_GROUP_STRING_DIGITAL);

    qCDebug(runtime) << decode;
    emit heardMePointRequested(decode, direction);
}
