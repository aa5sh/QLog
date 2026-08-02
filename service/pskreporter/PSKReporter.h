#ifndef QLOG_SERVICE_PSKREPORTER_PSKREPORTER_H
#define QLOG_SERVICE_PSKREPORTER_PSKREPORTER_H

#include <QObject>
#include <QString>

#include "data/PskDecode.h"

class PSKReporter : public QObject
{
    Q_OBJECT

public:
    enum class Direction
    {
        SentBy,
        ReceivedBy
    };

    explicit PSKReporter(QObject *parent = nullptr);

    void setSubscription(const QString &callsign);
    void clearSubscription();

public slots:
    void processMessage(PSKReporter::Direction direction,
                        const QString &topic,
                        const QString &payload);

signals:
    void subscriptionChanged(QString callsign);
    void heardMePointRequested(PskDecode decode,
                               PSKReporter::Direction direction);

private:
    QString subscriptionCallsign;
};

#endif // QLOG_SERVICE_PSKREPORTER_PSKREPORTER_H
