#ifndef QLOG_DATA_UPDATABLESQLRECORD_H
#define QLOG_DATA_UPDATABLESQLRECORD_H

#include <QObject>
#include <QSqlRecord>
#include <QTimer>
#include <QHash>

class UpdatableSQLRecord : public QObject
{
    Q_OBJECT

public:
    explicit UpdatableSQLRecord(int interval = 500,
                                int maxUpdates = 2,
                                QObject *parent = nullptr);

    ~UpdatableSQLRecord();
    void updateRecord(const QSqlRecord &record);

signals:
    void recordReady( QSqlRecord );

private slots:
    void emitStoreRecord();

private:

    enum MatchingType{
        QSOMatchingType
    };

    QHash<MatchingType, QStringList> matchingFields
    {
        {QSOMatchingType, {"callsign", "mode", "submode"}}
    };

    bool matchQSO(const MatchingType,
                  const QSqlRecord &);

    void resetCnt();
    void incrementCnt() {currUpdateCnt++;};
    bool emitIfMaxUpdatesReached();
    void startNextUpdateCycle();

    QSqlRecord internalRecord;
    QTimer timer;
    int interval;
    int maxUpdates;
    int currUpdateCnt;
};

#endif // QLOG_DATA_UPDATABLESQLRECORD_H
