#include <QColor>
#include "AlertTableModel.h"
#include "core/debug.h"
#include "data/Data.h"
#include "rig/macros.h"

//-+ FREQ_MATCH_TOLERANCE MHz is OK when QLog evaluates the same spot freq
#define FREQ_MATCH_TOLERANCE 0.005

int AlertTableModel::rowCount(const QModelIndex&) const
{
    return alertList.count();
}

int AlertTableModel::columnCount(const QModelIndex&) const
{
    return 8;
}

QVariant AlertTableModel::data(const QModelIndex& index, int role) const
{
    AlertTableRecord selectedRecord = alertList.at(index.row());

    if (role == Qt::DisplayRole)
    {
        switch ( index.column() )
        {
        case COLUMN_RULENAME: return selectedRecord.ruleName.join(",");
        case COLUMN_CALLSIGN: return selectedRecord.alert.callsign;
        case COLUMN_FREQ: return QSTRING_FREQ(selectedRecord.alert.freq);
        case COLUMN_MODE: return selectedRecord.alert.modeGroupString;
        case COLUMN_UPDATED: return selectedRecord.counter;
        case COLUMN_LAST_UPDATE: return selectedRecord.alert.dateTime.toString(locale.formatTimeLongWithoutTZ());
        case COLUMN_LAST_COMMENT: return selectedRecord.alert.comment;
        case COLUMN_MEMBER: return selectedRecord.alert.memberList2StringList().join(",");
        default: return QVariant();
        }
    }
    else if ( index.column() == COLUMN_CALLSIGN && role == Qt::BackgroundRole )
    {
        return Data::statusToColor(selectedRecord.alert.status, selectedRecord.alert.dupeCount, QColor(Qt::transparent));
    }
    else if ( role == Qt::UserRole )
    {
        switch ( index.column() )
        {
        case COLUMN_FREQ: return data(index, Qt::DisplayRole).toDouble(); break;
        case COLUMN_UPDATED: return data(index, Qt::DisplayRole).toULongLong(); break;
        case COLUMN_LAST_UPDATE: return selectedRecord.alert.dateTime; break;
        default: return data(index, Qt::DisplayRole);
        }
    }

    return QVariant();
}

QVariant AlertTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) return QVariant();

    switch (section)
    {
    case COLUMN_RULENAME: return tr("Rule Name");
    case COLUMN_CALLSIGN: return tr("Callsign");
    case COLUMN_FREQ: return tr("Frequency");
    case COLUMN_MODE: return tr("Mode");
    case COLUMN_UPDATED: return tr("Updated");
    case COLUMN_LAST_UPDATE: return tr("Last Update");
    case COLUMN_LAST_COMMENT: return tr("Last Comment");
    case COLUMN_MEMBER: return tr("Member");
    default: return QVariant();
    }
}

void AlertTableModel::addAlert(SpotAlert entry)
{
    AlertTableRecord newRecord(entry);

    QMutexLocker locker(&alertListMutex);

    int spotIndex = alertList.indexOf(newRecord);

    if ( spotIndex >= 0)
    {
        /* QLog already contains the spot, update it */
        alertList[spotIndex].counter++;
        alertList[spotIndex].ruleName << entry.ruleName;
        alertList[spotIndex].ruleName.removeDuplicates();
        alertList[spotIndex].ruleName.sort();

        // QLog have WSJTX record and Spot from DXC is processed.
        // only change a limited number of fields to preserve the WSJTX Decode for an WSJTX application tuning
        if ( entry.source == SpotAlert::DXSPOT
             && alertList[spotIndex].alert.source == SpotAlert::WSJTXCQSPOT )
        {
            alertList[spotIndex].alert.comment = entry.comment;
            alertList[spotIndex].alert.spotter = entry.spotter;
            alertList[spotIndex].alert.dxcc_spotter = entry.dxcc_spotter;
        }
        else
        {
            alertList[spotIndex].alert = entry;
        }
        emit dataChanged(createIndex(spotIndex,0), createIndex(spotIndex,5));
    }
    else
    {
        /* New spot, insert it */
        beginInsertRows(QModelIndex(), 0, 0);
        alertList.prepend(newRecord);
        endInsertRows();
    }
}

void AlertTableModel::clear()
{
    QMutexLocker locker(&alertListMutex);
    beginResetModel();
    alertList.clear();
    endResetModel();
}

const AlertTableModel::AlertTableRecord AlertTableModel::getTableRecord(const QModelIndex &index)
{
    QMutexLocker locker(&alertListMutex);
    return alertList.at(index.row());
}

void AlertTableModel::aging(const int clear_interval_sec)
{
    if ( clear_interval_sec <= 0 ) return;

    QMutexLocker locker(&alertListMutex);

    QMutableListIterator<AlertTableRecord> alertIterator(alertList);

    beginResetModel();
    while ( alertIterator.hasNext() )
    {
        alertIterator.next();
        if ( alertIterator.value().alert.dateTime.addSecs(clear_interval_sec) <= QDateTime::currentDateTimeUtc() )
        {
            alertIterator.remove();
        }
    }
    endResetModel();
}

void AlertTableModel::resetDupe()
{
    QMutexLocker locker(&alertListMutex);

    beginResetModel();
    for ( AlertTableRecord &alert : alertList )
        alert.alert.dupeCount = 0;
    endResetModel();
}

void AlertTableModel::recalculateDupe()
{
    QMutexLocker locker(&alertListMutex);

    beginResetModel();
    for ( AlertTableRecord &alert : alertList )
    {
        SpotAlert &spotAlert = alert.alert;
        spotAlert.dupeCount = Data::countDupe(spotAlert.callsign,
                                              spotAlert.band,
                                              spotAlert.modeGroupString);
    }
    endResetModel();
}

void AlertTableModel::updateSpotsStatusWhenQSOAdded(const QSqlRecord &record)
{
    qint32 dxcc = record.value("dxcc").toInt();
    const QString &band = record.value("band").toString();
    const QString &dxccModeGroup = BandPlan::modeToDXCCModeGroup(record.value("mode").toString());
    const QString &callsign = record.value("callsign").toString();

    QMutexLocker locker(&alertListMutex);

    beginResetModel();
    for ( AlertTableRecord &alert : alertList )
    {
        SpotAlert &spot = alert.alert;

        spot.status = Data::dxccNewStatusWhenQSOAdded(spot.status,
                                             spot.dxcc.dxcc,
                                             spot.band,
                                             ( ( spot.modeGroupString == BandPlan::MODE_GROUP_STRING_FT8 ) ? BandPlan::MODE_GROUP_STRING_DIGITAL
                                                                                                        : dxccModeGroup ),
                                             dxcc,
                                             band,
                                             dxccModeGroup);
        if ( spot.callsign == callsign )
            spot.dupeCount = Data::dupeNewCountWhenQSOAdded(spot.dupeCount,
                                                            spot.band,
                                                            spot.modeGroupString,
                                                            band,
                                                            dxccModeGroup);
    }
    endResetModel();
}

void AlertTableModel::updateSpotsStatusWhenQSOUpdated(const QSqlRecord &)
{
    QMutexLocker locker(&alertListMutex);

    // at this point, we don't know if callsign has been changed or other field.
    // TODO: DXCC status
    // TODO: Dupe status update

    // beginResetModel();
    // for ( AlertTableRecord &alert : alertList )
    // {
    //     SpotAlert &spot = alert.alert;
    //     spot.dupeCount = Data::countDupe(spot.callsign, spot.band, spot.modeGroupString);
    // }
    // endResetModel();
}

void AlertTableModel::updateSpotsStatusWhenQSODeleted(const QSqlRecord &record)
{
    // Pay attention: this method is called before the QSO is added to contacts
    const QString &callsign = record.value("callsign").toString();
    const QString &band = record.value("band").toString();
    const QString &dxccModeGroup = BandPlan::modeToDXCCModeGroup(record.value("mode").toString());

    QMutexLocker locker(&alertListMutex);

    for ( AlertTableRecord &alert : alertList )
    {
        SpotAlert &spot = alert.alert;

        if ( spot.dupeCount && spot.callsign == callsign )
            spot.dupeCount = Data::dupeNewCountWhenQSODelected(spot.dupeCount,
                                                               spot.band,
                                                               spot.modeGroupString,
                                                               band,
                                                               dxccModeGroup);
    }
}

void AlertTableModel::updateSpotsDxccStatusWhenQSODeleted(const QSet<uint> &entities)
{
    if ( entities.isEmpty() )
        return;

    QMutexLocker locker(&alertListMutex);

    beginResetModel();
    for ( AlertTableRecord &alert : alertList  )
    {
        SpotAlert &spot = alert.alert;

        if ( !entities.contains(spot.dxcc.dxcc) )
            continue;

        spot.status = Data::instance()->dxccStatus(spot.dxcc.dxcc, spot.band, spot.modeGroupString);
    }
    endResetModel();
}

bool AlertTableModel::AlertTableRecord::operator==(const AlertTableRecord &spot) const
{
   return ( (spot.alert.callsign == this->alert.callsign)
            && (spot.alert.modeGroupString == this->alert.modeGroupString)
            && (qAbs(this->alert.freq - spot.alert.freq) <= FREQ_MATCH_TOLERANCE)
            );
}

AlertTableModel::AlertTableRecord::AlertTableRecord(const SpotAlert &spotAlert) :

    ruleName(spotAlert.ruleName),
    counter(0),
    alert(spotAlert)
{
}
