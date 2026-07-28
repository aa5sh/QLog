#include <QCache>
#include "CallbookManager.h"
#include "core/debug.h"
#include "service/hamqth/HamQTH.h"
#include "service/qrzcom/QRZ.h"
#include "service/qslinfo/QSLInfo.h"
#include "data/Callsign.h"
#include "LogParam.h"

MODULE_IDENTIFICATION("qlog.ui.callbookmanager");

CallbookManager::CallbookManager(QObject *parent)
    : QObject{parent},
    primaryCallbookAuthSuccess(false),
    secondaryCallbookAuthSuccess(false),
    pendingCallbookResultAvailable(false),
    waitingForQSLInfo(false)
{
    FCT_IDENTIFICATION;

    initCallbooks();
}

void CallbookManager::queryCallsign(const QString &callsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign;

    if ( queryCache.contains(callsign) )
    {
        emit callsignResult(CallbookResponseData(*queryCache.object(callsign)));
        return;
    }

    // create an empty object in cache
    // if there is the second query for the same call immediately after
    // the first query, then it returns a result of empty object
    queryCache.insert(callsign, new CallbookResponseData);
    currentQueryCallsign = callsign;

    if ( !primaryCallbook.isNull() )
    {
        primaryCallbook->queryCallsign(currentQueryCallsign);
    }
    else if ( !qslInfoCallbook.isNull() )
    {
        CallbookResponseData data;
        data.call = callsign;
        requestQSLInfo(data, false);
    }
    else
    {
        queryCache.remove(callsign);
        emit callsignNotFound(callsign);
    }
}

bool CallbookManager::isActive()
{
    FCT_IDENTIFICATION;

    bool ret = primaryCallbookAuthSuccess
               || secondaryCallbookAuthSuccess
               || !qslInfoCallbook.isNull();
    qCDebug(runtime) << ret;
    return ret;
}

GenericCallbook *CallbookManager::createCallbook(const QString &callbookID)
{
    FCT_IDENTIFICATION;

    GenericCallbook *ret = nullptr;

    if (callbookID == HamQTHCallbook::CALLBOOK_NAME )
    {
        ret = new HamQTHCallbook(this);
    }
    else if ( callbookID == QRZCallbook::CALLBOOK_NAME )
    {
        ret = new QRZCallbook(this);
    }

    if ( ret )
    {
        connect(ret, &GenericCallbook::callsignResult, this, &CallbookManager::processCallsignResult);
    }

    return ret;
}

void CallbookManager::initCallbooks()
{
    FCT_IDENTIFICATION;

    disposeCallbook(primaryCallbook);
    disposeCallbook(secondaryCallbook);
    disposeCallbook(qslInfoCallbook);

    primaryCallbookAuthSuccess = false;
    secondaryCallbookAuthSuccess = false;
    pendingCallbookData = CallbookResponseData();
    pendingCallbookResultAvailable = false;
    waitingForQSLInfo = false;

    queryCache.clear();
    currentQueryCallsign = QString();

    primaryCallbook = createCallbook(LogParam::getPrimaryCallbook(GenericCallbook::CALLBOOK_NAME));
    secondaryCallbook = createCallbook(LogParam::getSecondaryCallbook(GenericCallbook::CALLBOOK_NAME));
    if ( LogParam::getQSLInfoCallbookEnabled() )
        qslInfoCallbook = new QSLInfoCallbook(this);

    qCDebug(runtime) << "Initializing callbooks"
                     << "primary" << (primaryCallbook ? primaryCallbook->getDisplayName() : QStringLiteral("disabled"))
                     << "secondary" << (secondaryCallbook ? secondaryCallbook->getDisplayName() : QStringLiteral("disabled"))
                     << "QSLInfo.de" << !qslInfoCallbook.isNull();

    if ( !primaryCallbook.isNull() )
    {
        connect(primaryCallbook, &GenericCallbook::callsignNotFound, this, &CallbookManager::primaryCallbookCallsignNotFound);
        connect(primaryCallbook, &GenericCallbook::loginFailed, this, [this]()
        {
            primaryCallbookAuthSuccess = false;
            emit loginFailed(primaryCallbook->getDisplayName());
        });
        connect(primaryCallbook, &GenericCallbook::lookupError, this, [this](const QString &error)
        {
            emit lookupError(primaryCallbook->getDisplayName()
                             + " - " + error
                             + ((!secondaryCallbook.isNull()) ? tr("<p>The secondary callbook will be used</p>") : ""));
            primaryCallbookCallsignNotFound(currentQueryCallsign);
        });
        primaryCallbookAuthSuccess = true;
    }

    if ( !secondaryCallbook.isNull() )
    {
        connect(secondaryCallbook, &GenericCallbook::callsignNotFound, this, &CallbookManager::secondaryCallbookCallsignNotFound);
        connect(secondaryCallbook, &GenericCallbook::loginFailed, this, [this]()
        {
            secondaryCallbookAuthSuccess = false;
            emit loginFailed(secondaryCallbook->getDisplayName());
        });
        connect(secondaryCallbook, &GenericCallbook::lookupError, this, [this](const QString &error)
        {
            emit lookupError(secondaryCallbook->getDisplayName() + " - " + error);
            secondaryCallbookCallsignNotFound(currentQueryCallsign);
        });
        secondaryCallbookAuthSuccess = true;
    }

    if ( !qslInfoCallbook.isNull() )
    {
        connect(qslInfoCallbook, &GenericCallbook::callsignResult,
                this, &CallbookManager::processQSLInfoResult);
        connect(qslInfoCallbook, &GenericCallbook::callsignNotFound,
                this, &CallbookManager::qslInfoCallsignNotFound);
        connect(qslInfoCallbook, &GenericCallbook::lookupError,
                this, &CallbookManager::qslInfoLookupError);
    }
}

void CallbookManager::abortQuery()
{
    FCT_IDENTIFICATION;

    queryCache.remove(currentQueryCallsign);
    if ( ! primaryCallbook.isNull() ) primaryCallbook->abortQuery();
    if ( ! secondaryCallbook.isNull() ) secondaryCallbook->abortQuery();
    if ( ! qslInfoCallbook.isNull() ) qslInfoCallbook->abortQuery();
    pendingCallbookData = CallbookResponseData();
    pendingCallbookResultAvailable = false;
    waitingForQSLInfo = false;
}

void CallbookManager::primaryCallbookCallsignNotFound(const QString &notFoundCallsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << notFoundCallsign;

    queryCache.remove(notFoundCallsign);

    if ( notFoundCallsign != currentQueryCallsign ) return ;

    if ( secondaryCallbook.isNull() )
    {
        completeCallsignNotFound(notFoundCallsign);
        return;
    }

    qCDebug(runtime) << "Callsign not found - primary - trying the secondary callbook";

    secondaryCallbook->queryCallsign(notFoundCallsign);
}

void CallbookManager::secondaryCallbookCallsignNotFound(const QString &notFoundCallsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << notFoundCallsign;

    queryCache.remove(notFoundCallsign);

    if ( notFoundCallsign != currentQueryCallsign ) return ;

    qCDebug(runtime) << "Callsign not found - secondary ";

    completeCallsignNotFound(notFoundCallsign);
}

void CallbookManager::processCallsignResult(const CallbookResponseData &data)
{
    FCT_IDENTIFICATION;

    // Callbook returned queried callsign
    if ( data.call == currentQueryCallsign )
    {
        requestQSLInfo(data, true);
        return;
    }

    Callsign queryCall(currentQueryCallsign);

    if ( ! queryCall.isValid() )
    {
        qCDebug(runtime) << "Query callsign is not valid " << currentQueryCallsign;
        return;
    }

    // If not exists full match record for example for SP/OK1xxx in a callbook
    // then callbooks return a partial result (usually base callsign) OK1xxx. In this case, QLog
    // takes only selected fields from the callbook response.

    if ( queryCall.getBase() == data.call )
    {
        qCDebug(runtime) << "Partial match for result - forwarding limited set of information";

        CallbookResponseData newData;

        newData.call = currentQueryCallsign;
        newData.fname = data.fname;
        newData.lname = data.lname;
        newData.lic_year = data.lic_year;
        newData.qsl_via = data.qsl_via;
        newData.email = data.email;
        newData.born = data.born;
        newData.url = data.url;
        newData.name_fmt = data.name_fmt;
        newData.nick = data.nick;
        newData.image_url = data.image_url;

        requestQSLInfo(newData, true);
    }
}

void CallbookManager::requestQSLInfo(const CallbookResponseData &data,
                                     bool baseResultAvailable)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << data.call << baseResultAvailable;

    if ( qslInfoCallbook.isNull() )
    {
        if ( baseResultAvailable )
            completeCallsignResult(data);
        else
            completeCallsignNotFound(data.call);
        return;
    }

    pendingCallbookData = data;
    pendingCallbookResultAvailable = baseResultAvailable;
    waitingForQSLInfo = true;

    qCDebug(runtime) << "Querying QSLInfo.de supplemental QSL manager for"
                     << currentQueryCallsign;
    qslInfoCallbook->queryCallsign(currentQueryCallsign);
}

void CallbookManager::processQSLInfoResult(const CallbookResponseData &data)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << data.call << data.qsl_via;

    if ( !waitingForQSLInfo || data.call != currentQueryCallsign )
    {
        qCDebug(runtime) << "Ignoring stale QSLInfo.de result for" << data.call;
        return;
    }

    if ( !data.qsl_via.isEmpty() )
    {
        qCDebug(runtime) << "QSLInfo.de overrides QSL Via for"
                         << currentQueryCallsign << data.qsl_via;
        pendingCallbookData.qsl_via = data.qsl_via;
        pendingCallbookData.qsl_via_override = true;
    }

    pendingCallbookData.call = currentQueryCallsign;
    const CallbookResponseData result = pendingCallbookData;
    waitingForQSLInfo = false;
    pendingCallbookResultAvailable = false;
    pendingCallbookData = CallbookResponseData();
    completeCallsignResult(result);
}

void CallbookManager::qslInfoCallsignNotFound(const QString &callsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign;

    if ( !waitingForQSLInfo || callsign != currentQueryCallsign )
        return;

    const bool baseResultAvailable = pendingCallbookResultAvailable;
    const CallbookResponseData result = pendingCallbookData;
    waitingForQSLInfo = false;
    pendingCallbookResultAvailable = false;
    pendingCallbookData = CallbookResponseData();

    if ( baseResultAvailable )
        completeCallsignResult(result);
    else
    {
        queryCache.remove(callsign);
        emit callsignNotFound(callsign);
    }
}

void CallbookManager::qslInfoLookupError(const QString &error)
{
    FCT_IDENTIFICATION;

    qCWarning(runtime) << "QSLInfo.de supplemental lookup failed" << error;

    // A supplemental service outage must not discard or interrupt a valid
    // result from the primary/secondary callbook. Surface the error only when
    // QSLInfo.de is the sole provider for this lookup.
    if ( !pendingCallbookResultAvailable )
        emit lookupError(QStringLiteral("QSLInfo.de - ") + error);

    qslInfoCallsignNotFound(currentQueryCallsign);
}

void CallbookManager::completeCallsignResult(const CallbookResponseData &data)
{
    FCT_IDENTIFICATION;

    CallbookResponseData *cacheData = new CallbookResponseData;
    *cacheData = data;
    queryCache.insert(data.call, cacheData);
    emit callsignResult(data);
}

void CallbookManager::completeCallsignNotFound(const QString &callsign)
{
    FCT_IDENTIFICATION;

    queryCache.remove(callsign);

    if ( !qslInfoCallbook.isNull() )
    {
        CallbookResponseData data;
        data.call = callsign;
        requestQSLInfo(data, false);
        return;
    }

    emit callsignNotFound(callsign);
}

void CallbookManager::disposeCallbook(QPointer<GenericCallbook> &callbook)
{
    FCT_IDENTIFICATION;

    if ( callbook.isNull() )
        return;

    disconnect(callbook, nullptr, this, nullptr);
    callbook->abortQuery();
    callbook->deleteLater();
    callbook.clear();
}

QCache<QString, CallbookResponseData> CallbookManager::queryCache(100);
