#include <QtTest>
#include <QSqlRecord>
#include <QSqlField>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSignalSpy>
#include <QMap>

#include "core/QSOApiKeySender.h"

namespace {

struct ReceivedRequest
{
    QString method;
    QString target;   // path + query string, as sent on the request line
    QString body;
    QString cookieHeader;
};

// Minimal fake HTTP/1.1 server used to observe what QSOApiKeySender::sendQSO
// actually puts on the wire, without depending on any real service.
class FakeHttpServer : public QObject
{
    Q_OBJECT

public:
    explicit FakeHttpServer(QObject *parent = nullptr) : QObject(parent)
    {
        connect(&server, &QTcpServer::newConnection, this, &FakeHttpServer::onNewConnection);
        server.listen(QHostAddress::LocalHost);
    }

    quint16 port() const { return server.serverPort(); }

    // Canned response for the Nth request received (0-based). Defaults to
    // "200 OK" with an empty body if never set.
    void setResponse(int index, int statusCode, const QByteArray &extraHeaders = {})
    {
        Response r;
        r.statusCode = statusCode;
        r.extraHeaders = extraHeaders;
        responses[index] = r;
    }

    QList<ReceivedRequest> requests;

private slots:
    void onNewConnection()
    {
        while (server.hasPendingConnections())
        {
            QTcpSocket *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { onReadyRead(socket); });
        }
    }

private:
    struct Response { int statusCode = 200; QByteArray extraHeaders; };

    void onReadyRead(QTcpSocket *socket)
    {
        QByteArray &buf = buffers[socket];
        buf += socket->readAll();

        const int headerEnd = buf.indexOf("\r\n\r\n");
        if (headerEnd < 0)
            return;

        const QByteArray headerPart = buf.left(headerEnd);
        const QList<QByteArray> headerLines = headerPart.split('\n');
        const QByteArray requestLine = headerLines.value(0).trimmed();

        int contentLength = 0;
        QByteArray cookieHeader;
        for (const QByteArray &line : headerLines)
        {
            if (line.startsWith("Content-Length:"))
                contentLength = line.mid(15).trimmed().toInt();
            if (line.startsWith("Cookie:"))
                cookieHeader = line.mid(7).trimmed();
        }

        const int bodyStart = headerEnd + 4;
        if (buf.size() < bodyStart + contentLength)
            return;

        const QByteArray body = buf.mid(bodyStart, contentLength);

        const QList<QByteArray> parts = requestLine.split(' ');

        ReceivedRequest req;
        req.method = QString::fromLatin1(parts.value(0));
        req.target = QString::fromLatin1(parts.value(1));
        req.body = QString::fromLatin1(body);
        req.cookieHeader = QString::fromLatin1(cookieHeader);

        const int index = requests.size();
        requests.append(req);

        const Response resp = responses.value(index);

        QByteArray out = "HTTP/1.1 " + QByteArray::number(resp.statusCode) + " Status\r\n";
        out += "Content-Length: 0\r\n";
        out += "Connection: close\r\n";
        out += resp.extraHeaders;
        out += "\r\n";

        socket->write(out);
        socket->flush();
        socket->disconnectFromHost();

        buffers.remove(socket);
    }

    QTcpServer server;
    QMap<QTcpSocket *, QByteArray> buffers;
    QMap<int, Response> responses;
};

} // namespace

class QSOApiKeySenderTest : public QObject
{
    Q_OBJECT

private slots:
    void sendQSO_postsApiKeyThenGetsQSOFieldsWithoutApiKey();
    void sendQSO_carriesLoginSessionCookieIntoUpload();
    void sendQSO_stopsAfterFailedLogin();
    void sendQSO_reportsFailedUpload();

private:
    static void appendField(QSqlRecord &record, const QString &name, const QVariant &value);
};

void QSOApiKeySenderTest::appendField(QSqlRecord &record, const QString &name, const QVariant &value)
{
    QSqlField field(name, value.type());
    field.setValue(value);
    record.append(field);
}

void QSOApiKeySenderTest::sendQSO_postsApiKeyThenGetsQSOFieldsWithoutApiKey()
{
    FakeHttpServer server;
    QVERIFY(server.port() != 0);

    QSqlRecord record;
    appendField(record, QStringLiteral("callsign"), QStringLiteral("OK1AA"));
    appendField(record, QStringLiteral("band"), QStringLiteral("20m"));

    QSOApiKeySender sender;
    QSignalSpy finishedSpy(&sender, &QSOApiKeySender::sendFinished);

    const QString url = QStringLiteral("http://127.0.0.1:%1/upload_qso").arg(server.port());
    sender.sendQSO(url, QStringLiteral("secret-key-123"), record);

    QVERIFY(finishedSpy.wait(2000));
    QCOMPARE(server.requests.size(), 2);

    const ReceivedRequest &login = server.requests.at(0);
    QCOMPARE(login.method, QStringLiteral("POST"));
    QVERIFY(login.target.startsWith(QStringLiteral("/upload_qso")));
    QCOMPARE(login.body, QStringLiteral("apikey=secret-key-123"));
    // the secret must never leak into the URL/target of the login request
    QVERIFY(!login.target.contains(QStringLiteral("secret-key-123")));

    const ReceivedRequest &upload = server.requests.at(1);
    QCOMPARE(upload.method, QStringLiteral("GET"));
    QVERIFY(upload.target.contains(QStringLiteral("call=OK1AA")));
    QVERIFY(upload.target.contains(QStringLiteral("band=20m")));
    // the api key must not be repeated in the QSO upload request
    QVERIFY(!upload.target.contains(QStringLiteral("secret-key-123")));
    QVERIFY(upload.body.isEmpty());

    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(finishedSpy.at(0).at(0).toBool(), true);
}

void QSOApiKeySenderTest::sendQSO_carriesLoginSessionCookieIntoUpload()
{
    FakeHttpServer server;
    server.setResponse(0, 200, "Set-Cookie: sessid=abc123; Path=/\r\n");

    QSqlRecord record;
    appendField(record, QStringLiteral("callsign"), QStringLiteral("OK1AA"));

    QSOApiKeySender sender;
    QSignalSpy finishedSpy(&sender, &QSOApiKeySender::sendFinished);

    const QString url = QStringLiteral("http://127.0.0.1:%1/upload_qso").arg(server.port());
    sender.sendQSO(url, QStringLiteral("secret-key-123"), record);

    QVERIFY(finishedSpy.wait(2000));
    QCOMPARE(server.requests.size(), 2);

    // the cookie set on the POST (login) response must come back on the
    // following GET (upload) - QNetworkAccessManager's default cookie jar
    // is what makes the "apikey as login" concept work at all.
    QCOMPARE(server.requests.at(1).cookieHeader, QStringLiteral("sessid=abc123"));
}

void QSOApiKeySenderTest::sendQSO_stopsAfterFailedLogin()
{
    FakeHttpServer server;
    server.setResponse(0, 403);

    QSqlRecord record;
    appendField(record, QStringLiteral("callsign"), QStringLiteral("OK1AA"));

    QSOApiKeySender sender;
    QSignalSpy finishedSpy(&sender, &QSOApiKeySender::sendFinished);

    const QString url = QStringLiteral("http://127.0.0.1:%1/upload_qso").arg(server.port());
    sender.sendQSO(url, QStringLiteral("wrong-key"), record);

    QVERIFY(finishedSpy.wait(2000));

    // a rejected login must never be followed by the QSO upload request
    QCOMPARE(server.requests.size(), 1);
    QCOMPARE(finishedSpy.at(0).at(0).toBool(), false);
}

void QSOApiKeySenderTest::sendQSO_reportsFailedUpload()
{
    FakeHttpServer server;
    server.setResponse(1, 500);

    QSqlRecord record;
    appendField(record, QStringLiteral("callsign"), QStringLiteral("OK1AA"));

    QSOApiKeySender sender;
    QSignalSpy finishedSpy(&sender, &QSOApiKeySender::sendFinished);

    const QString url = QStringLiteral("http://127.0.0.1:%1/upload_qso").arg(server.port());
    sender.sendQSO(url, QStringLiteral("secret-key-123"), record);

    QVERIFY(finishedSpy.wait(2000));
    QCOMPARE(server.requests.size(), 2);
    QCOMPARE(finishedSpy.at(0).at(0).toBool(), false);
}

QTEST_MAIN(QSOApiKeySenderTest)

#include "tst_qsoapikeysender.moc"
