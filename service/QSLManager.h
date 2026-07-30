#ifndef QLOG_SERVICE_QSLMANAGER_H
#define QLOG_SERVICE_QSLMANAGER_H

#include <QObject>
#include <QPointer>
#include <QTimer>

#include "service/QSLManagerSource.h"

class QSLManager : public QObject
{
    Q_OBJECT

public:
    const static QString NO_SOURCE;

    explicit QSLManager(QObject *parent = nullptr);

    bool isActive() const;
    void queryCallsign(const QString &callsign,
                       const QString &fallback = QString());

signals:
    void queryFinished(QSLQueryResult result);

public slots:
    void initSource();
    void abortQuery();

private slots:
    void sourceQueryFinished(const QSLQueryResult &result);
    void queryTimedOut();

private:
    QSLManagerSource *createSource(const QString &sourceId);
    void finishQuery(QSLQueryResult result);

    static const int QUERY_TIMEOUT_MS = 8000;

    QPointer<QSLManagerSource> source;
    QTimer queryTimer;
    QString currentCallsign;
    QString fallbackQslVia;
};

#endif // QLOG_SERVICE_QSLMANAGER_H
