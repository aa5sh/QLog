#include "QSLInfo.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.service.qslinfo");

const QString QSLInfoCallbook::CALLBOOK_NAME = QStringLiteral("qslinfo");

QSLInfoCallbook::QSLInfoCallbook(QObject *parent)
    : GenericCallbook(parent),
      currentReply(nullptr)
{
    FCT_IDENTIFICATION;
}

QSLInfoCallbook::~QSLInfoCallbook()
{
    FCT_IDENTIFICATION;

    if ( currentReply )
        currentReply->abort();
}

QString QSLInfoCallbook::getDisplayName()
{
    FCT_IDENTIFICATION;

    return tr("QSLInfo.de");
}

void QSLInfoCallbook::queryCallsign(const QString &callsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign;

    if ( currentReply )
    {
        qCWarning(runtime) << "processing a new request but the previous one hasn't been completed yet";
        currentReply->abort();
        currentReply = nullptr;
    }

    const QString normalizedCallsign = callsign.trimmed().toUpper();
    if ( normalizedCallsign.isEmpty() )
    {
        qCWarning(runtime) << "Cannot query QSLInfo.de with an empty callsign";
        emit callsignNotFound(callsign);
        return;
    }

    // Encode portable callsigns as one path component instead of treating '/'
    // as an API path separator.
    const QString encodedCallsign = QString::fromLatin1(
        QUrl::toPercentEncoding(normalizedCallsign));
    const QUrl url(API_URL + encodedCallsign, QUrl::StrictMode);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("QLog"));
    request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));

    qCDebug(runtime) << "Sending QSLInfo.de callsign request" << url;

    currentReply = getNetworkAccessManager()->get(request);
    // Preserve the caller's spelling for result correlation while using the
    // normalized value only in the HTTP request.
    currentReply->setProperty("queryCallsign", callsign.trimmed());
}

void QSLInfoCallbook::abortQuery()
{
    FCT_IDENTIFICATION;

    if ( currentReply )
    {
        qCDebug(runtime) << "Aborting QSLInfo.de request";
        currentReply->abort();
        currentReply = nullptr;
    }
}

void QSLInfoCallbook::processReply(QNetworkReply *reply)
{
    FCT_IDENTIFICATION;

    if ( reply == currentReply )
        currentReply = nullptr;

    if ( !reply )
    {
        qCWarning(runtime) << "QSLInfo.de returned no network reply";
        return;
    }

    const QString callsign = reply->property("queryCallsign").toString();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    qCDebug(runtime) << "QSLInfo.de response"
                     << reply->request().url() << "HTTP status" << status;

    if ( status == 404 )
    {
        qCDebug(runtime) << "QSLInfo.de has no entry for" << callsign;
        emit callsignNotFound(callsign);
        reply->deleteLater();
        return;
    }

    if ( reply->error() != QNetworkReply::NoError
         || status < 200
         || status >= 300 )
    {
        qCWarning(runtime) << "QSLInfo.de request failed"
                           << reply->errorString() << "HTTP status" << status;

        if ( reply->error() != QNetworkReply::OperationCanceledError )
            emit lookupError(reply->errorString());

        reply->deleteLater();
        return;
    }

    const QByteArray response = reply->readAll();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);

    if ( parseError.error != QJsonParseError::NoError || !document.isArray() )
    {
        qCWarning(runtime) << "QSLInfo.de JSON parse error"
                           << parseError.errorString();
        emit lookupError(tr("Invalid JSON response"));
        reply->deleteLater();
        return;
    }

    const QJsonArray entries = document.array();
    QString qslInformation;

    // Use the first usable information entry returned by the API as the
    // QSL Via override.
    for ( const QJsonValue &value : entries )
    {
        if ( !value.isObject() )
            continue;

        const QJsonObject object = value.toObject();
        const QString info = decodeHtmlEntities(
            object.value(QStringLiteral("info")).toString());

        if ( !info.isEmpty() )
        {
            qslInformation = info;
            break;
        }
    }

    if ( qslInformation.isEmpty() )
    {
        qCDebug(runtime) << "QSLInfo.de returned no usable QSL information for"
                         << callsign << "entries" << entries.size();
        emit callsignNotFound(callsign);
        reply->deleteLater();
        return;
    }

    CallbookResponseData responseData;
    responseData.call = callsign;
    responseData.qsl_via = qslInformation;
    responseData.qsl_via_override = true;

    qCDebug(runtime) << "QSLInfo.de QSL manager result"
                     << responseData.call << responseData.qsl_via;

    emit callsignResult(responseData);
    reply->deleteLater();
}
