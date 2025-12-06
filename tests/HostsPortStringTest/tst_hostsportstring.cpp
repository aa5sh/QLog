#include <QtTest>

#include "data/HostsPortString.h"

using AddressExpectation = QList<QPair<QString, quint16>>;
Q_DECLARE_METATYPE(AddressExpectation)

class HostsPortStringTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void regexValidation_data();
    void regexValidation();
    void parsing_data();
    void parsing();
    void hasLocalIPWithPort_data();
    void hasLocalIPWithPort();
    void hostPortAddress_basic();
    void hostPortAddress_equality();
};

void HostsPortStringTest::initTestCase()
{
    qRegisterMetaType<QList<QPair<QString, quint16>>>("AddressExpectation");
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
}

void HostsPortStringTest::regexValidation_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<bool>("expectedMatch");

    struct Case { const char *name; const char *input; bool match; } cases[] = {
        {"single_address", "127.0.0.1:80", true},
        {"multiple_addresses", "192.168.1.1:1 10.0.0.1:65535", true},
        {"trailing_space", "10.10.10.10:12345 ", true},
        {"empty_string", "", false},
        {"missing_port", "127.0.0.1", false},
        {"invalid_ip", "300.0.0.1:80", false},
        {"hostname", "example.com:80", false}
    };

    for (const Case &c : cases)
    {
        QTest::newRow(c.name) << QString::fromLatin1(c.input) << c.match;
    }
}

void HostsPortStringTest::regexValidation()
{
    QFETCH(QString, input);
    QFETCH(bool, expectedMatch);

    const QRegularExpression regex = HostsPortString::hostsPortRegEx();
    const bool actual = regex.match(input).hasMatch();
    QCOMPARE(actual, expectedMatch);
}

void HostsPortStringTest::parsing_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<AddressExpectation>("expected");

    QTest::newRow("empty") << QString() << AddressExpectation{};

    AddressExpectation basic;
    basic << qMakePair(QStringLiteral("127.0.0.1"), quint16(80))
          << qMakePair(QStringLiteral("10.0.0.5"), quint16(123));
    QTest::newRow("basic_valid") << QStringLiteral("127.0.0.1:80 10.0.0.5:123") << basic;

    AddressExpectation filtered;
    filtered << qMakePair(QStringLiteral("127.0.0.1"), quint16(80))
             << qMakePair(QStringLiteral("1.1.1.1"), quint16(65535));
    QTest::newRow("mixed_tokens") << QStringLiteral("127.0.0.1:80 invalid 192.168.0.1:-1 0.0.0.0:70000 1.1.1.1:65535")
                                  << filtered;
}

void HostsPortStringTest::parsing()
{
    QFETCH(QString, input);
    QFETCH(AddressExpectation, expected);

    const HostsPortString hosts(input);
    const QList<HostPortAddress> actual = hosts.getAddrList();

    QCOMPARE(actual.size(), expected.size());

    for (int i = 0; i < actual.size(); ++i)
    {
        QCOMPARE(actual.at(i).toString(), expected.at(i).first);
        QCOMPARE(actual.at(i).getPort(), expected.at(i).second);
    }
}

void HostsPortStringTest::hasLocalIPWithPort_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<int>("port");
    QTest::addColumn<bool>("expected");

    QTest::newRow("loopback_match") << QStringLiteral("127.0.0.1:4321") << 4321 << true;
    QTest::newRow("loopback_wrong_port") << QStringLiteral("127.0.0.1:4321") << 1111 << false;
    QTest::newRow("wildcard_match") << QStringLiteral("0.0.0.0:5555") << 5555 << true;
    QTest::newRow("non_local") << QStringLiteral("192.0.2.1:6000") << 6000 << false;
}

void HostsPortStringTest::hasLocalIPWithPort()
{
    QFETCH(QString, input);
    QFETCH(int, port);
    QFETCH(bool, expected);

    const HostsPortString hosts(input);
    QCOMPARE(hosts.hasLocalIPWithPort(port), expected);
}

void HostsPortStringTest::hostPortAddress_basic()
{
    HostPortAddress addr(QStringLiteral("127.0.0.1"), 1234);
    QCOMPARE(addr.getPort(), quint16(1234));

    addr.setPort(4321);
    QCOMPARE(addr.getPort(), quint16(4321));
    QCOMPARE(addr.toString(), QStringLiteral("127.0.0.1"));
}

void HostsPortStringTest::hostPortAddress_equality()
{
    const HostPortAddress addr1(QStringLiteral("127.0.0.1"), 80);
    const HostPortAddress addr2(QStringLiteral("127.0.0.1"), 80);
    const HostPortAddress addrDifferentPort(QStringLiteral("127.0.0.1"), 81);
    const HostPortAddress addrDifferentHost(QStringLiteral("10.0.0.1"), 80);

    QVERIFY(addr1 == addr2);
    QVERIFY(!(addr1 == addrDifferentPort));
    QVERIFY(!(addr1 == addrDifferentHost));
}

QTEST_APPLESS_MAIN(HostsPortStringTest)

#include "tst_hostsportstring.moc"
