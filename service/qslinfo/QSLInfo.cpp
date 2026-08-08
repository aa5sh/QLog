#include "QSLInfo.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTextDocument>
#include <QUrl>

#include "core/debug.h"
#include "data/Callsign.h"

MODULE_IDENTIFICATION("qlog.service.qslinfo");

/*
 * QSLInfo.de REST API:
 * https://www.qslinfo.de/api
 *
 * The official page documents the endpoint, but not the response fields.
 * The currently observed request and response format is:
 *
 * GET https://www.qslinfo.de/api/v1/call/OK1KPU
 *
 * HTTP 200 response body:
 *
 * [
 *   {
 *     "info": "OK1AXA",
 *     "meta": "UPDATED BY DL1SBF 2024-05-27 14:44",
 *     "call": "OK1KPU"
 *   }
 * ]
 *
 * One callsign may have multiple records, for example:
 *
 * GET https://www.qslinfo.de/api/v1/call/A71AH
 *
 * HTTP 200 response body:
 *
 * [
 *   {
 *     "info": "EA7FTR and LoTW",
 *     "meta": "UPDATED BY DL1SBF 2025-07-27 08:22",
 *     "call": "A71AH"
 *   },
 *   {
 *     "info": "A71AH:\r\nAbdulrahman Albinali\r\nAldafna Area 64 Alowina St. 957 Villa 13\r\nDoha, 974\r\nQatar",
 *     "meta": "UPDATED BY DL1SBF 2025-08-22 15:00",
 *     "call": "A71AH"
 *   }
 * ]
 *
 * Every object's "info" value is examined; the first object is not
 * automatically preferred. The first record identifies EA7FTR as the manager;
 * the second record is an address and is ignored. The resulting QSL_VIA is
 * therefore EA7FTR.
 *
 * An unknown callsign currently returns HTTP 200 with an empty array:
 *
 * []
 *
 * Only the unstructured "info" value is used to find the QSL manager. The
 * "meta" value is not searched. Examples:
 *
 * "info": "OK1AXA"            -> QSL manager is OK1AXA
 * "info": "QSL via EA7FTR"    -> QSL manager is EA7FTR
 * "info": "OK1KPU: address"   -> address, no QSL manager
 *
 * The callsign following "via", or the callsign at the beginning of "info",
 * is used. A new QSL_VIA value is applied only when all usable entries
 * identify the same external manager. Conflicting or unclear entries produce
 * no new QSL_VIA value.
 */

const QString QSLInfo::SOURCE_ID = QStringLiteral("qslinfo");

QSLInfo::QSLInfo(QObject *parent)
    : QSLManagerSource(parent),
      networkManager(new QNetworkAccessManager(this)),
      currentQueryIndex(-1)
{
    FCT_IDENTIFICATION;

    connect(networkManager, &QNetworkAccessManager::finished, this, &QSLInfo::processReply);
}

QSLInfo::~QSLInfo()
{
    FCT_IDENTIFICATION;

    abortCurrentQuery();
}

QString QSLInfo::sourceId() const
{
    FCT_IDENTIFICATION;

    return SOURCE_ID;
}

void QSLInfo::queryCallsign(const QString &callsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign;

    abortQuery();

    requestedCallsign = callsign.trimmed().toUpper();

    if ( requestedCallsign.isEmpty() )
    {
        QSLQueryResult result;
        result.status = QSLQueryResult::Status::NotFound;
        finish(result);
        return;
    }

    queryCallsigns.append(requestedCallsign);

    const Callsign parsedCallsign(requestedCallsign);
    if ( parsedCallsign.isValid()
         && parsedCallsign.getBase() != requestedCallsign )
    {
        queryCallsigns.append(parsedCallsign.getBase());
    }

    qCDebug(runtime) << "Prepared QSLInfo.de query for"
                     << requestedCallsign
                     << "using callsigns"
                     << queryCallsigns;

    startNextRequest();
}

void QSLInfo::abortQuery()
{
    FCT_IDENTIFICATION;

    abortCurrentQuery();
}

void QSLInfo::abortCurrentQuery()
{
    FCT_IDENTIFICATION;

    if ( !currentReply.isNull() )
    {
        qCDebug(runtime) << "Aborting QSLInfo.de request"
                         << currentReply->request().url();

        QNetworkReply *reply = currentReply.data();
        currentReply.clear();
        reply->abort();
    }

    requestedCallsign.clear();
    queryCallsigns.clear();
    currentQueryIndex = -1;
}

void QSLInfo::processReply(QNetworkReply *reply)
{
    FCT_IDENTIFICATION;

    if ( !reply )
    {
        qCWarning(runtime) << "Ignoring null QSLInfo.de reply";
        return;
    }

    if ( reply != currentReply.data()  )
    {
        qCDebug(runtime) << "Ignoring stale QSLInfo.de reply from"
                         << reply->request().url();
        reply->deleteLater();
        return;
    }

    currentReply.clear();

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qCDebug(runtime) << "QSLInfo.de reply received"
                     << " with HTTP status"
                     << httpStatus;

    if ( httpStatus == 404 )
    {
        qCDebug(runtime) << "QSLInfo.de callsign not found; trying fallback";
        reply->deleteLater();
        startNextRequest();
        return;
    }

    if ( reply->error() != QNetworkReply::NoError
         || httpStatus < 200
         || httpStatus >= 300 )
    {
        QSLQueryResult result;
        result.status = QSLQueryResult::Status::Error;
        result.error = reply->errorString();

        qCDebug(runtime) << "QSLInfo.de request failed"
                         << result.error;

        reply->deleteLater();
        finish(result);
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
    reply->deleteLater();

    if ( parseError.error != QJsonParseError::NoError
         || !document.isArray() )
    {
        QSLQueryResult result;
        result.status = QSLQueryResult::Status::Error;
        result.error = tr("Invalid JSON response");

        qCDebug(runtime) << "Invalid QSLInfo.de response "
                         << parseError.errorString();

        finish(result);
        return;
    }

    const QJsonArray entries = document.array();
    if ( entries.isEmpty() )
    {
        qCDebug(runtime) << "QSLInfo.de returned no entries; trying fallback";
        startNextRequest();
        return;
    }

    qCDebug(runtime) << "Resolving"
                     << entries.size()
                     << "QSLInfo.de entries for"
                     << queryCallsigns.at(currentQueryIndex);

    finish(resolveResponse(requestedCallsign,
                           queryCallsigns.at(currentQueryIndex),
                           entries));
}

const QUrl QSLInfo::queryUrl(const QString &callsign) const
{
    FCT_IDENTIFICATION;

    QUrl url(QStringLiteral("https://www.qslinfo.de/api/v1/call/"));
    url.setPath(url.path() + callsign);
    qCDebug(runtime) << url;
    return url;
}

void QSLInfo::startNextRequest()
{
    FCT_IDENTIFICATION;

    ++currentQueryIndex;

    if ( currentQueryIndex >= queryCallsigns.size() )
    {
        qCDebug(runtime) << "No QSLInfo.de data found for"
                         << requestedCallsign;

        QSLQueryResult result;
        result.status = QSLQueryResult::Status::NotFound;
        finish(result);
        return;
    }

    const QUrl url = queryUrl(queryCallsigns.at(currentQueryIndex));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("QLog"));
    request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));

    qCDebug(runtime) << "Querying QSLInfo.de" << url;
    currentReply = networkManager->get(request);
}

void QSLInfo::finish(QSLQueryResult result)
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "QSLInfo.de query finished for"
                     << requestedCallsign
                     << "with status"
                     << static_cast<int>(result.status)
                     << "and QSL via"
                     << result.qslVia;

    result.callsign = requestedCallsign;

    requestedCallsign.clear();
    queryCallsigns.clear();
    currentQueryIndex = -1;
    currentReply.clear();

    emit queryFinished(result);
}

QSLQueryResult QSLInfo::resolveResponse(const QString &requestedCallsign,
                                        const QString &responseCallsign,
                                        const QJsonArray &entries)
{
    FCT_IDENTIFICATION;

    QSLQueryResult result;

    const QString normalizedRequest = requestedCallsign.trimmed().toUpper();
    const QString normalizedResponse = responseCallsign.trimmed().toUpper();

    QSet<QString> queriedCallsigns;
    queriedCallsigns.insert(normalizedRequest);
    queriedCallsigns.insert(normalizedResponse);

    const Callsign parsedCallsign(normalizedRequest);

    if ( parsedCallsign.isValid() )
        queriedCallsigns.insert(parsedCallsign.getBase());

    QSet<QString> managerCallsigns;
    bool selfRoutingFound = false;
    bool unclassifiedInformationFound = false;
    bool mismatchedResponseFound = false;

    for ( const QJsonValue &value : entries )
    {
        if ( !value.isObject() )
        {
            unclassifiedInformationFound = true;
            continue;
        }

        const QJsonObject object = value.toObject();
        const QString entryCallsign =
                object.value(QStringLiteral("call")).toString().trimmed().toUpper();

        if ( !entryCallsign.isEmpty()
             && !queriedCallsigns.contains(entryCallsign) )
        {
            mismatchedResponseFound = true;
            continue;
        }

        const QString information = decodeInformation(
                    object.value(QStringLiteral("info")).toString());

        if ( information.isEmpty() )
            continue;

        if ( isAddressInformation(information, queriedCallsigns) )
            continue;

        const QString managerCallsign = extractManagerCallsign(information);
        if ( managerCallsign.isEmpty() )
        {
            unclassifiedInformationFound = true;
        }
        else if ( queriedCallsigns.contains(managerCallsign) )
        {
            selfRoutingFound = true;
        }
        else
        {
            managerCallsigns.insert(managerCallsign);
        }
    }

    if ( managerCallsigns.size() == 1
         && !selfRoutingFound
         && !unclassifiedInformationFound
         && !mismatchedResponseFound )
    {
        result.status = QSLQueryResult::Status::Found;
        result.qslVia = *managerCallsigns.constBegin();
    }
    else if ( managerCallsigns.isEmpty() )
        result.status = QSLQueryResult::Status::NotFound;
    else
        result.status = QSLQueryResult::Status::Ambiguous;

    qCDebug(runtime) << "Resolved QSLInfo.de response for"
                     << normalizedRequest
                     << "using"
                     << normalizedResponse
                     << "- entries:"
                     << entries.size()
                     << "managers:"
                     << managerCallsigns.size()
                     << "self routing:"
                     << selfRoutingFound
                     << "unclassified:"
                     << unclassifiedInformationFound
                     << "mismatched:"
                     << mismatchedResponseFound
                     << "status:"
                     << static_cast<int>(result.status)
                     << "QSL via:"
                     << result.qslVia;

    return result;
}

const QString QSLInfo::decodeInformation(const QString &information) const
{
    FCT_IDENTIFICATION;

    QTextDocument document;
    document.setHtml(information);
    return document.toPlainText().trimmed();
}

const QString QSLInfo::extractManagerCallsign(const QString &information) const
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << information;

    const QString normalizedInformation = information.simplified();
    const QString viaMarker = QStringLiteral("VIA ");

    int managerStart = 0;
    int viaPosition = normalizedInformation.indexOf(viaMarker, 0, Qt::CaseInsensitive);

    while ( viaPosition > 0
            && normalizedInformation.at(viaPosition - 1).isLetterOrNumber() )
    {
        viaPosition = normalizedInformation.indexOf(viaMarker, viaPosition + 1, Qt::CaseInsensitive);
    }

    if ( viaPosition >= 0 )
        managerStart = viaPosition + viaMarker.length();

    int managerLength = 0;
    while ( managerStart + managerLength < normalizedInformation.length() )
    {
        const QChar character = normalizedInformation.at(managerStart + managerLength);

        if ( !character.isLetterOrNumber() && character != QLatin1Char('/') )
            break;

        ++managerLength;
    }

    const QString candidate = normalizedInformation.mid(managerStart, managerLength).toUpper();

    return Callsign(candidate).isValid() ? candidate : QString();
}

bool QSLInfo::isAddressInformation(const QString &information,
                                   const QSet<QString> &queriedCallsigns) const
{
    FCT_IDENTIFICATION;

    const int colonPosition = information.indexOf(QLatin1Char(':'));

    if ( colonPosition < 0 )
        return false;

    const QString addressCallsign = information.left(colonPosition).trimmed().toUpper();

    return queriedCallsigns.contains(addressCallsign);
}
