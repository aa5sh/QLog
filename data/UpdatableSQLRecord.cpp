#include <QSqlField>

#include "UpdatableSQLRecord.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.core.updatableqslrecord");

UpdatableSQLRecord::UpdatableSQLRecord(int interval, int maxUpdates, QObject *parent)
    : QObject{parent},
      interval(qMax(1, interval)),
      maxUpdates(qMax(1, maxUpdates)),
      currUpdateCnt(0)
{
    FCT_IDENTIFICATION;

    internalRecord.clear();
    timer.setSingleShot(true);
    timer.setInterval(interval);
    connect(&timer, &QTimer::timeout, this, &UpdatableSQLRecord::emitStoreRecord);
}

UpdatableSQLRecord::~UpdatableSQLRecord()
{
    FCT_IDENTIFICATION;

    timer.stop();
}

void UpdatableSQLRecord::updateRecord(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    if ( internalRecord.isEmpty() )
    {
        qCDebug(runtime) << "Internal record is empty, storing new record";

        internalRecord = record;
        startNextUpdateCycle();
        return;
    }

    if ( matchQSO(QSOMatchingType, record) )
    {
        qCDebug(runtime) << "Records match, merging values";

        timer.stop();

        // merge
        for ( int i = 0; i < record.count(); ++i )
        {
            const QString &fieldName = record.fieldName(i);

            if ( !internalRecord.contains(fieldName) )
                internalRecord.append(record.field(i));
            else if ( !record.value(i).toString().isEmpty()
                      && internalRecord.value(fieldName).toString().isEmpty() )
                internalRecord.setValue(fieldName, record.value(i));
        }

        startNextUpdateCycle();
        return;
    }

    qCDebug(runtime) << "Records do not match, emitting current record";

    emitStoreRecord();

    internalRecord = record;
    startNextUpdateCycle();
}

void UpdatableSQLRecord::emitStoreRecord()
{
    FCT_IDENTIFICATION;

    timer.stop();
    resetCnt();

    if ( internalRecord.isEmpty() )
        return;

    qCDebug(runtime) << "emitting record";
    emit recordReady(internalRecord);
    internalRecord.clear();
}

bool UpdatableSQLRecord::matchQSO(const MatchingType matchingType,
                                  const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    const QStringList &fields = matchingFields.value(matchingType);

    for ( const QString &fieldName : fields )
    {
        qCDebug(runtime) << "compare field name " << fieldName
                         << "In value" << internalRecord.value(fieldName)
                         << "New value" << record.value(fieldName);

        if ( internalRecord.value(fieldName) != record.value(fieldName))
            return false;
    }

    return true;
}

void UpdatableSQLRecord::resetCnt()
{
    FCT_IDENTIFICATION;

    timer.stop();
    currUpdateCnt = 0;
}

bool UpdatableSQLRecord::emitIfMaxUpdatesReached()
{
    FCT_IDENTIFICATION;

    if (currUpdateCnt < maxUpdates)
        return false;

    qCDebug(runtime) << "Maximum number of updates reached - emitting";
    emitStoreRecord();
    return true;
}

void UpdatableSQLRecord::startNextUpdateCycle()
{
    FCT_IDENTIFICATION;

    incrementCnt();

    if ( emitIfMaxUpdatesReached() ) return;

    qCDebug(runtime) << "Starting timer" << interval;
    timer.start();
}
