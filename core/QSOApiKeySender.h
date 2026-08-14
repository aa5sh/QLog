#ifndef QLOG_CORE_QSOAPIKEYSENDER_H
#define QLOG_CORE_QSOAPIKEYSENDER_H

#include <QObject>
#include <QUrl>
#include <QSqlRecord>
#include <QNetworkAccessManager>
#include "core/CredentialStore.h"

class QNetworkReply;

class QSOApiKeySenderBase : public SecureServiceBase<QSOApiKeySenderBase>
{
protected:
    const static QString SECURE_STORAGE_KEY;
    const static QString CONFIG_USERNAME_CONST;

public:
    explicit QSOApiKeySenderBase() {};
    virtual ~QSOApiKeySenderBase() {};

    DECLARE_SECURE_SERVICE(QSOApiKeySenderBase);

    static QString getUsername() {return CONFIG_USERNAME_CONST;}
    static QString getAPIKey();
    static void saveAPIKey(const QString &newKey);

    static bool getEnabled();
    static void setEnabled(bool enabled);
    static QString getURL();
    static void setURL(const QString &url);
};

/*!
 * Settings -> Network -> "Send QSO via API key".
 *
 * Unlike the existing UDP Notifications (core/NetworkNotification), this
 * talks HTTP(S) to a single configured endpoint, for services that
 * authenticate with a login-style API key rather than accept it as a
 * plain query parameter (the radiodyplom.pl live-log concept).
 *
 * Contract - two separate requests per logged QSO:
 *   1. POST {url}  body: apikey=<key>  (application/x-www-form-urlencoded)
 *      The secret never appears in a URL/access log, and - over https://
 *      - stays encrypted end-to-end.
 *   2. GET  {url}?<qso fields>
 *      The QSO itself, as flat ADIF-named query parameters (see
 *      QSOApiKeyQuery). No apikey here - authentication already happened
 *      in step 1. If the server sets a session cookie on the POST
 *      response, QNetworkAccessManager's default cookie jar carries it
 *      into the GET automatically - the same login-then-act flow a
 *      browser would do, just performed by QLog instead of a human.
 *   The server decides how (or whether) to correlate the two requests;
 *   QLog does not send any shared token/session id of its own beyond the
 *   above.
 */
class QSOApiKeySender : public QObject, private QSOApiKeySenderBase
{
    Q_OBJECT

public:
    explicit QSOApiKeySender(QObject *parent = nullptr);

public slots:
    void QSOInserted(const QSqlRecord &record);

    // The actual network mechanism, deliberately free of LogParam/
    // CredentialStore reads so it can be unit-tested (fake HTTP server)
    // without touching either - see tests/QSOApiKeySenderTest.
    void sendQSO(const QString &url, const QString &apiKey, const QSqlRecord &record);

signals:
    void sendFinished(bool ok, const QString &message);

private slots:
    void onNetworkReply(QNetworkReply *reply);

private:
    enum class Stage { Login, Upload };

    QNetworkAccessManager nam;
    Stage currentStage = Stage::Login;
    QUrl pendingUrl;
    QSqlRecord pendingRecord;
};

#endif // QLOG_CORE_QSOAPIKEYSENDER_H
