#ifndef QLOG_CORE_CONTACTSYNC_H
#define QLOG_CORE_CONTACTSYNC_H

#include <QObject>
#include <QDateTime>
#include <QString>
#include <QSet>
#include <QAtomicInt>
#include <QJsonValue>
#include <QLatin1String>
#include <QVariant>

class QTimer;
class QJsonObject;
class QRegularExpression;
class QSqlDatabase;

class ContactSync : public QObject
{
    Q_OBJECT

public:
    // How to handle a remote upsert whose qso_uuid is unknown locally but
    // whose fingerprint (callsign + mode + band + sat_name + start_time
    // ±30 min) matches an existing local row.
    enum class DuplicatePolicy {
        Skip  = 0,  // log dupe and leave both rows independent
        Merge = 1   // converge on the lexicographically-smaller UUID, then
                    // let LWW reconcile fields (default)
    };

    static ContactSync *instance();

    bool    isEnabled() const;
    QString folder() const;
    QString selfNodeId() const;
    QDateTime lastFlushTime() const;
    QDateTime lastPullTime() const;

    bool setEnabled(bool enabled, QString *errorOut = nullptr);
    bool setFolder(const QString &path, QString *errorOut = nullptr);
    bool setSelfNodeId(const QString &nodeId, QString *errorOut = nullptr);

    DuplicatePolicy duplicatePolicy() const;
    void            setDuplicatePolicy(DuplicatePolicy policy);

    int  pendingChangesCount() const;

    // True when this database's sync identity was claimed by a different
    // QLog installation (typically: qlog.db was copied to a second computer).
    // Sync refuses to run until the user saves a Node ID in the Sync dialog.
    bool nodeIdOwnedElsewhere() const;

    // Node ids double as directory names in the shared folder.
    static bool isValidNodeId(const QString &nodeId);

    void start();
    void stop();

public slots:
    // Run a flush cycle. Called by both the periodic timer and the local-
    // change signals (contactAdded/contactUpdated/Deleted). If a cycle is
    // already running, the call coalesces (the in-flight cycle picks up
    // everything above the watermark anyway).
    void requestFlush();
    void requestPull();

signals:
    void flushed(int upsertCount, int deleteCount);
    void flushFailed(const QString &message);
    void pulled(int appliedCount);
    void pullFailed(const QString &message);
    void busyChanged(bool busy);

private slots:
    void onTick();

private:
    explicit ContactSync(QObject *parent = nullptr);

    // ---- folder helpers (no DB) ----
    QString rootDir() const;
    QString nodeDir() const;
    QString manifestPath() const;
    QString headPath() const;
    QString journalPath(int seq) const;
    QString currentJournalPath() const;

    // ---- worker-side: every method takes the cloned connection it should
    //      use. They never touch the default (main-thread) connection.
    // Outcome of applying one journal record.
    enum class ApplyResult {
        Applied,        // applied (or a no-op that LWW says is correct)
        SkipPermanent,  // malformed record; skip it forever
        Retry,          // transient failure; stop and retry next cycle
        OtherEpoch      // from a different journal epoch; wait for the files to catch up
    };

    static QString currentTimestamp();
    static QString journalFileName(int seq);
    static const QRegularExpression &journalNameRegExp();
    static int journalSeq(const QString &fileName);

    // The helpers below run once per column of every applied record, so they
    // stay inline and free of per-call instrumentation.

    // Upload bookkeeping is per node; see updateFromPayloadOnDb.
    static bool isUploadStateColumn(const QString &col)
    {
        return col.endsWith(QLatin1String("_qso_upload_status"))
            || col.endsWith(QLatin1String("_qso_upload_date"));
    }

    // SQL identifiers cannot be bound. Callers only pass column names taken
    // from PRAGMA table_info(contacts), and those include SQL keywords
    // (the ADIF CHECK field is stored as "check"), so always quote them.
    static QString quoteIdent(const QString &name)
    {
        return QLatin1Char('"') + QString(name).replace(QLatin1Char('"'), QLatin1String("\"\""))
             + QLatin1Char('"');
    }

    // A JSON null must bind as SQL NULL, not as a std::nullptr_t variant.
    static QVariant jsonToBind(const QJsonValue &value)
    {
        if ( value.isNull() || value.isUndefined() )
            return QVariant();
        return value.toVariant();
    }

    static QString installId();     // per-machine id kept in QSettings
    QString writerId() const;       // installId + this log's id
    bool checkOwnership(QString *errorOut);

    bool ensureFolderLayout(QString *errorOut);
    bool loadHead(QString *errorOut);
    bool writeHead();
    bool writeManifestIfMissing();
    bool appendRecord(const QByteArray &line, QString *errorOut);
    bool rotateIfNeeded();

    bool flushOnDb(QSqlDatabase &db, int *upsertsOut, int *deletesOut,
                   QString *newUpsertWatermark, QString *newDeleteWatermark,
                   QString *errorOut);

    bool pullOnDb(QSqlDatabase &db, int *appliedOut, QString *errorOut);
    bool pullPeerOnDb(QSqlDatabase &db, const QString &peerNode,
                      const QString &peerDir, int *appliedOut,
                      QString *errorOut);
    ApplyResult applyJournalLineOnDb(QSqlDatabase &db, const QByteArray &line,
                                     const QString &expectedEpoch, QString *errorOut);
    static QString readPeerEpoch(const QString &peerDir);
    bool applyUpsertOnDb(QSqlDatabase &db, const QJsonObject &record,
                         QString *errorOut);
    bool updateFromPayloadOnDb(QSqlDatabase &db, const QString &qsoUuid,
                               const QJsonObject &payload, QString *errorOut);
    bool applyDeleteOnDb(QSqlDatabase &db, const QString &qsoUuid,
                         const QString &deletedAt, const QString &origin,
                         QString *errorOut);
    bool setApplyingOnDb(QSqlDatabase &db, bool applying);

    QSet<QString> contactColumnsOnDb(QSqlDatabase &db) const;

    // Dispatch helpers. They currently run synchronously on the main thread
    // with the default connection.
    void runFlushCycle();
    void runPullCycle();
    void runFullCycle();   // flush then pull (timer tick)

    void setBusy(bool busy);

    // Cache folder + self for the duration of one flush/pull cycle. Avoids
    // re-querying sync_runtime on every path-helper call. Also positions the
    // class for a future move back to a worker thread, where these would
    // need to be captured on the main thread before dispatch (the path
    // helpers must never call LogParam or QSqlDatabase::database() from a
    // background thread).
    void snapshotForCycle();

    QTimer        *timer;
    int            currentSeq;
    qint64         currentBytes;
    // Identifies this node's current journal set. Changes when the journal
    // restarts (node folder deleted or recreated), so readers know to read
    // it again from the beginning instead of resuming at a stale offset.
    QString        journalEpoch;
    QDateTime      lastFlush;
    QAtomicInt     busyFlag;        // 0 = idle, 1 = cycle running
    QString        cycleFolder;     // snapshot of folder() for this cycle
    QString        cycleSelf;       // snapshot of selfNodeId() for this cycle

    static constexpr const char *SUBDIR_ROOT  = "qlog-sync";
    static constexpr const char *SUBDIR_NODES = "nodes";
    static constexpr qint64 MAX_JOURNAL_BYTES = 4 * 1024 * 1024;
    static constexpr int    FLUSH_INTERVAL_MS = 30000;
    static constexpr int    JOURNAL_FORMAT_VERSION = 1;
};

#endif // QLOG_CORE_CONTACTSYNC_H
