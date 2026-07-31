#ifndef QLOG_SERVICE_QSLINFO_QSLINFO_H
#define QLOG_SERVICE_QSLINFO_QSLINFO_H

#include <QJsonArray>
#include <QPointer>
#include <QSet>
#include <QStringList>

#include "service/QSLManagerSource.h"

class QNetworkAccessManager;
class QNetworkReply;
class QUrl;

class QSLInfo : public QSLManagerSource
{
    Q_OBJECT

public:
    const static QString SOURCE_ID;

    explicit QSLInfo(QObject *parent = nullptr);
    ~QSLInfo() override;

    QString sourceId() const override;

public slots:
    void queryCallsign(const QString &callsign) override;
    void abortQuery() override;

private slots:
    void processReply(QNetworkReply *reply);

protected:
    const QUrl queryUrl(const QString &callsign) const;

private:
    void abortCurrentQuery();
    void startNextRequest();
    void finish(QSLQueryResult result);
    QSLQueryResult resolveResponse(const QString &requestedCallsign,
                                   const QString &responseCallsign,
                                   const QJsonArray &entries);
    const QString decodeInformation(const QString &information) const;
    const QString extractManagerCallsign(const QString &information) const;
    bool isAddressInformation(const QString &information,
                              const QSet<QString> &queriedCallsigns) const;

    QNetworkAccessManager *networkManager;
    QPointer<QNetworkReply> currentReply;
    QString requestedCallsign;
    QStringList queryCallsigns;
    int currentQueryIndex;
};

#endif // QLOG_SERVICE_QSLINFO_QSLINFO_H
