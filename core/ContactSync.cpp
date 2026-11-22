#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSettings>
#include <QUuid>
#include <QSaveFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QStringList>
#include <QTimer>
#include <QVariant>

#include "ContactSync.h"
#include "LogParam.h"
#include "debug.h"

MODULE_IDENTIFICATION("qlog.core.contactsync");

QString ContactSync::currentTimestamp()
{
    FCT_IDENTIFICATION;

    return QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss.zzzZ"));
}

QString ContactSync::journalFileName(int seq)
{
    FCT_IDENTIFICATION;

    return QStringLiteral("journal-%1.jsonl").arg(seq, 4, 10, QChar('0'));
}

// Strict journal-name match. Cloud clients create conflict copies such as
// "journal-0001 (Mike's conflicted copy).jsonl", which must never be read as
// part of the stream (they would also sort ahead of the real file).
const QRegularExpression &ContactSync::journalNameRegExp()
{
    FCT_IDENTIFICATION;

    static const QRegularExpression re(QStringLiteral("^journal-(\\d{4,})\\.jsonl$"));
    return re;
}

int ContactSync::journalSeq(const QString &fileName)
{
    FCT_IDENTIFICATION;

    const QRegularExpressionMatch match = journalNameRegExp().match(fileName);
    if ( !match.hasMatch() )
        return -1;
    bool ok = false;
    const int seq = match.captured(1).toInt(&ok);
    return ok ? seq : -1;
}

ContactSync *ContactSync::instance()
{
    static ContactSync inst;
    return &inst;
}

ContactSync::ContactSync(QObject *parent)
    : QObject(parent),
      timer(new QTimer(this)),
      currentSeq(1),
      currentBytes(0),
      busyFlag(0)
{
    FCT_IDENTIFICATION;

    timer->setInterval(FLUSH_INTERVAL_MS);
    connect(timer, &QTimer::timeout, this, &ContactSync::onTick);
}

//
// ---- main-thread getters/setters (use the default DB connection) ----
//

bool ContactSync::isEnabled() const
{
    FCT_IDENTIFICATION;

    return LogParam::getSyncEnabled();
}

QString ContactSync::folder() const
{
    FCT_IDENTIFICATION;

    return LogParam::getSyncFolder();
}

QString ContactSync::selfNodeId() const
{
    FCT_IDENTIFICATION;

    QSqlQuery q;
    if ( !q.exec("SELECT value FROM sync_runtime WHERE key = 'self_node_id'") )
        return LogParam::getLogID();
    if ( q.next() )
        return q.value(0).toString();
    return LogParam::getLogID();
}

bool ContactSync::isValidNodeId(const QString &nodeId)
{
    FCT_IDENTIFICATION;

    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$"));
    return re.match(nodeId).hasMatch();
}

bool ContactSync::setSelfNodeId(const QString &nodeId, QString *errorOut)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << nodeId;

    const QString trimmed = nodeId.trimmed();
    if ( trimmed.isEmpty() )
    {
        if ( errorOut ) *errorOut = tr("Node ID cannot be empty");
        return false;
    }
    if ( !isValidNodeId(trimmed) )
    {
        if ( errorOut ) *errorOut = tr("Node ID may contain only letters, digits, '.', '_' and '-', "
                                       "must start with a letter or digit, and be at most 64 characters");
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database();
    const QString previous = selfNodeId();

    if ( !db.transaction() )
    {
        if ( errorOut ) *errorOut = db.lastError().text();
        return false;
    }

    // Any failure below rolls back the whole rename.
    const auto fail = [&db, errorOut](const QSqlQuery &query)
    {
        if ( errorOut ) *errorOut = query.lastError().text();
        db.rollback();
        return false;
    };

    // Only this install's own rows are journaled ("origin_node = self"), so
    // its QSOs and deletions move to the new name. Otherwise renaming would
    // silently stop the existing log from ever being sent. The sync triggers
    // are paused so the move does not re-stamp updated_at.
    if ( !previous.isEmpty() && previous != trimmed )
    {
        if ( !setApplyingOnDb(db, true) )
        {
            if ( errorOut ) *errorOut = tr("cannot set sync apply flag");
            db.rollback();
            return false;
        }
        for ( const QString &table : { QStringLiteral("contacts"), QStringLiteral("contacts_tombstones") } )
        {
            QSqlQuery move(db);
            if ( !move.prepare(QStringLiteral("UPDATE %1 SET origin_node = :new WHERE origin_node = :old").arg(table)) )
                return fail(move);
            move.bindValue(QStringLiteral(":new"), trimmed);
            move.bindValue(QStringLiteral(":old"), previous);
            if ( !move.exec() )
                return fail(move);
            qCDebug(runtime) << "Moved" << move.numRowsAffected() << table << "rows from node" << previous << "to" << trimmed;
        }
        if ( !setApplyingOnDb(db, false) )
        {
            if ( errorOut ) *errorOut = tr("cannot clear sync apply flag");
            db.rollback();
            return false;
        }
    }

    QSqlQuery q(db);
    if ( !q.prepare("INSERT OR REPLACE INTO sync_runtime (key, value) VALUES (:k, :v)") )
        return fail(q);

    // Saving a Node ID is the user's explicit statement that this install
    // owns the identity, so it also (re)claims ownership.
    const QList<QPair<QString, QString>> rows = {
        { QStringLiteral("self_node_id"), trimmed },
        { QStringLiteral("owner_install_id"), installId() }
    };
    for ( const auto &row : rows )
    {
        q.bindValue(":k", row.first);
        q.bindValue(":v", row.second);
        if ( !q.exec() )
            return fail(q);
    }

    if ( !db.commit() )
    {
        if ( errorOut ) *errorOut = db.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}

QString ContactSync::installId()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    QString id = settings.value(QStringLiteral("sync/install_id")).toString();
    if ( id.isEmpty() )
    {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.setValue(QStringLiteral("sync/install_id"), id);
        settings.sync();
    }
    return id;
}

QString ContactSync::writerId() const
{
    FCT_IDENTIFICATION;

    return installId() + QLatin1Char('/') + LogParam::getLogID();
}

bool ContactSync::nodeIdOwnedElsewhere() const
{
    FCT_IDENTIFICATION;

    QSqlQuery q;
    if ( !q.exec("SELECT value FROM sync_runtime WHERE key = 'owner_install_id'") || !q.next() )
        return false;
    const QString owner = q.value(0).toString();
    return !owner.isEmpty() && owner != installId();
}

bool ContactSync::checkOwnership(QString *errorOut)
{
    FCT_IDENTIFICATION;

    QSqlQuery q;
    QString owner;
    if ( q.exec("SELECT value FROM sync_runtime WHERE key = 'owner_install_id'") && q.next() )
        owner = q.value(0).toString();

    if ( owner.isEmpty() )
    {
        // First sync on this database: claim it.
        QSqlQuery claim;
        if ( !claim.prepare("INSERT OR REPLACE INTO sync_runtime (key, value) "
                            "VALUES ('owner_install_id', :v)") )
        {
            if ( errorOut ) *errorOut = claim.lastError().text();
            return false;
        }
        claim.bindValue(":v", installId());
        if ( !claim.exec() )
        {
            if ( errorOut ) *errorOut = claim.lastError().text();
            return false;
        }
        return true;
    }

    if ( owner != installId() )
    {
        if ( errorOut ) *errorOut = tr("This logbook's sync Node ID (%1) belongs to a different QLog "
                                       "installation - the database was probably copied from another "
                                       "computer. Open Logbook > Contact Sync, enter a Node ID that is "
                                       "unique to this computer and press Save.").arg(cycleSelf);
        return false;
    }
    return true;
}

QDateTime ContactSync::lastFlushTime() const
{
    FCT_IDENTIFICATION;

    const QString stored = LogParam::getSyncLastFlush();
    if ( stored.isEmpty() )
        return QDateTime();
    return QDateTime::fromString(stored, Qt::ISODateWithMs);
}

QDateTime ContactSync::lastPullTime() const
{
    FCT_IDENTIFICATION;

    const QString stored = LogParam::getSyncLastPull();
    if ( stored.isEmpty() )
        return QDateTime();
    return QDateTime::fromString(stored, Qt::ISODateWithMs);
}

ContactSync::DuplicatePolicy ContactSync::duplicatePolicy() const
{
    FCT_IDENTIFICATION;

    return static_cast<DuplicatePolicy>(LogParam::getSyncDuplicatePolicy());
}

void ContactSync::setDuplicatePolicy(DuplicatePolicy policy)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << static_cast<int>(policy);

    LogParam::setSyncDuplicatePolicy(static_cast<int>(policy));
}

bool ContactSync::setEnabled(bool enabled, QString *errorOut)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << enabled;

    if ( enabled )
    {
        snapshotForCycle();
        if ( !ensureFolderLayout(errorOut) )
            return false;
    }
    LogParam::setSyncEnabled(enabled);
    if ( enabled )
        start();
    else
        stop();
    return true;
}

bool ContactSync::setFolder(const QString &path, QString *errorOut)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << path;

    LogParam::setSyncFolder(path);
    if ( isEnabled() )
    {
        snapshotForCycle();
        return ensureFolderLayout(errorOut);
    }
    return true;
}

void ContactSync::start()
{
    FCT_IDENTIFICATION;

    if ( !isEnabled() )
        return;
    if ( !timer->isActive() )
        timer->start();
}

void ContactSync::stop()
{
    FCT_IDENTIFICATION;

    timer->stop();
}

int ContactSync::pendingChangesCount() const
{
    FCT_IDENTIFICATION;

    if ( !isEnabled() )
        return 0;

    const QString self = selfNodeId();
    if ( self.isEmpty() )
        return 0;

    const QString upsertWm = LogParam::getSyncUpsertWatermark();
    const QString deleteWm = LogParam::getSyncDeleteWatermark();

    int total = 0;

    QString sql1 = QStringLiteral("SELECT COUNT(*) FROM contacts "
                                  "WHERE origin_node = :node "
                                  "  AND updated_at IS NOT NULL");
    if ( !upsertWm.isEmpty() )
        sql1 += QStringLiteral(" AND updated_at > :wm");

    QSqlQuery q1;
    if ( q1.prepare(sql1) )
    {
        q1.bindValue(":node", self);
        if ( !upsertWm.isEmpty() )
            q1.bindValue(":wm", upsertWm);
        if ( q1.exec() && q1.next() )
            total += q1.value(0).toInt();
    }

    QString sql2 = QStringLiteral("SELECT COUNT(*) FROM contacts_tombstones "
                                  "WHERE origin_node = :node");
    if ( !deleteWm.isEmpty() )
        sql2 += QStringLiteral(" AND deleted_at > :wm");

    QSqlQuery q2;
    if ( q2.prepare(sql2) )
    {
        q2.bindValue(":node", self);
        if ( !deleteWm.isEmpty() )
            q2.bindValue(":wm", deleteWm);
        if ( q2.exec() && q2.next() )
            total += q2.value(0).toInt();
    }

    return total;
}

//
// ---- folder layout / journal file helpers (no DB) ----
//

void ContactSync::snapshotForCycle()
{
    FCT_IDENTIFICATION;

    cycleFolder = folder();        // LogParam — main-thread safe
    cycleSelf   = selfNodeId();    // sync_runtime via default conn — main-thread safe
}

QString ContactSync::rootDir() const
{
    FCT_IDENTIFICATION;

    if ( cycleFolder.isEmpty() )
        return QString();
    return QDir(cycleFolder).filePath(SUBDIR_ROOT);
}

QString ContactSync::nodeDir() const
{
    FCT_IDENTIFICATION;

    const QString root = rootDir();
    if ( root.isEmpty() || cycleSelf.isEmpty() )
        return QString();
    return QDir(root).filePath(QStringLiteral("%1/%2").arg(SUBDIR_NODES, cycleSelf));
}

QString ContactSync::manifestPath() const
{
    FCT_IDENTIFICATION;

    const QString root = rootDir();
    return root.isEmpty() ? QString() : QDir(root).filePath(QStringLiteral("manifest.json"));
}

QString ContactSync::headPath() const
{
    FCT_IDENTIFICATION;

    const QString nd = nodeDir();
    return nd.isEmpty() ? QString() : QDir(nd).filePath(QStringLiteral("HEAD"));
}

QString ContactSync::journalPath(int seq) const
{
    FCT_IDENTIFICATION;

    const QString nd = nodeDir();
    return nd.isEmpty() ? QString() : QDir(nd).filePath(journalFileName(seq));
}

QString ContactSync::currentJournalPath() const
{
    FCT_IDENTIFICATION;

    return journalPath(currentSeq);
}

bool ContactSync::ensureFolderLayout(QString *errorOut)
{
    FCT_IDENTIFICATION;

    if ( cycleFolder.isEmpty() )
    {
        if ( errorOut ) *errorOut = tr("Sync folder is not set");
        return false;
    }
    if ( cycleSelf.isEmpty() )
    {
        if ( errorOut ) *errorOut = tr("Log ID is not set");
        return false;
    }

    if ( !checkOwnership(errorOut) )
        return false;

    QDir dir;
    if ( !dir.mkpath(nodeDir()) )
    {
        if ( errorOut ) *errorOut = tr("Cannot create sync folder: %1").arg(nodeDir());
        return false;
    }

    if ( !writeManifestIfMissing() )
    {
        if ( errorOut ) *errorOut = tr("Cannot write manifest");
        return false;
    }

    return loadHead(errorOut);
}

bool ContactSync::writeManifestIfMissing()
{
    FCT_IDENTIFICATION;

    const QString path = manifestPath();
    if ( path.isEmpty() )
        return false;
    if ( QFile::exists(path) )
        return true;

    QJsonObject obj;
    obj["version"] = JOURNAL_FORMAT_VERSION;
    obj["created_at"] = currentTimestamp();

    QSaveFile f(path);
    if ( !f.open(QIODevice::WriteOnly | QIODevice::Truncate) )
        return false;
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    return f.commit();
}

bool ContactSync::loadHead(QString *errorOut)
{
    FCT_IDENTIFICATION;

    const QString path = headPath();
    if ( path.isEmpty() )
    {
        if ( errorOut ) *errorOut = tr("Sync folder is not set");
        return false;
    }

    if ( !QFile::exists(path) )
    {
        // No HEAD (fresh node, or HEAD not synced down yet). Never restart at
        // journal-0001 if later journals already exist: peers that are past
        // that file would never see records appended to it.
        int maxSeq = 0;
        const QStringList existing = QDir(nodeDir()).entryList(QStringList() << "journal-*.jsonl",
                                                               QDir::Files);
        for ( const QString &name : existing )
            maxSeq = qMax(maxSeq, journalSeq(name));
        currentSeq = qMax(1, maxSeq);
        currentBytes = QFileInfo(currentJournalPath()).size();
        journalEpoch = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QFile create(currentJournalPath());
        if ( !create.open(QIODevice::WriteOnly | QIODevice::Append) )
        {
            if ( errorOut ) *errorOut = tr("Cannot create journal: %1").arg(create.errorString());
            return false;
        }
        create.close();
        if ( !writeHead() )
        {
            if ( errorOut ) *errorOut = tr("Cannot update HEAD");
            return false;
        }
        return true;
    }

    QFile f(path);
    if ( !f.open(QIODevice::ReadOnly) )
    {
        if ( errorOut ) *errorOut = tr("Cannot read HEAD: %1").arg(f.errorString());
        return false;
    }
    const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();

    // Two installs writing into the same node folder (same hostname, or a
    // copied database) would interleave their journals and silently skip
    // each other's changes. HEADs written before this check have no writer.
    const QString writer = obj.value("writer").toString();
    if ( !writer.isEmpty() && writer != writerId() )
    {
        if ( errorOut ) *errorOut = tr("The sync folder for Node ID '%1' is already used by another "
                                       "QLog installation. Open Logbook > Contact Sync and choose a "
                                       "Node ID that is unique to this computer.").arg(cycleSelf);
        return false;
    }

    const int parsed = journalSeq(obj.value("file").toString());
    currentSeq = (parsed >= 1) ? parsed : 1;
    currentBytes = QFileInfo(currentJournalPath()).size();

    journalEpoch = obj.value("epoch").toString();
    if ( journalEpoch.isEmpty() )
    {
        // HEAD from before epochs existed. Records already written carry no
        // epoch and stay readable; new ones get this one.
        journalEpoch = QUuid::createUuid().toString(QUuid::WithoutBraces);
        if ( !writeHead() )
        {
            if ( errorOut ) *errorOut = tr("Cannot update HEAD");
            return false;
        }
    }
    return true;
}

bool ContactSync::writeHead()
{
    FCT_IDENTIFICATION;

    const QString path = headPath();
    if ( path.isEmpty() )
        return false;

    QJsonObject obj;
    obj["file"] = journalFileName(currentSeq);
    obj["bytes"] = static_cast<qint64>(currentBytes);
    obj["writer"] = writerId();
    obj["epoch"] = journalEpoch;
    obj["updated_at"] = currentTimestamp();

    QSaveFile f(path);
    if ( !f.open(QIODevice::WriteOnly | QIODevice::Truncate) )
        return false;
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    return f.commit();
}

bool ContactSync::rotateIfNeeded()
{
    FCT_IDENTIFICATION;

    if ( currentBytes < MAX_JOURNAL_BYTES )
        return true;
    currentSeq += 1;
    currentBytes = 0;
    return true;
}

bool ContactSync::appendRecord(const QByteArray &line, QString *errorOut)
{
    FCT_IDENTIFICATION;

    if ( !rotateIfNeeded() )
    {
        if ( errorOut ) *errorOut = tr("Cannot rotate journal");
        return false;
    }

    QFile f(currentJournalPath());
    if ( !f.open(QIODevice::WriteOnly | QIODevice::Append) )
    {
        if ( errorOut ) *errorOut = tr("Cannot open journal: %1").arg(f.errorString());
        return false;
    }
    QByteArray payload = line;
    payload.append('\n');
    if ( f.write(payload) != payload.size() )
    {
        if ( errorOut ) *errorOut = tr("Short write to journal");
        return false;
    }
    currentBytes += payload.size();
    return true;
}

//
// ---- DB helpers (take the connection to use explicitly) ----
//
// Everything below takes `QSqlDatabase &db` and uses `QSqlQuery q(db)`. For
// now the dispatchers call these synchronously with the default connection,
// but the signature is set up so a future move back to a worker thread can
// pass in a per-call cloned connection without changing the call sites.
//

QSet<QString> ContactSync::contactColumnsOnDb(QSqlDatabase &db) const
{
    FCT_IDENTIFICATION;

    // Cache is per-process; the schema can't change at runtime.
    static QSet<QString> cache;
    static bool loaded = false;
    if ( !loaded )
    {
        QSqlQuery q(db);
        if ( q.exec(QStringLiteral("PRAGMA table_info(contacts)")) )
        {
            while ( q.next() )
                cache.insert(q.value(QStringLiteral("name")).toString());
            loaded = !cache.isEmpty();
        }
    }
    return cache;
}

bool ContactSync::flushOnDb(QSqlDatabase &db, int *upsertsOut, int *deletesOut,
                            QString *newUpsertWatermark,
                            QString *newDeleteWatermark, QString *errorOut)
{
    FCT_IDENTIFICATION;

    // Reads self and the current watermarks straight from the DB. New
    // watermark values are returned via the out-params and committed by the
    // caller through LogParam, so its cache stays coherent.

    QString self;
    {
        QSqlQuery q(db);
        if ( q.exec(QStringLiteral("SELECT value FROM sync_runtime "
                                   "WHERE key = 'self_node_id'")) && q.next() )
            self = q.value(0).toString();
    }

    auto readParam = [&db](const QString &name) -> QString
    {
        QSqlQuery q(db);
        if ( !q.prepare(QStringLiteral("SELECT value FROM log_param WHERE name = :n")) )
            return QString();
        q.bindValue(QStringLiteral(":n"), name);
        if ( !q.exec() || !q.next() ) return QString();
        return q.value(0).toString();
    };

    const QString upsertWm = readParam(QStringLiteral("sync/last_journaled_updated_at"));
    const QString deleteWm = readParam(QStringLiteral("sync/last_journaled_deleted_at"));

    // ---- upserts ----
    int upserts = 0;
    QString maxUpsert = upsertWm;
    {
        QString sql = QStringLiteral("SELECT * FROM contacts "
                                     "WHERE origin_node = :node "
                                     "  AND updated_at IS NOT NULL");
        if ( !upsertWm.isEmpty() )
            sql += QStringLiteral(" AND updated_at > :wm");
        sql += QStringLiteral(" ORDER BY updated_at ASC, id ASC");

        QSqlQuery q(db);
        if ( !q.prepare(sql) )
        {
            if ( errorOut ) *errorOut = q.lastError().text();
            return false;
        }
        q.bindValue(":node", self);
        if ( !upsertWm.isEmpty() )
            q.bindValue(":wm", upsertWm);
        if ( !q.exec() )
        {
            if ( errorOut ) *errorOut = q.lastError().text();
            return false;
        }

        while ( q.next() )
        {
            const QSqlRecord rec = q.record();
            QJsonObject payload;
            for ( int i = 0; i < rec.count(); ++i )
            {
                const QString name = rec.fieldName(i);
                if ( name == QLatin1String("id") )
                    continue;
                const QVariant v = rec.value(i);
                // Nulls are written explicitly so that clearing a field on
                // one node also clears it on its peers.
                payload.insert(name, v.isNull() ? QJsonValue(QJsonValue::Null)
                                                : QJsonValue::fromVariant(v));
            }

            QJsonObject record;
            record["v"]          = JOURNAL_FORMAT_VERSION;
            record["op"]         = QStringLiteral("upsert");
            record["epoch"]      = journalEpoch;
            record["qso_uuid"]   = rec.value("qso_uuid").toString();
            record["updated_at"] = rec.value("updated_at").toString();
            record["node"]       = self;
            record["payload"]    = payload;

            const QByteArray line = QJsonDocument(record).toJson(QJsonDocument::Compact);
            if ( !appendRecord(line, errorOut) )
                return false;

            const QString updatedAt = rec.value("updated_at").toString();
            if ( updatedAt > maxUpsert )
                maxUpsert = updatedAt;
            ++upserts;
        }
    }

    // ---- deletes ----
    int deletes = 0;
    QString maxDelete = deleteWm;
    {
        QString sql = QStringLiteral("SELECT qso_uuid, deleted_at, origin_node "
                                     "FROM contacts_tombstones "
                                     "WHERE origin_node = :node");
        if ( !deleteWm.isEmpty() )
            sql += QStringLiteral(" AND deleted_at > :wm");
        sql += QStringLiteral(" ORDER BY deleted_at ASC");

        QSqlQuery q(db);
        if ( !q.prepare(sql) )
        {
            if ( errorOut ) *errorOut = q.lastError().text();
            return false;
        }
        q.bindValue(":node", self);
        if ( !deleteWm.isEmpty() )
            q.bindValue(":wm", deleteWm);
        if ( !q.exec() )
        {
            if ( errorOut ) *errorOut = q.lastError().text();
            return false;
        }

        while ( q.next() )
        {
            QJsonObject record;
            record["v"]          = JOURNAL_FORMAT_VERSION;
            record["op"]         = QStringLiteral("delete");
            record["epoch"]      = journalEpoch;
            record["qso_uuid"]   = q.value("qso_uuid").toString();
            record["updated_at"] = q.value("deleted_at").toString();
            record["node"]       = self;

            const QByteArray line = QJsonDocument(record).toJson(QJsonDocument::Compact);
            if ( !appendRecord(line, errorOut) )
                return false;

            const QString deletedAt = q.value("deleted_at").toString();
            if ( deletedAt > maxDelete )
                maxDelete = deletedAt;
            ++deletes;
        }
    }

    if ( !writeHead() )
    {
        if ( errorOut ) *errorOut = tr("Cannot update HEAD");
        return false;
    }

    if ( upsertsOut ) *upsertsOut = upserts;
    if ( deletesOut ) *deletesOut = deletes;
    if ( newUpsertWatermark && upserts > 0 ) *newUpsertWatermark = maxUpsert;
    if ( newDeleteWatermark && deletes > 0 ) *newDeleteWatermark = maxDelete;

    return true;
}

bool ContactSync::setApplyingOnDb(QSqlDatabase &db, bool applying)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << applying;

    // Read by contacts_sync_update / contacts_record_tombstone. Only ever
    // set inside the pull transaction, so a crash rolls it back to '0'.
    QSqlQuery q(db);
    if ( !q.prepare(QStringLiteral("INSERT OR REPLACE INTO sync_runtime (key, value) "
                                   "VALUES ('applying', :v)")) )
        return false;
    q.bindValue(":v", applying ? QStringLiteral("1") : QStringLiteral("0"));
    return q.exec();
}

bool ContactSync::updateFromPayloadOnDb(QSqlDatabase &db, const QString &qsoUuid,
                                        const QJsonObject &payload, QString *errorOut)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << qsoUuid;

    // Upload bookkeeping (<service>_qso_upload_status/_date) is per node:
    // each install may upload to QRZ/ClubLog/... on its own. The main UPDATE
    // leaves it alone, so update_contacts_upload_status still flips a local
    // 'Y' to 'M' when the QSO content changed. Afterwards, any service the
    // peer reports as 'Y' is taken over: the peer's row is the newest
    // version, so the service already holds this exact content, and
    // re-uploading it from here would create a duplicate.
    const QSet<QString> cols = contactColumnsOnDb(db);

    QStringList sets;
    QVariantList binds;
    QStringList uploadedServices;
    for ( auto it = payload.constBegin(); it != payload.constEnd(); ++it )
    {
        const QString &col = it.key();
        if ( col == QLatin1String("id") || col == QLatin1String("qso_uuid") )
            continue;
        if ( !cols.contains(col) )
            continue;
        if ( isUploadStateColumn(col) )
        {
            if ( col.endsWith(QLatin1String("_qso_upload_status"))
                 && it.value().toString() == QLatin1String("Y") )
                uploadedServices << col.chopped(int(sizeof("_qso_upload_status")) - 1);
            continue;
        }
        sets << quoteIdent(col) + QStringLiteral(" = ?");
        binds << jsonToBind(it.value());
    }

    if ( !sets.isEmpty() )
    {
        QSqlQuery upd(db);
        if ( !upd.prepare("UPDATE contacts SET " + sets.join(", ") +
                          " WHERE qso_uuid = ?") )
        {
            if ( errorOut ) *errorOut = upd.lastError().text();
            return false;
        }
        for ( int i = 0; i < binds.size(); ++i )
            upd.bindValue(i, binds.at(i));
        upd.bindValue(binds.size(), qsoUuid);
        if ( !upd.exec() )
        {
            if ( errorOut ) *errorOut = upd.lastError().text();
            return false;
        }
    }

    for ( const QString &service : uploadedServices )
    {
        const QString statusCol = service + QLatin1String("_qso_upload_status");
        const QString dateCol   = service + QLatin1String("_qso_upload_date");
        QStringList svcSets { quoteIdent(statusCol) + " = 'Y'" };
        if ( cols.contains(dateCol) )
            svcSets << quoteIdent(dateCol) + " = :d";

        QSqlQuery upd(db);
        if ( !upd.prepare("UPDATE contacts SET " + svcSets.join(", ") +
                          " WHERE qso_uuid = :where_uuid") )
        {
            if ( errorOut ) *errorOut = upd.lastError().text();
            return false;
        }
        if ( cols.contains(dateCol) )
            upd.bindValue(":d", jsonToBind(payload.value(dateCol)));
        upd.bindValue(":where_uuid", qsoUuid);
        if ( !upd.exec() )
        {
            if ( errorOut ) *errorOut = upd.lastError().text();
            return false;
        }
    }
    return true;
}

bool ContactSync::applyUpsertOnDb(QSqlDatabase &db, const QJsonObject &rec,
                                  QString *errorOut)
{
    FCT_IDENTIFICATION;

    const QString qsoUuid   = rec.value("qso_uuid").toString();
    const QString updatedAt = rec.value("updated_at").toString();
    const QJsonObject payload = rec.value("payload").toObject();

    int policyValue = 1;
    {
        QSqlQuery q(db);
        if ( q.prepare(QStringLiteral("SELECT value FROM log_param "
                                      "WHERE name = 'sync/duplicate_policy'"))
             && q.exec() && q.next() )
        {
            policyValue = q.value(0).toInt();
        }
    }
    const DuplicatePolicy policy = static_cast<DuplicatePolicy>(policyValue);

    // LWW vs existing local row by qso_uuid.
    {
        QSqlQuery check(db);
        if ( !check.prepare("SELECT updated_at FROM contacts WHERE qso_uuid = :u") )
        {
            if ( errorOut ) *errorOut = check.lastError().text();
            return false;
        }
        check.bindValue(":u", qsoUuid);
        if ( !check.exec() )
        {
            if ( errorOut ) *errorOut = check.lastError().text();
            return false;
        }
        if ( check.next() )
        {
            const QString local = check.value(0).toString();
            if ( !local.isEmpty() && local >= updatedAt )
                return true;
            return updateFromPayloadOnDb(db, qsoUuid, payload, errorOut);
        }
    }

    // Tombstone wins over older incoming insert.
    {
        QSqlQuery tomb(db);
        if ( !tomb.prepare("SELECT deleted_at FROM contacts_tombstones WHERE qso_uuid = :u") )
        {
            if ( errorOut ) *errorOut = tomb.lastError().text();
            return false;
        }
        tomb.bindValue(":u", qsoUuid);
        if ( !tomb.exec() )
        {
            if ( errorOut ) *errorOut = tomb.lastError().text();
            return false;
        }
        if ( tomb.next() )
        {
            const QString deletedAt = tomb.value(0).toString();
            if ( !deletedAt.isEmpty() && deletedAt >= updatedAt )
                return true;
        }
    }

    // Fingerprint-based duplicate detection (callsign + mode + band +
    // sat_name + start_time ±30 min). Mirrors the ADIF import dupe query.
    {
        const QString fpCall    = payload.value("callsign").toString();
        const QString fpMode    = payload.value("mode").toString();
        const QString fpBand    = payload.value("band").toString();
        const QString fpSat     = payload.value("sat_name").toString();
        const QString fpStart   = payload.value("start_time").toString();

        if ( !fpCall.isEmpty() && !fpMode.isEmpty() && !fpBand.isEmpty()
             && !fpStart.isEmpty() )
        {
            QSqlQuery dup(db);
            if ( !dup.prepare("SELECT qso_uuid, updated_at FROM contacts "
                              "WHERE callsign = upper(:callsign) "
                              "  AND upper(mode) = upper(:mode) "
                              "  AND upper(band) = upper(:band) "
                              "  AND COALESCE(sat_name, '') = COALESCE(:sat_name, '') "
                              "  AND ABS(JULIANDAY(start_time) - "
                              "          JULIANDAY(datetime(:startdate))) * 24 * 60 < 30 "
                              "LIMIT 1") )
            {
                if ( errorOut ) *errorOut = dup.lastError().text();
                return false;
            }
            dup.bindValue(":callsign",  fpCall);
            dup.bindValue(":mode",      fpMode);
            dup.bindValue(":band",      fpBand);
            dup.bindValue(":sat_name",  fpSat.isEmpty() ? QVariant() : QVariant(fpSat));
            dup.bindValue(":startdate", fpStart);
            if ( !dup.exec() )
            {
                if ( errorOut ) *errorOut = dup.lastError().text();
                return false;
            }

            if ( dup.next() )
            {
                const QString localUuid    = dup.value(0).toString();
                const QString localUpdated = dup.value(1).toString();

                if ( policy == DuplicatePolicy::Skip )
                {
                    qCInfo(runtime) << "Sync dup-skip:" << fpCall
                                    << "remote uuid" << qsoUuid
                                    << "local uuid" << localUuid;
                    return true;
                }

                // Merge: both peers converge on the smaller UUID. The side
                // holding the larger one adopts it; the other side waits
                // for that adoption to come back through the journal.
                if ( qsoUuid < localUuid )
                {
                    const bool remoteNewer = localUpdated.isEmpty() || updatedAt > localUpdated;

                    QSqlQuery adopt(db);
                    // When the local copy is the newer one, re-stamp it as
                    // ours so it gets journaled and the peer takes our
                    // fields via LWW. (The update trigger is suppressed
                    // while applying, so this has to be explicit.)
                    const QString sql = remoteNewer
                        ? QStringLiteral("UPDATE contacts SET qso_uuid = :new WHERE qso_uuid = :old")
                        : QStringLiteral("UPDATE contacts SET qso_uuid = :new, "
                                         "  updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), "
                                         "  origin_node = (SELECT value FROM sync_runtime "
                                         "                 WHERE key = 'self_node_id') "
                                         "WHERE qso_uuid = :old");
                    if ( !adopt.prepare(sql) )
                    {
                        if ( errorOut ) *errorOut = adopt.lastError().text();
                        return false;
                    }
                    adopt.bindValue(":new", qsoUuid);
                    adopt.bindValue(":old", localUuid);
                    if ( !adopt.exec() )
                    {
                        if ( errorOut ) *errorOut = adopt.lastError().text();
                        return false;
                    }
                    qCInfo(runtime) << "Sync dup-merge: local" << localUuid
                                    << "→ remote" << qsoUuid
                                    << (remoteNewer ? "(remote fields)" : "(local fields)");

                    if ( remoteNewer )
                        return updateFromPayloadOnDb(db, qsoUuid, payload, errorOut);
                }
                return true;
            }
        }
    }

    // Fresh INSERT with all sync columns pre-set so the AFTER INSERT auto-fill
    // trigger's WHEN-guard skips and preserves the remote node id and ts.
    // Upload state is copied as-is: a brand-new row has no local history.
    QStringList cols;
    QStringList placeholders;
    QVariantList binds;
    const QSet<QString> known = contactColumnsOnDb(db);
    for ( auto it = payload.constBegin(); it != payload.constEnd(); ++it )
    {
        const QString &col = it.key();
        if ( col == QLatin1String("id") )
            continue;
        if ( !known.contains(col) )
            continue;
        if ( it.value().isNull() )
            continue;   // let column defaults apply
        cols << quoteIdent(col);
        placeholders << QStringLiteral("?");
        binds << jsonToBind(it.value());
    }

    QSqlQuery ins(db);
    const QString sql = "INSERT INTO contacts (" + cols.join(", ") + ") "
                        "VALUES (" + placeholders.join(", ") + ")";
    if ( !ins.prepare(sql) )
    {
        if ( errorOut ) *errorOut = ins.lastError().text();
        return false;
    }
    for ( int i = 0; i < binds.size(); ++i )
        ins.bindValue(i, binds.at(i));
    if ( !ins.exec() )
    {
        if ( errorOut ) *errorOut = ins.lastError().text();
        return false;
    }
    return true;
}

bool ContactSync::applyDeleteOnDb(QSqlDatabase &db, const QString &qsoUuid,
                                  const QString &deletedAt,
                                  const QString &origin, QString *errorOut)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << qsoUuid << deletedAt << origin;

    QSqlQuery check(db);
    if ( !check.prepare("SELECT updated_at FROM contacts WHERE qso_uuid = :u") )
    {
        if ( errorOut ) *errorOut = check.lastError().text();
        return false;
    }
    check.bindValue(":u", qsoUuid);
    if ( !check.exec() )
    {
        if ( errorOut ) *errorOut = check.lastError().text();
        return false;
    }
    if ( check.next() )
    {
        const QString local = check.value(0).toString();
        if ( !local.isEmpty() && local > deletedAt )
            return true;   // local edit is newer than the delete
        QSqlQuery del(db);
        if ( !del.prepare("DELETE FROM contacts WHERE qso_uuid = :u") )
        {
            if ( errorOut ) *errorOut = del.lastError().text();
            return false;
        }
        del.bindValue(":u", qsoUuid);
        if ( !del.exec() )
        {
            if ( errorOut ) *errorOut = del.lastError().text();
            return false;
        }
    }

    // Record the tombstone with the remote's timestamp and origin (the
    // tombstone trigger is suppressed while applying). Because origin is
    // the peer, this node does not echo the delete back out.
    QSqlQuery tomb(db);
    if ( !tomb.prepare("INSERT OR REPLACE INTO contacts_tombstones "
                       "(qso_uuid, deleted_at, origin_node) VALUES (:u, :d, :o)") )
    {
        if ( errorOut ) *errorOut = tomb.lastError().text();
        return false;
    }
    tomb.bindValue(":u", qsoUuid);
    tomb.bindValue(":d", deletedAt);
    tomb.bindValue(":o", origin);
    if ( !tomb.exec() )
    {
        if ( errorOut ) *errorOut = tomb.lastError().text();
        return false;
    }
    return true;
}

ContactSync::ApplyResult ContactSync::applyJournalLineOnDb(QSqlDatabase &db,
                                                           const QByteArray &line,
                                                           const QString &expectedEpoch,
                                                           QString *errorOut)
{
    FCT_IDENTIFICATION;

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &parseErr);
    if ( parseErr.error != QJsonParseError::NoError || !doc.isObject() )
    {
        if ( errorOut ) *errorOut = parseErr.errorString();
        return ApplyResult::SkipPermanent;
    }
    const QJsonObject rec = doc.object();

    if ( rec.value("v").toInt() != JOURNAL_FORMAT_VERSION )
    {
        // Probably written by a newer QLog. Don't skip past it; wait until
        // this install is upgraded.
        if ( errorOut ) *errorOut = tr("unsupported journal record version %1")
                                        .arg(rec.value("v").toInt());
        return ApplyResult::Retry;
    }

    // Records written before epochs existed have none and are always valid.
    const QString epoch = rec.value("epoch").toString();
    if ( !epoch.isEmpty() && !expectedEpoch.isEmpty() && epoch != expectedEpoch )
    {
        if ( errorOut ) *errorOut = tr("record from journal epoch %1, expected %2").arg(epoch, expectedEpoch);
        return ApplyResult::OtherEpoch;
    }

    const QString op        = rec.value("op").toString();
    const QString qsoUuid   = rec.value("qso_uuid").toString();
    const QString updatedAt = rec.value("updated_at").toString();
    const QString origin    = rec.value("node").toString();

    if ( qsoUuid.isEmpty() )
    {
        if ( errorOut ) *errorOut = tr("missing qso_uuid");
        return ApplyResult::SkipPermanent;
    }

    if ( op == QLatin1String("upsert") && updatedAt.isEmpty() )
    {
        if ( errorOut ) *errorOut = tr("missing updated_at");
        return ApplyResult::SkipPermanent;
    }

    bool ok = false;
    if ( op == QLatin1String("upsert") )
        ok = applyUpsertOnDb(db, rec, errorOut);
    else if ( op == QLatin1String("delete") )
        ok = applyDeleteOnDb(db, qsoUuid, updatedAt, origin, errorOut);
    else
    {
        if ( errorOut ) *errorOut = tr("unknown op: %1").arg(op);
        return ApplyResult::SkipPermanent;
    }

    // Apply failures are database errors (locked, disk full, constraint
    // from a schema mismatch, ...): retry rather than lose the record.
    return ok ? ApplyResult::Applied : ApplyResult::Retry;
}

QString ContactSync::readPeerEpoch(const QString &peerDir)
{
    FCT_IDENTIFICATION;

    QFile head(QDir(peerDir).filePath(QStringLiteral("HEAD")));
    if ( !head.open(QIODevice::ReadOnly) )
        return QString();
    return QJsonDocument::fromJson(head.readAll()).object().value(QStringLiteral("epoch")).toString();
}

bool ContactSync::pullPeerOnDb(QSqlDatabase &db, const QString &peerNode,
                               const QString &peerDir, int *appliedOut,
                               QString *errorOut)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << peerNode << peerDir;

    if ( appliedOut ) *appliedOut = 0;

    QString lastFile;
    qint64  lastOffset = 0;
    QString storedEpoch;
    {
        QSqlQuery q(db);
        if ( q.prepare("SELECT last_file, last_offset, epoch FROM sync_peers WHERE node = :n") )
        {
            q.bindValue(":n", peerNode);
            if ( q.exec() && q.next() )
            {
                lastFile    = q.value(0).toString();
                lastOffset  = q.value(1).toLongLong();
                storedEpoch = q.value(2).toString();
            }
        }
    }

    // A changed epoch means the peer's journal was restarted: its files and
    // offsets no longer match what was read before, so read it all again
    // (re-applied records are no-ops). Without HEAD (not synced yet), keep
    // the stored epoch.
    const QString headEpoch = readPeerEpoch(peerDir);
    const QString epoch = headEpoch.isEmpty() ? storedEpoch : headEpoch;
    if ( epoch != storedEpoch && ( !lastFile.isEmpty() || lastOffset > 0 ) )
    {
        qCInfo(runtime) << "Journal of" << peerNode << "was restarted; reading it again from the start";
        lastFile.clear();
        lastOffset = 0;
    }
    const int lastSeq = journalSeq(lastFile);

    // Only real journal files, in numeric order.
    QDir peerD(peerDir);
    QList<QPair<int, QString>> journals;
    const QStringList candidates = peerD.entryList(QStringList() << "journal-*.jsonl",
                                                   QDir::Files, QDir::Name);
    for ( const QString &name : candidates )
    {
        const int seq = journalSeq(name);
        if ( seq >= 1 && seq >= lastSeq )
            journals << qMakePair(seq, name);
    }
    std::sort(journals.begin(), journals.end());

    // One transaction per peer: applied records and the resume offset are
    // committed together (no gaps or re-applies after a crash), and the
    // apply runs far faster than one implicit commit per record.
    if ( !db.transaction() )
    {
        if ( errorOut ) *errorOut = db.lastError().text();
        return false;
    }
    if ( !setApplyingOnDb(db, true) )
    {
        if ( errorOut ) *errorOut = tr("cannot set sync apply flag");
        db.rollback();
        return false;
    }

    int applied = 0;
    QString finalFile   = lastFile;
    qint64  finalOffset = lastOffset;
    QString stopReason;

    for ( const auto &entry : journals )
    {
        const QString &name = entry.second;
        QFile f(peerD.filePath(name));
        if ( !f.open(QIODevice::ReadOnly) )
        {
            // e.g. a cloud placeholder that hasn't downloaded yet. Later
            // files must not be read before this one.
            stopReason = tr("cannot open %1: %2").arg(name, f.errorString());
            break;
        }

        const qint64 startAt = (name == lastFile) ? lastOffset : 0;
        if ( startAt > f.size() || !f.seek(startAt) )
        {
            stopReason = tr("%1 is shorter than the last read position; "
                            "it may still be syncing").arg(name);
            break;
        }

        bool fileComplete = true;
        while ( true )
        {
            const QByteArray line = f.readLine();
            if ( line.isEmpty() )
                break;   // EOF

            if ( !line.endsWith('\n') )
            {
                // Partially written or partially synced last line. Leave
                // the offset before it and pick it up on a later cycle.
                fileComplete = false;
                break;
            }

            if ( !line.trimmed().isEmpty() )
            {
                QString err;
                const ApplyResult res = applyJournalLineOnDb(db, line, epoch, &err);
                if ( res == ApplyResult::OtherEpoch )
                {
                    // HEAD and journal files sync separately; wait for the
                    // other half to arrive before moving past this point.
                    qCDebug(runtime) << "Waiting for" << peerNode << name << ":" << err;
                    fileComplete = false;
                    break;
                }
                if ( res == ApplyResult::Retry )
                {
                    stopReason = tr("%1 @%2: %3").arg(name).arg(f.pos() - line.size()).arg(err);
                    fileComplete = false;
                    break;
                }
                if ( res == ApplyResult::Applied )
                    ++applied;
                else
                    qCWarning(runtime) << "Skipping malformed record from" << peerNode
                                       << name << ":" << err;
            }

            finalFile   = name;
            finalOffset = f.pos();
        }
        f.close();

        // A file with an unfinished tail is still being written or synced;
        // records in later files are newer and must wait for it.
        if ( !fileComplete )
            break;
    }

    bool ok = setApplyingOnDb(db, false);

    QSqlQuery upd(db);
    ok = ok && upd.prepare("INSERT OR REPLACE INTO sync_peers "
                           "(node, last_file, last_offset, last_seen_at, epoch) "
                           "VALUES (:n, :f, :o, :t, :e)");
    if ( ok )
    {
        upd.bindValue(":n", peerNode);
        upd.bindValue(":f", finalFile);
        upd.bindValue(":o", static_cast<qlonglong>(finalOffset));
        upd.bindValue(":t", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        upd.bindValue(":e", epoch);
        ok = upd.exec();
    }

    if ( !ok || !db.commit() )
    {
        const QString err = ok ? db.lastError().text() : upd.lastError().text();
        db.rollback();
        if ( errorOut ) *errorOut = err;
        return false;
    }

    if ( appliedOut ) *appliedOut = applied;

    if ( !stopReason.isEmpty() )
    {
        if ( errorOut ) *errorOut = stopReason;
        return false;
    }
    return true;
}

bool ContactSync::pullOnDb(QSqlDatabase &db, int *appliedOut, QString *errorOut)
{
    FCT_IDENTIFICATION;

    QString self;
    {
        QSqlQuery q(db);
        if ( q.exec("SELECT value FROM sync_runtime WHERE key = 'self_node_id'")
             && q.next() )
            self = q.value(0).toString();
    }

    const QString nodesPath = QDir(rootDir()).filePath(SUBDIR_NODES);
    QDir nodesDir(nodesPath);
    if ( !nodesDir.exists() )
    {
        if ( appliedOut ) *appliedOut = 0;
        return true;
    }

    int totalApplied = 0;
    QStringList errors;

    const QStringList peers = nodesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for ( const QString &peer : peers )
    {
        if ( peer == self )
            continue;
        int applied = 0;
        QString err;
        if ( !pullPeerOnDb(db, peer, nodesDir.filePath(peer), &applied, &err) )
            errors << QString("%1: %2").arg(peer, err);
        totalApplied += applied;
    }

    if ( appliedOut ) *appliedOut = totalApplied;
    if ( !errors.isEmpty() )
    {
        if ( errorOut ) *errorOut = errors.join(QStringLiteral("; "));
        return false;
    }
    return true;
}

//
// ---- main-thread dispatchers ----
//

void ContactSync::setBusy(bool busy)
{
    FCT_IDENTIFICATION;

    emit busyChanged(busy);
}

void ContactSync::runFlushCycle()
{
    FCT_IDENTIFICATION;

    if ( !isEnabled() )
        return;
    if ( !busyFlag.testAndSetAcquire(0, 1) )
        return;
    setBusy(true);

    snapshotForCycle();

    QString err;
    if ( !ensureFolderLayout(&err) )
    {
        busyFlag.storeRelease(0);
        setBusy(false);
        qCWarning(runtime) << "Contact sync flush failed:" << err;
        emit flushFailed(err);
        return;
    }

    QSqlDatabase db = QSqlDatabase::database();
    int upserts = 0, deletes = 0;
    QString newUpsertWm, newDeleteWm;
    const bool ok = flushOnDb(db, &upserts, &deletes,
                              &newUpsertWm, &newDeleteWm, &err);

    if ( ok )
    {
        if ( !newUpsertWm.isEmpty() && !LogParam::setSyncUpsertWatermark(newUpsertWm) )
            qCWarning(runtime) << "ContactSync: setSyncUpsertWatermark failed";
        if ( !newDeleteWm.isEmpty() && !LogParam::setSyncDeleteWatermark(newDeleteWm) )
            qCWarning(runtime) << "ContactSync: setSyncDeleteWatermark failed";
        lastFlush = QDateTime::currentDateTimeUtc();
        if ( !LogParam::setSyncLastFlush(lastFlush.toString(Qt::ISODateWithMs)) )
            qCWarning(runtime) << "ContactSync: setSyncLastFlush failed";
        emit flushed(upserts, deletes);
    }
    else
    {
        qCWarning(runtime) << "Contact sync flush failed:" << err;
        emit flushFailed(err);
    }
    busyFlag.storeRelease(0);
    setBusy(false);
}

void ContactSync::runPullCycle()
{
    FCT_IDENTIFICATION;

    if ( !isEnabled() )
        return;
    if ( !busyFlag.testAndSetAcquire(0, 1) )
        return;
    setBusy(true);

    snapshotForCycle();

    QString err;
    if ( !ensureFolderLayout(&err) )
    {
        busyFlag.storeRelease(0);
        setBusy(false);
        qCWarning(runtime) << "Contact sync pull failed:" << err;
        emit pullFailed(err);
        return;
    }

    QSqlDatabase db = QSqlDatabase::database();
    int applied = 0;
    const bool ok = pullOnDb(db, &applied, &err);

    if ( ok )
    {
        if ( !LogParam::setSyncLastPull(
                QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)) )
            qCWarning(runtime) << "ContactSync: setSyncLastPull failed";
        emit pulled(applied);
    }
    else
    {
        qCWarning(runtime) << "Contact sync pull failed:" << err;
        if ( applied > 0 )
            emit pulled(applied);   // other peers may still have applied rows
        emit pullFailed(err);
    }
    busyFlag.storeRelease(0);
    setBusy(false);
}

void ContactSync::runFullCycle()
{
    FCT_IDENTIFICATION;

    if ( !isEnabled() )
        return;
    if ( !busyFlag.testAndSetAcquire(0, 1) )
        return;
    setBusy(true);

    snapshotForCycle();

    QString preErr;
    if ( !ensureFolderLayout(&preErr) )
    {
        busyFlag.storeRelease(0);
        setBusy(false);
        qCWarning(runtime) << "Contact sync cycle failed:" << preErr;
        emit flushFailed(preErr);
        return;
    }

    QSqlDatabase db = QSqlDatabase::database();
    int upserts = 0, deletes = 0, applied = 0;
    QString newUpsertWm, newDeleteWm;
    QString flushErr, pullErr;

    const bool flushOk = flushOnDb(db, &upserts, &deletes,
                                   &newUpsertWm, &newDeleteWm, &flushErr);
    const bool pullOk  = pullOnDb(db, &applied, &pullErr);

    if ( flushOk )
    {
        if ( !newUpsertWm.isEmpty() && !LogParam::setSyncUpsertWatermark(newUpsertWm) )
            qCWarning(runtime) << "ContactSync: setSyncUpsertWatermark failed";
        if ( !newDeleteWm.isEmpty() && !LogParam::setSyncDeleteWatermark(newDeleteWm) )
            qCWarning(runtime) << "ContactSync: setSyncDeleteWatermark failed";
        lastFlush = QDateTime::currentDateTimeUtc();
        if ( !LogParam::setSyncLastFlush(lastFlush.toString(Qt::ISODateWithMs)) )
            qCWarning(runtime) << "ContactSync: setSyncLastFlush failed";
        emit flushed(upserts, deletes);
    }
    else
    {
        qCWarning(runtime) << "Contact sync flush failed:" << flushErr;
        emit flushFailed(flushErr);
    }

    if ( pullOk )
    {
        if ( !LogParam::setSyncLastPull(
                QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)) )
            qCWarning(runtime) << "ContactSync: setSyncLastPull failed";
        emit pulled(applied);
    }
    else
    {
        qCWarning(runtime) << "Contact sync pull failed:" << pullErr;
        if ( applied > 0 )
            emit pulled(applied);
        emit pullFailed(pullErr);
    }

    busyFlag.storeRelease(0);
    setBusy(false);
}

void ContactSync::requestFlush()
{
    FCT_IDENTIFICATION;

    runFlushCycle();
}

void ContactSync::requestPull()
{
    FCT_IDENTIFICATION;

    runPullCycle();
}

void ContactSync::onTick()
{
    FCT_IDENTIFICATION;

    runFullCycle();
}
