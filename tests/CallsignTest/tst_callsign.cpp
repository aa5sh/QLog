#include <QtTest>
#include "data/Callsign.h"
#include "generated_cases.h"

class CallsignTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void parse_data();
    void parse();
    void wpx_data();
    void wpx();
};

void CallsignTest::initTestCase()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
}

void CallsignTest::parse_data()
{
    QTest::addColumn<QString>("callsign");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<QString>("normalized");
    QTest::addColumn<QString>("hostPrefix");
    QTest::addColumn<QString>("hostPrefixWithDelimiter");
    QTest::addColumn<QString>("base");
    QTest::addColumn<QString>("basePrefix");
    QTest::addColumn<QString>("basePrefixNumber");
    QTest::addColumn<QString>("suffix");
    QTest::addColumn<QString>("suffixWithDelimiter");
    QTest::addColumn<QString>("wpxPrefix");

    int idx = 0;
    for (const ParseCase &parseCase : kParseCases)
    {
        const QString rowId = QStringLiteral("%1_%2").arg(parseCase.name).arg(idx++);
        const QByteArray rowBytes = rowId.toUtf8();
        QTest::newRow(rowBytes.constData())
                << QString::fromLatin1(parseCase.callsign)
                << parseCase.valid
                << QString::fromLatin1(parseCase.normalized)
                << QString::fromLatin1(parseCase.host)
                << QString::fromLatin1(parseCase.hostDelim)
                << QString::fromLatin1(parseCase.base)
                << QString::fromLatin1(parseCase.basePrefix)
                << QString::fromLatin1(parseCase.basePrefixNumber)
                << QString::fromLatin1(parseCase.suffix)
                << QString::fromLatin1(parseCase.suffixDelim)
                << QString::fromLatin1(parseCase.wpx);
    }
}

void CallsignTest::parse()
{
    QFETCH(QString, callsign);
    QFETCH(bool, valid);
    QFETCH(QString, normalized);
    QFETCH(QString, hostPrefix);
    QFETCH(QString, hostPrefixWithDelimiter);
    QFETCH(QString, base);
    QFETCH(QString, basePrefix);
    QFETCH(QString, basePrefixNumber);
    QFETCH(QString, suffix);
    QFETCH(QString, suffixWithDelimiter);
    QFETCH(QString, wpxPrefix);

    Callsign cs(callsign);

    QCOMPARE(cs.isValid(), valid);

    if (!valid)
    {
        QVERIFY(cs.getCallsign().isEmpty());
        QCOMPARE(cs.getWPXPrefix(), wpxPrefix);
        return;
    }

    QCOMPARE(cs.getCallsign(), normalized);
    QCOMPARE(cs.getHostPrefix(), hostPrefix);
    QCOMPARE(cs.getHostPrefixWithDelimiter(), hostPrefixWithDelimiter);
    QCOMPARE(cs.getBase(), base);
    QCOMPARE(cs.getBasePrefix(), basePrefix);
    QCOMPARE(cs.getBasePrefixNumber(), basePrefixNumber);
    QCOMPARE(cs.getSuffix(), suffix);
    QCOMPARE(cs.getSuffixWithDelimiter(), suffixWithDelimiter);
    QCOMPARE(cs.getWPXPrefix(), wpxPrefix);
}

void CallsignTest::wpx_data()
{
    QTest::addColumn<QString>("callsign");
    QTest::addColumn<QString>("expectedPrefix");

    int idx = 0;
    for (const WPXCase &wpxCase : kWPXCases)
    {
        const QString rowId = QStringLiteral("%1_%2").arg(wpxCase.name).arg(idx++);
        const QByteArray rowBytes = rowId.toUtf8();
        QTest::newRow(rowBytes.constData())
                << QString::fromLatin1(wpxCase.callsign)
                << QString::fromLatin1(wpxCase.wpx);
    }
}

void CallsignTest::wpx()
{
    QFETCH(QString, callsign);
    QFETCH(QString, expectedPrefix);

    Callsign cs(callsign);

    QCOMPARE(cs.getWPXPrefix(), expectedPrefix);
}

QTEST_APPLESS_MAIN(CallsignTest)

#include "tst_callsign.moc"
