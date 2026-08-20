#include "QSLManager.h"

#include "core/LogParam.h"
#include "core/debug.h"
#include "service/qslinfo/QSLInfo.h"

MODULE_IDENTIFICATION("qlog.service.qslmanager");

const QString QSLManager::NO_SOURCE = QStringLiteral("none");

QSLManager::QSLManager(QObject *parent) :
    QObject(parent)
{
    FCT_IDENTIFICATION;

    queryTimer.setSingleShot(true);
    connect(&queryTimer, &QTimer::timeout, this, &QSLManager::queryTimedOut);

    initSource();
}

bool QSLManager::isActive() const
{
    FCT_IDENTIFICATION;

    return !source.isNull();
}

void QSLManager::queryCallsign(const QString &callsign, const QString &fallback)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign << fallback;

    abortQuery();

    currentCallsign = callsign.trimmed().toUpper();
    fallbackQslVia = fallback;

    if ( currentCallsign.isEmpty() )
    {
        QSLQueryResult result;
        result.status = QSLQueryResult::Status::NotFound;
        finishQuery(result);
        return;
    }

    if ( source.isNull() )
    {
        qCDebug(runtime) << "QSL manager query is disabled for"
                         << currentCallsign;

        QSLQueryResult result;
        finishQuery(result);
        return;
    }

    qCDebug(runtime) << "Starting QSL manager query for"
                     << currentCallsign
                     << "using source"
                     << source->sourceId();

    queryTimer.start(QUERY_TIMEOUT_MS);
    source->queryCallsign(currentCallsign);
}

void QSLManager::initSource()
{
    FCT_IDENTIFICATION;

    abortQuery();

    delete source.data();

    const QString sourceId = LogParam::getQSLManagerSource(NO_SOURCE);
    source = createSource(sourceId);

    if ( !source.isNull() )
    {
        connect(source, &QSLManagerSource::queryFinished, this, &QSLManager::sourceQueryFinished);

        qCDebug(runtime) << "QSL manager source initialized:"
                         << source->sourceId();
    }
    else
        qCDebug(runtime) << "QSL manager is disabled; configured source:"
                         << sourceId;
}

void QSLManager::abortQuery()
{
    FCT_IDENTIFICATION;

    if ( !currentCallsign.isEmpty() )
        qCDebug(runtime) << "Aborting QSL manager query for"
                         << currentCallsign;

    queryTimer.stop();

    if ( !source.isNull() )
        source->abortQuery();

    currentCallsign.clear();
    fallbackQslVia.clear();
}

void QSLManager::sourceQueryFinished(const QSLQueryResult &result)
{
    FCT_IDENTIFICATION;

    if ( currentCallsign.isEmpty()
         || result.callsign.compare(currentCallsign, Qt::CaseInsensitive) != 0 )
    {
        qCDebug(runtime) << "Ignoring stale QSL manager result for"
                         << result.callsign;
        return;
    }

    finishQuery(result);
}

void QSLManager::queryTimedOut()
{
    FCT_IDENTIFICATION;

    if ( currentCallsign.isEmpty() )
        return;

    qCDebug(runtime) << "QSL manager query timed out for"
                     << currentCallsign;

    if ( !source.isNull() )
        source->abortQuery();

    QSLQueryResult result;
    result.status = QSLQueryResult::Status::Timeout;
    result.error = tr("QSL manager query timed out");
    finishQuery(result);
}

QSLManagerSource *QSLManager::createSource(const QString &sourceId)
{
    FCT_IDENTIFICATION;

    if ( sourceId == QSLInfo::SOURCE_ID )
        return new QSLInfo(this);

    if ( sourceId != NO_SOURCE )
        qCDebug(runtime) << "Unknown QSL manager source:" << sourceId;

    return nullptr;
}

void QSLManager::finishQuery(QSLQueryResult result)
{
    FCT_IDENTIFICATION;

    const QString callsign = currentCallsign;
    const QString fallback = fallbackQslVia;

    currentCallsign.clear();
    fallbackQslVia.clear();
    queryTimer.stop();

    result.callsign = callsign;

    if ( result.status != QSLQueryResult::Status::Found
         || result.qslVia.isEmpty() )
    {
        result.qslVia = fallback;

        if ( !fallback.isEmpty() )
            qCDebug(runtime) << "Using fallback QSL via for"
                             << callsign;
    }

    qCDebug(runtime) << "QSL manager query finished for"
                     << callsign
                     << "with status"
                     << static_cast<int>(result.status)
                     << "and QSL via"
                     << result.qslVia;

    emit queryFinished(result);
}
