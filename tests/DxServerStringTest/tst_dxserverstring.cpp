#include <QtTest>

#include "data/DxServerString.h"

class DxServerStringTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void invalidServerStrings_data();
    void invalidServerStrings();
    void validServerStrings_data();
    void validServerStrings();
};

void DxServerStringTest::initTestCase()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
}

void DxServerStringTest::invalidServerStrings_data()
{
    QTest::addColumn<QString>("serverString");

    const QStringList invalidStrings = {
        "",
        "1",
        "192.",
        "aa.",
        ".11",
        ".aa",
        ".11.11",
        ".aa.aa",
        "a1.11",
        "aa.cz:1234567",
        "aa.cz:-1234",
        "aa.cz:a234",
        "aa.cz#1234",
        "aa/aa@aa.cz:1234",
        "aa:1234567",
        "aa:-1234",
        "aa:a234",
        "aa#1234",
        "aa/aa@aa:1234",
        "192.168.2.1:1234567",
        "192.168.2.1:-1234",
        "192.168.2.1:a234",
        "192.168.2.1#1234",
        "aa/aa@192.168.2.1:1234",
        "a1@aaa@localhost:7000"
    };

    for (const QString &value : invalidStrings)
    {
        QTest::newRow(value.toUtf8().constData()) << value;
    }
}

void DxServerStringTest::invalidServerStrings()
{
    QFETCH(QString, serverString);

    const DxServerString server(serverString);
    QVERIFY(!DxServerString::isValidServerString(serverString));
    QVERIFY(!server.isValid());
    QCOMPARE(server.getUsername(), QString());
    QCOMPARE(server.getHostname(), QString());
    QCOMPARE(server.getPort(), 7300);
    QCOMPARE(server.getPasswordStorageKey(), QString());
}

void DxServerStringTest::validServerStrings_data()
{
    QTest::addColumn<QString>("serverString");
    QTest::addColumn<QString>("expectedUsername");
    QTest::addColumn<QString>("expectedHostname");
    QTest::addColumn<int>("expectedPort");
    QTest::addColumn<QString>("expectedPasswordKey");

    struct Case
    {
        const char *name;
        const char *server;
        const char *username;
        const char *hostname;
        int port;
    };

    const Case cases[] = {
        {"localhost_default", "localhost:7300", "", "localhost", 7300},
        {"localhost_user", "aa@localhost:7300", "aa", "localhost", 7300},
        {"localhost_useralpha", "a1aaa@localhost:7300", "a1aaa", "localhost", 7300},
        {"localhost_custom_port", "a1aaa@localhost:7000", "a1aaa", "localhost", 7000},
        {"localhost_mixed_case", "a1Aaa@localHost:7000", "a1Aaa", "localHost", 7000},
        {"loopback_default", "127.0.0.1:7300", "", "127.0.0.1", 7300},
        {"loopback_user", "aa@127.0.0.1:7300", "aa", "127.0.0.1", 7300},
        {"loopback_useralpha", "a1aaa@127.0.0.1:7300", "a1aaa", "127.0.0.1", 7300},
        {"loopback_custom_port", "a1aaa@127.0.0.1:7000", "a1aaa", "127.0.0.1", 7000},
        {"loopback_mixed_case", "a1Aaa@127.0.0.1:7000", "a1Aaa", "127.0.0.1", 7000},
        {"domain_default", "aa.cz:7300", "", "aa.cz", 7300},
        {"domain_user", "aa@aa.cz:7300", "aa", "aa.cz", 7300},
        {"domain_useralpha", "a1aaa@aa.cz:7300", "a1aaa", "aa.cz", 7300},
        {"domain_custom_port", "a1aaa@aa.cz:7000", "a1aaa", "aa.cz", 7000},
        {"domain_mixed_case", "a1Aaa@aa.cZ:7000", "a1Aaa", "aa.cZ", 7000},
        {"subdomain_default", "aa.bb.cz:7300", "", "aa.bb.cz", 7300},
        {"subdomain_user", "aa@aa.bb.cz:7300", "aa", "aa.bb.cz", 7300},
        {"subdomain_useralpha", "a1aaa@aa.bb.cz:7300", "a1aaa", "aa.bb.cz", 7300},
        {"subdomain_custom_port", "a1aaa@aa.bb.cz:7000", "a1aaa", "aa.bb.cz", 7000},
        {"subdomain_mixed_case", "a1Aaa@aa.bb.cZ:7000", "a1Aaa", "aa.bb.cZ", 7000}
    };

    for (const Case &c : cases)
    {
        QTest::newRow(c.name)
            << QString::fromLatin1(c.server)
            << QString::fromLatin1(c.username)
            << QString::fromLatin1(c.hostname)
            << c.port
            << QStringLiteral("%1:%2").arg(QString::fromLatin1(c.hostname)).arg(c.port);
    }
}

void DxServerStringTest::validServerStrings()
{
    QFETCH(QString, serverString);
    QFETCH(QString, expectedUsername);
    QFETCH(QString, expectedHostname);
    QFETCH(int, expectedPort);
    QFETCH(QString, expectedPasswordKey);

    const DxServerString server(serverString);

    QVERIFY(DxServerString::isValidServerString(serverString));
    QVERIFY(server.isValid());
    QCOMPARE(server.getUsername(), expectedUsername);
    QCOMPARE(server.getHostname(), expectedHostname);
    QCOMPARE(server.getPort(), expectedPort);
    QCOMPARE(server.getPasswordStorageKey(), expectedPasswordKey);
}

QTEST_APPLESS_MAIN(DxServerStringTest)

#include "tst_dxserverstring.moc"
