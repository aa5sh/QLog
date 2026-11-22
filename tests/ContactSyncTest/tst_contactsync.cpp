#include <QtTest>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSettings>
#include <QStandardPaths>
#include <QSignalSpy>

#include "core/ContactSync.h"
#include "core/LogDatabase.h"
#include "core/LogParam.h"
#include "core/Migration.h"

// Two QLog "installs" (separate app names => separate databases and
// QSettings install ids) syncing through one shared folder. Each test case
// starts from fresh nodes so the cases stay independent.
class ContactSyncTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    void remoteEditIsNotReclaimed();
    void clearedFieldPropagates();
    void remoteDeleteIsNotEchoed();
    void partialLineIsRetried();
    void conflictCopiesAreIgnored();
    void uploadStateMergesPerNode();
    void copiedDatabaseIsRejected();
    void renameKeepsOwnLogSyncing();
    void restartedJournalIsReadAgain();
    void otherEpochRecordsWait();

private:
    void switchTo(const QString &node);
    int flush();                     // returns upserts + deletes, -1 on failure
    int pull();                      // returns applied, -1 on failure
    QVariant value(const QString &sql);
    void exec(const QString &sql);

    QScopedPointer<QTemporaryDir> tempDir;
    QString syncFolder;
    QString nodeA, nodeB;      // application names (one database each)
    QString idA, idB;          // sync node ids
    QString runTag;
    int round = 0;
};

void ContactSyncTest::initTestCase()
{
    Q_INIT_RESOURCE(res);
    tempDir.reset(new QTemporaryDir);
    QVERIFY(tempDir->isValid());
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false\nqlog.*.info=false"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir->path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, tempDir->path());
#ifdef Q_OS_LINUX
    qputenv("XDG_DATA_HOME", tempDir->path().toUtf8());
#else
    QStandardPaths::setTestModeEnabled(true);
#endif
    // Test-mode data directories outlive the run; never reuse them.
    runTag = QUuid::createUuid().toString(QUuid::Id128).left(8);
}

void ContactSyncTest::cleanupTestCase()
{
    ContactSync::instance()->stop();
    QSqlDatabase::database().close();
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    for ( int i = 1; i <= round; ++i )
        for ( const QString &n : { QStringLiteral("A"), QStringLiteral("B") } )
        {
            QCoreApplication::setApplicationName(QStringLiteral("qlog-sync-test-%1-%2%3").arg(runTag, n).arg(i));
            QDir(LogDatabase::dbDirectory()).removeRecursively();
        }
}

void ContactSyncTest::init()
{
    ++round;
    syncFolder = tempDir->filePath(QStringLiteral("shared%1").arg(round));
    QVERIFY(QDir().mkpath(syncFolder));
    idA = QStringLiteral("A%1").arg(round);
    idB = QStringLiteral("B%1").arg(round);
    nodeA = QStringLiteral("qlog-sync-test-%1-%2").arg(runTag, idA);
    nodeB = QStringLiteral("qlog-sync-test-%1-%2").arg(runTag, idB);
    for ( const QString &node : { nodeA, nodeB } )
    {
        switchTo(node);
        if ( QTest::currentTestFailed() ) return;
    }
}

void ContactSyncTest::switchTo(const QString &node)
{
    ContactSync::instance()->stop();
    if ( QSqlDatabase::contains(QSqlDatabase::defaultConnection) )
    {
        QSqlDatabase::database().close();
        QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    }
    QCoreApplication::setApplicationName(node);
    LogParam::clearCache();
    QVERIFY(QDir().mkpath(LogDatabase::dbDirectory().absolutePath()));
    QVERIFY(LogDatabase::instance()->openDatabase());

    QSqlQuery q;
    const bool fresh = !q.exec(QStringLiteral("SELECT 1 FROM schema_versions LIMIT 1"));
    if ( fresh )
    {
        QVERIFY(q.exec(QStringLiteral("CREATE TABLE schema_versions (version INTEGER PRIMARY KEY, updated TEXT NOT NULL)")));
        QVERIFY(q.exec(QStringLiteral("INSERT INTO schema_versions VALUES (0, datetime('now'))")));
    }
    q.finish();
    DBSchemaMigration migration;
    QVERIFY(migration.run());

    if ( fresh )
    {
        QVERIFY(q.exec(QStringLiteral("INSERT OR REPLACE INTO log_param (name, value) VALUES ('logid', '%1')")
                           .arg(QUuid::createUuid().toString())));
        LogParam::clearCache();
        QString err;
        const QString id = (node == nodeA) ? idA : idB;
        QVERIFY2(ContactSync::instance()->setSelfNodeId(id, &err), qPrintable(err));
        QVERIFY(LogParam::setSyncFolder(syncFolder));
        QVERIFY(LogParam::setSyncEnabled(true));
    }
}

int ContactSyncTest::flush()
{
    QSignalSpy ok(ContactSync::instance(), &ContactSync::flushed);
    QSignalSpy bad(ContactSync::instance(), &ContactSync::flushFailed);
    ContactSync::instance()->requestFlush();
    if ( !bad.isEmpty() )
    {
        qWarning() << "flush failed:" << bad.first().first().toString();
        return -1;
    }
    if ( ok.isEmpty() ) return -1;
    return ok.first().at(0).toInt() + ok.first().at(1).toInt();
}

int ContactSyncTest::pull()
{
    QSignalSpy ok(ContactSync::instance(), &ContactSync::pulled);
    QSignalSpy bad(ContactSync::instance(), &ContactSync::pullFailed);
    ContactSync::instance()->requestPull();
    if ( !bad.isEmpty() )
    {
        qWarning() << "pull failed:" << bad.first().first().toString();
        return -1;
    }
    if ( ok.isEmpty() ) return -1;
    return ok.first().first().toInt();
}

QVariant ContactSyncTest::value(const QString &sql)
{
    QSqlQuery q;
    if ( !q.exec(sql) || !q.next() )
    {
        qWarning() << sql << q.lastError().text();
        return QVariant(QStringLiteral("<no row>"));
    }
    return q.value(0);
}

void ContactSyncTest::exec(const QString &sql)
{
    QSqlQuery q;
    QVERIFY2(q.exec(sql), qPrintable(sql + ": " + q.lastError().text()));
}

static const char *INSERT_QSO =
    "INSERT INTO contacts (callsign, mode, band, freq, start_time, end_time, comment, "
    "                      qrzcom_qso_upload_status) "
    "VALUES ('OK1TEST', 'CW', '20m', 14.025, '2026-11-01T10:00:00.000Z', "
    "        '2026-11-01T10:01:00.000Z', 'first', 'N')";

void ContactSyncTest::remoteEditIsNotReclaimed()
{
    switchTo(nodeA);
    exec(INSERT_QSO);
    QCOMPARE(flush(), 1);

    switchTo(nodeB);
    QCOMPARE(pull(), 1);
    QCOMPARE(value("SELECT origin_node FROM contacts WHERE callsign='OK1TEST'").toString(), idA);
    QCOMPARE(flush(), 0);                       // nothing of ours to send

    exec("UPDATE contacts SET comment = 'edited on B' WHERE callsign='OK1TEST'");
    QCOMPARE(flush(), 1);

    switchTo(nodeA);
    QCOMPARE(pull(), 1);
    QCOMPARE(value("SELECT comment FROM contacts WHERE callsign='OK1TEST'").toString(), QStringLiteral("edited on B"));
    QCOMPARE(value("SELECT origin_node FROM contacts WHERE callsign='OK1TEST'").toString(), idB);
    // Before the fix, update_contacts_upload_status re-fired the sync update
    // trigger here and A re-claimed the row, bouncing it back to B forever.
    QCOMPARE(flush(), 0);

    switchTo(nodeB);
    QCOMPARE(pull(), 0);
}

void ContactSyncTest::clearedFieldPropagates()
{
    switchTo(nodeA);
    exec(INSERT_QSO);
    QCOMPARE(flush(), 1);
    switchTo(nodeB);
    QCOMPARE(pull(), 1);

    switchTo(nodeA);
    exec("UPDATE contacts SET comment = NULL WHERE callsign='OK1TEST'");
    QCOMPARE(flush(), 1);
    switchTo(nodeB);
    QCOMPARE(pull(), 1);
    QVERIFY(value("SELECT comment FROM contacts WHERE callsign='OK1TEST'").isNull());
}

void ContactSyncTest::remoteDeleteIsNotEchoed()
{
    switchTo(nodeA);
    exec(INSERT_QSO);
    QCOMPARE(flush(), 1);
    switchTo(nodeB);
    QCOMPARE(pull(), 1);

    switchTo(nodeA);
    exec("DELETE FROM contacts WHERE callsign='OK1TEST'");
    QCOMPARE(flush(), 1);

    switchTo(nodeB);
    QCOMPARE(pull(), 1);
    QCOMPARE(value("SELECT COUNT(*) FROM contacts").toInt(), 0);
    QCOMPARE(value("SELECT origin_node FROM contacts_tombstones").toString(), idA);
    QCOMPARE(flush(), 0);
}

void ContactSyncTest::partialLineIsRetried()
{
    switchTo(nodeA);
    exec(INSERT_QSO);
    exec("INSERT INTO contacts (callsign, mode, band, start_time, end_time) "
         "VALUES ('OK2TEST', 'SSB', '40m', '2026-11-02T10:00:00.000Z', '2026-11-02T10:01:00.000Z')");
    QCOMPARE(flush(), 2);

    // Simulate the cloud client having delivered only part of the 2nd line.
    const QString journal = QDir(syncFolder).filePath(QStringLiteral("qlog-sync/nodes/%1/journal-0001.jsonl").arg(idA));
    QFile f(journal);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray full = f.readAll();
    f.close();
    const qsizetype firstEnd = full.indexOf('\n') + 1;
    QVERIFY(firstEnd > 0);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(full.left(firstEnd + 20));
    f.close();

    switchTo(nodeB);
    QCOMPARE(pull(), 1);
    QCOMPARE(value("SELECT last_offset FROM sync_peers").toLongLong(), qint64(firstEnd));

    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(full);
    f.close();
    QCOMPARE(pull(), 1);
    QCOMPARE(value("SELECT COUNT(*) FROM contacts").toInt(), 2);
}

void ContactSyncTest::conflictCopiesAreIgnored()
{
    switchTo(nodeA);
    exec(INSERT_QSO);
    QCOMPARE(flush(), 1);
    const QString dir = QDir(syncFolder).filePath(QStringLiteral("qlog-sync/nodes/%1").arg(idA));
    QFile conflict(QDir(dir).filePath(QStringLiteral("journal-0001 (conflicted copy).jsonl")));
    QVERIFY(conflict.open(QIODevice::WriteOnly));
    conflict.write("{\"v\":1,\"op\":\"delete\",\"qso_uuid\":\"x\",\"updated_at\":\"2030\",\"node\":\"A\"}\n");
    conflict.close();

    switchTo(nodeB);
    QCOMPARE(pull(), 1);
    QCOMPARE(value("SELECT COUNT(*) FROM contacts_tombstones").toInt(), 0);
}

void ContactSyncTest::uploadStateMergesPerNode()
{
    switchTo(nodeA);
    exec(INSERT_QSO);
    QCOMPARE(flush(), 1);
    switchTo(nodeB);
    QCOMPARE(pull(), 1);
    // B uploads to QRZ.
    exec("UPDATE contacts SET qrzcom_qso_upload_status='Y', qrzcom_qso_upload_date='2026-11-03' WHERE callsign='OK1TEST'");
    QCOMPARE(flush(), 1);

    switchTo(nodeA);
    QCOMPARE(pull(), 1);
    // A takes over B's 'Y' (QRZ already holds this content), so it won't re-upload.
    QCOMPARE(value("SELECT qrzcom_qso_upload_status FROM contacts").toString(), QStringLiteral("Y"));
    QCOMPARE(flush(), 0);

    // A edits; the upload trigger marks it modified locally ('M') and that is
    // journaled. B already uploaded the old version, so B must also see 'M',
    // not A's status copied over.
    exec("UPDATE contacts SET comment='fixed' WHERE callsign='OK1TEST'");
    QCOMPARE(value("SELECT qrzcom_qso_upload_status FROM contacts").toString(), QStringLiteral("M"));
    QCOMPARE(flush(), 1);

    switchTo(nodeB);
    QCOMPARE(pull(), 1);
    QCOMPARE(value("SELECT comment FROM contacts").toString(), QStringLiteral("fixed"));
    QCOMPARE(value("SELECT qrzcom_qso_upload_status FROM contacts").toString(), QStringLiteral("M"));
    QCOMPARE(flush(), 0);
}

void ContactSyncTest::copiedDatabaseIsRejected()
{
    switchTo(nodeA);
    // As if qlog.db had been copied here from a machine with another install id.
    exec("UPDATE sync_runtime SET value='someone-else' WHERE key='owner_install_id'");
    QVERIFY(ContactSync::instance()->nodeIdOwnedElsewhere());
    QCOMPARE(flush(), -1);

    QString err;
    QVERIFY(ContactSync::instance()->setSelfNodeId(QStringLiteral("A-laptop"), &err));
    QVERIFY(!ContactSync::instance()->nodeIdOwnedElsewhere());
    QCOMPARE(flush(), 0);

    QVERIFY(!ContactSync::isValidNodeId(QStringLiteral("../evil")));
    QVERIFY(!ContactSync::isValidNodeId(QStringLiteral(".hidden")));
    QVERIFY(ContactSync::isValidNodeId(QStringLiteral("Shack-PC_2.local")));
}

void ContactSyncTest::renameKeepsOwnLogSyncing()
{
    switchTo(nodeA);
    exec(INSERT_QSO);
    QCOMPARE(flush(), 1);                       // sent under the old name

    // Changes made before the rename that were never sent, then the rename.
    exec("INSERT INTO contacts (callsign, mode, band, start_time, end_time) "
         "VALUES ('OK2TEST', 'SSB', '40m', '2026-11-02T10:00:00.000Z', '2026-11-02T10:01:00.000Z')");
    exec("DELETE FROM contacts WHERE callsign='OK1TEST'");
    const QString renamed = idA + QStringLiteral("-new");
    QString err;
    QVERIFY2(ContactSync::instance()->setSelfNodeId(renamed, &err), qPrintable(err));
    QCOMPARE(value("SELECT COUNT(*) FROM contacts WHERE origin_node <> '" + renamed + "'").toInt(), 0);
    QCOMPARE(value("SELECT origin_node FROM contacts_tombstones").toString(), renamed);
    QCOMPARE(flush(), 2);                       // the new QSO and the delete

    switchTo(nodeB);
    QCOMPARE(pull(), 3);
    QCOMPARE(value("SELECT callsign FROM contacts").toString(), QStringLiteral("OK2TEST"));
    QCOMPARE(value("SELECT COUNT(*) FROM contacts").toInt(), 1);
}

void ContactSyncTest::restartedJournalIsReadAgain()
{
    switchTo(nodeA);
    exec(INSERT_QSO);
    exec("INSERT INTO contacts (callsign, mode, band, start_time, end_time) "
         "VALUES ('OK2TEST', 'SSB', '40m', '2026-11-02T10:00:00.000Z', '2026-11-02T10:01:00.000Z')");
    QCOMPARE(flush(), 2);
    switchTo(nodeB);
    QCOMPARE(pull(), 2);

    // The node folder is deleted (or recreated elsewhere); A starts a new,
    // shorter journal. B must read it from the start, not stop at its old offset.
    QVERIFY(QDir(QDir(syncFolder).filePath(QStringLiteral("qlog-sync/nodes/") + idA)).removeRecursively());
    switchTo(nodeA);
    exec("INSERT INTO contacts (callsign, mode, band, start_time, end_time) "
         "VALUES ('OK3TEST', 'CW', '30m', '2026-11-03T10:00:00.000Z', '2026-11-03T10:01:00.000Z')");
    QCOMPARE(flush(), 1);

    switchTo(nodeB);
    QCOMPARE(pull(), 1);
    QCOMPARE(value("SELECT COUNT(*) FROM contacts").toInt(), 3);
}

void ContactSyncTest::otherEpochRecordsWait()
{
    // HEAD and journal sync separately: a journal from another epoch than
    // HEAD must not be read past until they agree.
    switchTo(nodeA);
    exec(INSERT_QSO);
    QCOMPARE(flush(), 1);
    const QString dir = QDir(syncFolder).filePath(QStringLiteral("qlog-sync/nodes/") + idA);
    QFile journal(QDir(dir).filePath(QStringLiteral("journal-0001.jsonl")));
    QVERIFY(journal.open(QIODevice::ReadOnly));
    const QByteArray original = journal.readAll();
    journal.close();
    QByteArray stale = original;
    stale.replace("\"epoch\":\"", "\"epoch\":\"stale-");
    QVERIFY(journal.open(QIODevice::WriteOnly | QIODevice::Truncate));
    journal.write(stale);
    journal.close();

    switchTo(nodeB);
    QCOMPARE(pull(), 0);
    QCOMPARE(value("SELECT last_offset FROM sync_peers").toLongLong(), qint64(0));

    QVERIFY(journal.open(QIODevice::WriteOnly | QIODevice::Truncate));
    journal.write(original);
    journal.close();
    QCOMPARE(pull(), 1);
}

QTEST_MAIN(ContactSyncTest)
#include "tst_contactsync.moc"
