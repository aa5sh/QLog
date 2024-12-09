#include <QSqlQuery>
#include <QCache>

#include "LogParam.h"
#include "debug.h"

MODULE_IDENTIFICATION("qlog.core.logparam");

#define PARAMMUTEXLOCKER     qCDebug(runtime) << "Waiting for mutex"; \
                             QMutexLocker locker(&cacheMutex); \
                             qCDebug(runtime) << "Using logparam"

LogParam::LogParam(QObject *parent) :
    QObject(parent)
{
    FCT_IDENTIFICATION;
}

bool LogParam::setParam(const QString &name, const QVariant &value)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << name << " " << value;

    QSqlQuery query;

    if ( ! query.prepare("INSERT OR REPLACE INTO log_param (name, value) "
                         "VALUES (:nam, :val)") )
    {
        qWarning()<< "Cannot prepare insert parameter statement for parameter" << name;
        return false;
    }

    query.bindValue(":nam", name);
    query.bindValue(":val", value);

    PARAMMUTEXLOCKER;

    if ( !query.exec() )
    {
        qWarning() << "Cannot exec an insert parameter statement for" << name;
        return false;
    }

    localCache.insert(name, new QVariant(value));

    return true;
}

QVariant LogParam::getParam(const QString &name, const QVariant &defaultValue)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << name;

    PARAMMUTEXLOCKER;

    QVariant ret = defaultValue;
    QVariant *valueCached = localCache.object(name);

    if ( valueCached )
    {
        ret = *valueCached;
    }
    else
    {
        QSqlQuery query;

        if ( ! query.prepare("SELECT value "
                             "FROM log_param "
                             "WHERE name = :nam") )
        {
            qWarning()<< "Cannot prepare select parameter statement for" << name;
            return QString();
        }

        query.bindValue(":nam", name);

        if ( ! query.exec() )
        {
            qWarning() << "Cannot execute GetParam Select for" << name;
            return QString();
        }

        if ( query.first() )
        {
            ret = query.value(0);
            localCache.insert(name, new QVariant(ret));
        }
    }
    qDebug(runtime) << "value: " << ret;
    return ret;
}

void LogParam::removeParamGroup(const QString &paramGroup)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << paramGroup;

    PARAMMUTEXLOCKER;

    const QStringList &keys = localCache.keys();

    for ( const QString& key : keys )
    {
        if (key.startsWith(paramGroup))
            localCache.remove(key);
    }

    QSqlQuery query;

    if ( ! query.prepare("DELETE FROM log_param WHERE name LIKE :group ") )
    {
        qWarning()<< "Cannot prepare delete parameter statement for" << paramGroup;
        return;
    }

    query.bindValue(":group", paramGroup + "%");

    if ( ! query.exec() )
        qWarning() << "Cannot execute removeParamGroup statement for" << paramGroup;

    return;
}

QCache<QString, QVariant> LogParam::localCache(30);

QMutex LogParam::cacheMutex;

#undef PARAMMUTEXLOCKER
