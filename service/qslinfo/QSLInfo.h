#ifndef QLOG_SERVICE_QSLINFO_QSLINFO_H
#define QLOG_SERVICE_QSLINFO_QSLINFO_H

#include "service/GenericCallbook.h"

class QNetworkReply;

/**
 * Supplemental callbook provider for the public QSLInfo.de REST API.
 *
 * QSLInfo supplies QSL routing information only. CallbookManager combines
 * this response with the selected primary/secondary callbook response.
 */
class QSLInfoCallbook : public GenericCallbook
{
    Q_OBJECT

public:
    const static QString CALLBOOK_NAME;

    explicit QSLInfoCallbook(QObject *parent = nullptr);
    ~QSLInfoCallbook() override;

    QString getDisplayName() override;

public slots:
    void queryCallsign(const QString &callsign) override;
    void abortQuery() override;

protected:
    void processReply(QNetworkReply *reply) override;

private:
    QNetworkReply *currentReply;
    const QString API_URL = QStringLiteral("https://www.qslinfo.de/api/v1/call/");
};

#endif // QLOG_SERVICE_QSLINFO_QSLINFO_H
