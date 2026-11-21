#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

#include "service/emailqsl/EmailQSLService.h"

// Minimal plain-text SMTP server. Multi-line EHLO replies can be split
// across two writes to reproduce how real servers deliver them.
class FakeSmtpServer : public QObject
{
    Q_OBJECT

public:
    explicit FakeSmtpServer(bool splitReplies) : splitReplies(splitReplies)
    {
        connect(&server, &QTcpServer::newConnection, this, &FakeSmtpServer::onConnection);
        server.listen(QHostAddress::LocalHost);
    }

    quint16 port() const { return server.serverPort(); }

    QStringList commands;
    QByteArray message;
    int connections = 0;

private slots:
    void onConnection()
    {
        ++connections;
        client = server.nextPendingConnection();
        connect(client, &QTcpSocket::readyRead, this, &FakeSmtpServer::onRead);
        client->write("220 fake ESMTP\r\n");
    }

    void onRead()
    {
        input += client->readAll();
        while ( true )
        {
            if ( inData )
            {
                const int end = input.indexOf("\r\n.\r\n");
                if ( end < 0 )
                    return;
                message = input.left(end + 2);
                input.remove(0, end + 5);
                inData = false;
                client->write("250 queued\r\n");
                continue;
            }

            const int eol = input.indexOf("\r\n");
            if ( eol < 0 )
                return;
            const QByteArray line = input.left(eol);
            input.remove(0, eol + 2);
            handle(line);
        }
    }

private:
    void handle(const QByteArray &line)
    {
        if ( authStep == 1 ) { commands << QStringLiteral("<user>"); authStep = 2; client->write("334 UGFzc3dvcmQ6\r\n"); return; }
        if ( authStep == 2 ) { commands << QStringLiteral("<password>"); authStep = 0; client->write("235 ok\r\n"); return; }

        const QString command = QString::fromLatin1(line);
        commands << command;
        if ( command.startsWith(QLatin1String("EHLO")) )
        {
            client->write("250-fake\r\n");
            if ( splitReplies )
                QTimer::singleShot(100, client, [this]() { client->write("250 AUTH LOGIN\r\n"); });
            else
                client->write("250 AUTH LOGIN\r\n");
        }
        else if ( command == QLatin1String("AUTH LOGIN") )
        {
            authStep = 1;
            client->write("334 VXNlcm5hbWU6\r\n");
        }
        else if ( command.startsWith(QLatin1String("MAIL FROM")) || command.startsWith(QLatin1String("RCPT TO")) )
            client->write("250 ok\r\n");
        else if ( command == QLatin1String("DATA") )
        {
            inData = true;
            client->write("354 go ahead\r\n");
        }
        else if ( command == QLatin1String("QUIT") )
        {
            client->write("221 bye\r\n");
            client->disconnectFromHost();
        }
        else
            client->write("500 unknown\r\n");
    }

    QTcpServer server;
    QTcpSocket *client = nullptr;
    QByteArray input;
    bool splitReplies;
    bool inData = false;
    int authStep = 0;
};

class EmailQSLTest : public QObject
{
    Q_OBJECT

private slots:
    void finalReplyLine();
    void headerEncoding();
    void addressValidation();
    void testOnlyStopsAfterAuth();
    void sendsMessageWithHeaders();
    void rejectsInjectedRecipient();

private:
    bool runWorker(SmtpWorker *worker, QString *error);
};

bool EmailQSLTest::runWorker(SmtpWorker *worker, QString *error)
{
    QThread thread;
    worker->moveToThread(&thread);
    bool done = false;
    bool ok = false;
    connect(&thread, &QThread::started, worker, &SmtpWorker::run);
    connect(worker, &SmtpWorker::finished, this, [&](bool success, const QString &message)
    {
        done = true;
        ok = success;
        if ( error )
            *error = message;
    }, Qt::QueuedConnection);
    thread.start();
    const bool finished = QTest::qWaitFor([&]() { return done; }, 20000);
    thread.quit();
    thread.wait();
    delete worker;
    return finished && ok;
}

void EmailQSLTest::finalReplyLine()
{
    QVERIFY(!SmtpWorker::hasFinalReplyLine("250-fake\r\n"));
    QVERIFY(!SmtpWorker::hasFinalReplyLine("250-fake\r\n250 AUTH"));
    QVERIFY(SmtpWorker::hasFinalReplyLine("250-fake\r\n250 AUTH LOGIN\r\n"));
    QVERIFY(SmtpWorker::hasFinalReplyLine("220\r\n"));
    QVERIFY(!SmtpWorker::hasFinalReplyLine(""));
}

void EmailQSLTest::headerEncoding()
{
    QCOMPARE(SmtpWorker::encodeHeaderText(QStringLiteral("QSL")), QByteArray("=?UTF-8?B?UVNM?="));

    // Long non-ASCII text: every encoded-word within 75 characters, no UTF-8
    // sequence split between words, and the words decode to the original.
    const QString text = QString(60, QChar(0x00E9)) + QStringLiteral(" Ołomuniec ") + QString(10, QChar(0x20AC));
    const QByteArray encoded = SmtpWorker::encodeHeaderText(text);
    QByteArray decoded;
    for ( const QByteArray &word : encoded.split('\n') )
    {
        const QByteArray trimmed = word.trimmed();
        QVERIFY2(trimmed.size() <= 75, trimmed.constData());
        QVERIFY(trimmed.startsWith("=?UTF-8?B?") && trimmed.endsWith("?="));
        const QByteArray bytes = QByteArray::fromBase64(trimmed.mid(10, trimmed.size() - 12));
        QCOMPARE(QString::fromUtf8(bytes).toUtf8(), bytes);
        decoded += bytes;
    }
    QCOMPARE(QString::fromUtf8(decoded), text);
}

void EmailQSLTest::addressValidation()
{
    QVERIFY(SmtpWorker::isSafeAddress(QStringLiteral("ok1abc@example.com")));
    QVERIFY(!SmtpWorker::isSafeAddress(QString()));
    QVERIFY(!SmtpWorker::isSafeAddress(QStringLiteral("nobody")));
    QVERIFY(!SmtpWorker::isSafeAddress(QStringLiteral("a@b.c\r\nRCPT TO:<x@y.z>")));
    QVERIFY(!SmtpWorker::isSafeAddress(QStringLiteral("a@b.c>\r\nDATA")));
    QVERIFY(!SmtpWorker::isSafeAddress(QStringLiteral("a b@c.d")));
}

void EmailQSLTest::testOnlyStopsAfterAuth()
{
    FakeSmtpServer server(true);
    SmtpWorker *worker = new SmtpWorker(QStringLiteral("127.0.0.1"), server.port(), EmailQSLBase::ENCRYPTION_NONE, false,
                                        QStringLiteral("user"), QStringLiteral("secret"),
                                        QString(), QString(), QString(), QString(), QString(),
                                        QByteArray(), QString());
    worker->setTestOnly(true);

    QString error;
    QVERIFY2(runWorker(worker, &error), qPrintable(error));
    QTRY_VERIFY(server.commands.contains(QStringLiteral("QUIT")));
    QVERIFY(server.commands.contains(QStringLiteral("<password>")));
    for ( const QString &command : server.commands )
        QVERIFY2(!command.startsWith(QLatin1String("MAIL")) && command != QLatin1String("DATA"), qPrintable(command));
}

void EmailQSLTest::sendsMessageWithHeaders()
{
    // Split EHLO reply: the old reader returned after the first line and
    // treated the rest as the reply to AUTH LOGIN.
    FakeSmtpServer server(true);
    SmtpWorker *worker = new SmtpWorker(QStringLiteral("127.0.0.1"), server.port(), EmailQSLBase::ENCRYPTION_NONE, false,
                                        QStringLiteral("user"), QStringLiteral("secret"),
                                        QStringLiteral("aa5sh@example.com"), QStringLiteral("Mike Ołomuniec"),
                                        QStringLiteral("ok1abc@example.org"),
                                        QStringLiteral("QSL Card from AA5SH"), QStringLiteral("73"),
                                        QByteArray(), QString());

    QString error;
    QVERIFY2(runWorker(worker, &error), qPrintable(error));
    QVERIFY(server.commands.contains(QStringLiteral("MAIL FROM:<aa5sh@example.com>")));
    QVERIFY(server.commands.contains(QStringLiteral("RCPT TO:<ok1abc@example.org>")));

    const QByteArray &message = server.message;
    QVERIFY(message.contains("\r\nDate: ") || message.startsWith("Date: "));
    QVERIFY(message.contains("\r\nMessage-ID: <"));
    QVERIFY(message.contains("@example.com>\r\n"));
    QVERIFY(message.contains("From: " + SmtpWorker::encodeHeaderText(QStringLiteral("Mike Ołomuniec")) + " <aa5sh@example.com>\r\n"));
    QVERIFY(message.contains("Subject: =?UTF-8?B?"));
}

void EmailQSLTest::rejectsInjectedRecipient()
{
    FakeSmtpServer server(false);
    SmtpWorker *worker = new SmtpWorker(QStringLiteral("127.0.0.1"), server.port(), EmailQSLBase::ENCRYPTION_NONE, false,
                                        QStringLiteral("user"), QStringLiteral("secret"),
                                        QStringLiteral("aa5sh@example.com"), QString(),
                                        QStringLiteral("ok1abc@example.org>\r\nRCPT TO:<victim@example.net"),
                                        QStringLiteral("QSL"), QStringLiteral("73"),
                                        QByteArray(), QString());

    QString error;
    QVERIFY(!runWorker(worker, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(server.connections, 0);
}

QTEST_MAIN(EmailQSLTest)
#include "tst_emailqsl.moc"
