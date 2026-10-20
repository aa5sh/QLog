#ifndef QLOG_CORE_MIGRATION_H
#define QLOG_CORE_MIGRATION_H

#include <QObject>

class QSqlQuery;

class DBSchemaMigration : public QObject
{
    Q_OBJECT

public:
    DBSchemaMigration(QObject *parent = nullptr) : QObject(parent) {}
    bool run(QString *backupSnapshot = nullptr);

    static constexpr int latestVersion = 44;

    // Fork-private schema track for MyFeatures-only changes. Kept separate
    // from latestVersion/migration_0NN.sql so upstream syncs never collide
    // with personal-branch schema changes.
    static constexpr int latestMyFeaturesVersion = 1;

private:
    bool setForeignKeysEnabled(bool enabled);
    bool functionMigration(int version);
    bool migrate(int toVersion);
    int getVersion();
    bool setVersion(int version);
    bool migrateMyFeatures(int toVersion);
    int getMyFeaturesVersion();
    bool setMyFeaturesVersion(int version);
    bool runSqlFile(QString filename);
    int tableRows(const QString &name);
    bool fixIntlFields();
    bool insertUUID();
    bool fillMyDXCC();
    bool createTriggers();
    bool importQSLCards2DB();
    bool fillCQITUZStationProfiles();
    bool resetConfigs();
    bool profiles2DB();
    bool settings2DB();
    bool removeSettings2DB();
    bool emailQSLSettings2DB();
    bool setSelectedProfile(const QString &tablename, const QString &profileName);
    QString fixIntlField(const QSqlQuery &query, const QString &columName, const QString &columnNameIntl);
    bool refreshUploadStatusTrigger();

    friend class MigrationSqlTest_FriendAccessor;
};

#endif // QLOG_CORE_MIGRATION_H
