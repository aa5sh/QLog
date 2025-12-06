#include <QtTest>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QTextStream>


#include "core/Migration.h"

class MigrationSqlTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void migrateVersion_data();
    void migrateVersion();

private:
    QScopedPointer<QTemporaryDir> tempDir;
    QString dbPath;

    bool executeSqlFile(int version);
    int currentVersion() const;
};

void MigrationSqlTest::initTestCase()
{
    Q_INIT_RESOURCE(res);

    tempDir.reset(new QTemporaryDir);
    QVERIFY(tempDir->isValid());
    dbPath = tempDir->filePath(QStringLiteral("migration_sql_test.sqlite"));

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    db.setDatabaseName(dbPath);
    db.setConnectOptions("QSQLITE_ENABLE_REGEXP");
    QVERIFY2(db.open(), qPrintable(db.lastError().text()));

    QSqlQuery query(QStringLiteral("CREATE TABLE schema_versions (version INTEGER PRIMARY KEY, updated TEXT NOT NULL)"));
    QVERIFY2(query.isActive(), qPrintable(query.lastError().text()));

    QSqlQuery insert(QStringLiteral("INSERT INTO schema_versions (version, updated) VALUES (0, datetime('now'))"));
    QVERIFY2(insert.isActive(), qPrintable(insert.lastError().text()));
}

void MigrationSqlTest::cleanupTestCase()
{
    {
        QSqlDatabase db = QSqlDatabase::database();
        if (db.isValid())
            db.close();
    }
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
}

bool MigrationSqlTest::executeSqlFile(int version)
{
    const QString resourceName = QStringLiteral(":/res/sql/migration_%1.sql")
                                     .arg(version, 3, 10, QChar('0'));
    QFile sqlFile(resourceName);
    if (!sqlFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QWARN(qPrintable(QStringLiteral("Cannot open %1").arg(resourceName)));
        return false;
    }

    const QString sqlContent = QTextStream(&sqlFile).readAll();
    sqlFile.close();

    const QStringList statements = sqlContent.split('\n').join(QStringLiteral(" ")).split(';');

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);

    if (!db.transaction())
        return false;

    for (const QString &statement : statements)
    {
        const QString trimmed = statement.trimmed();
        if (trimmed.isEmpty())
            continue;

        if (!query.exec(trimmed))
        {
            qWarning() << "SQL execution failed for version" << version
                       << ":" << trimmed << query.lastError();
            db.rollback();
            return false;
        }
    }

    QSqlQuery versionInsert(db);
    if (!versionInsert.prepare(QStringLiteral("INSERT INTO schema_versions (version, updated) VALUES (:version, datetime('now'))")))
    {
        db.rollback();
        return false;
    }
    versionInsert.bindValue(QStringLiteral(":version"), version);
    if (!versionInsert.exec())
    {
        db.rollback();
        return false;
    }

    return db.commit();
}

int MigrationSqlTest::currentVersion() const
{
    QSqlQuery query(QStringLiteral("SELECT MAX(version) FROM schema_versions"));
    if (!query.first())
        return -1;
    return query.value(0).toInt();
}

class MigrationSqlTest_FriendAccessor
{
public:
    static int latestVersion() { return Migration::latestVersion; }
};

void MigrationSqlTest::migrateVersion_data()
{
    QTest::addColumn<int>("targetVersion");

    const int latestVersion = MigrationSqlTest_FriendAccessor::latestVersion();
    for (int version = 1; version <= latestVersion; ++version)
    {
        QTest::newRow(QStringLiteral("migration_%1").arg(version, 3, 10, QChar('0')).toUtf8().constData())
            << version;
    }
}

void MigrationSqlTest::migrateVersion()
{
    QFETCH(int, targetVersion);

    const int expectedCurrent = targetVersion - 1;
    QCOMPARE(currentVersion(), expectedCurrent);

    QVERIFY2(executeSqlFile(targetVersion),
             qPrintable(QStringLiteral("Migration SQL file %1 failed").arg(targetVersion, 3, 10, QChar('0'))));
    QCOMPARE(currentVersion(), targetVersion);
}

QTEST_APPLESS_MAIN(MigrationSqlTest)

#include "tst_migration.moc"
