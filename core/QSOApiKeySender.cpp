#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>

#include "QSOApiKeySender.h"
#include "QSOApiKeyQuery.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.core.qsoapikeysender");

QSOApiKeySender::QSOApiKeySender(QObject *parent) :
    QObject(parent)
{
    FCT_IDENTIFICATION;

    connect(&nam, &QNetworkAccessManager::finished, this, &QSOApiKeySender::onNetworkReply);
}

void QSOApiKeySender::QSOInserted(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    if (!getEnabled())
        return;

    const QString url = getURL();
    const QString apiKey = getAPIKey();

    if (url.isEmpty() || apiKey.isEmpty())
        return;

    sendQSO(url, apiKey, record);
}

void QSOApiKeySender::sendQSO(const QString &url, const QString &apiKey, const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << url << record;

    const QUrl endpoint(url);

    if (!endpoint.isValid() || endpoint.scheme().isEmpty())
    {
        emit sendFinished(false, tr("Invalid QSO API URL"));
        return;
    }

    pendingUrl = endpoint;
    pendingRecord = record;
    currentStage = Stage::Login;

    // Step 1 - login: the api key travels in the POST body only, never in
    // a URL/query string, so it cannot end up in a server access log.
    QNetworkRequest request(endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));

    QUrlQuery loginBody;
    loginBody.addQueryItem(QStringLiteral("apikey"), apiKey);

    nam.post(request, loginBody.query(QUrl::FullyEncoded).toUtf8());
}

void QSOApiKeySender::onNetworkReply(QNetworkReply *reply)
{
    FCT_IDENTIFICATION;

    reply->deleteLater();

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool ok = (reply->error() == QNetworkReply::NoError && httpStatus >= 200 && httpStatus < 300);

    if (currentStage == Stage::Login)
    {
        if (!ok)
        {
            qCDebug(runtime) << "QSO API login failed" << httpStatus << reply->errorString();
            emit sendFinished(false, tr("API key login failed: %1").arg(reply->errorString()));
            return;
        }

        // Step 2 - upload: the QSO itself, as a plain GET. No api key here
        // - if the server answered step 1 with a session cookie,
        // QNetworkAccessManager's default cookie jar attaches it
        // automatically, exactly like a browser would after a login form.
        currentStage = Stage::Upload;

        QUrl uploadUrl(pendingUrl);
        QUrlQuery query;
        const auto params = QSOApiKeyQuery::buildParams(pendingRecord);
        for (const auto &p : params)
            query.addQueryItem(p.first, p.second);
        uploadUrl.setQuery(query);

        nam.get(QNetworkRequest(uploadUrl));
        return;
    }

    qCDebug(runtime) << "QSO API upload finished" << ok << httpStatus << reply->errorString();
    emit sendFinished(ok, ok ? tr("QSO sent")
                             : tr("QSO upload failed: %1").arg(reply->errorString()));
}
