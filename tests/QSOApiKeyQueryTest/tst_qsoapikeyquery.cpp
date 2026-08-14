#include <QtTest>
#include <QSqlRecord>
#include <QSqlField>
#include <QDateTime>
#include <QTimeZone>

#include "core/QSOApiKeyQuery.h"

class QSOApiKeyQueryTest : public QObject
{
    Q_OBJECT

private slots:
    void buildParams_flattensCoreQSOFieldsAndDerivesQsoDateTimeOn();
    void buildParams_omitsFieldsNotPresentInRecord();

private:
    static void appendField(QSqlRecord &record, const QString &name, const QVariant &value);
    static QString valueOf(const QList<QPair<QString, QString>> &params, const QString &key);
};

void QSOApiKeyQueryTest::appendField(QSqlRecord &record, const QString &name, const QVariant &value)
{
    QSqlField field(name, value.type());
    field.setValue(value);
    record.append(field);
}

QString QSOApiKeyQueryTest::valueOf(const QList<QPair<QString, QString>> &params, const QString &key)
{
    for (const auto &p : params)
    {
        if (p.first == key)
            return p.second;
    }
    return QString();
}

void QSOApiKeyQueryTest::buildParams_flattensCoreQSOFieldsAndDerivesQsoDateTimeOn()
{
    QSqlRecord record;
    appendField(record, QStringLiteral("callsign"), QStringLiteral("OK1AA"));
    appendField(record, QStringLiteral("band"), QStringLiteral("20M"));
    appendField(record, QStringLiteral("mode"), QStringLiteral("FT8"));
    appendField(record, QStringLiteral("rst_sent"), QStringLiteral("599"));
    appendField(record, QStringLiteral("rst_rcvd"), QStringLiteral("599"));
    appendField(record, QStringLiteral("gridsquare"), QStringLiteral("JO70AA"));
    appendField(record, QStringLiteral("comment"), QString());
    appendField(record, QStringLiteral("start_time"),
                QDateTime(QDate(2026, 8, 14), QTime(12, 0, 0), QTimeZone::utc()));

    const auto params = QSOApiKeyQuery::buildParams(record);

    QCOMPARE(valueOf(params, QStringLiteral("call")), QStringLiteral("OK1AA"));
    // AdiFormat's export lower-cases band ("20M" -> "20m")
    QCOMPARE(valueOf(params, QStringLiteral("band")), QStringLiteral("20m"));
    QCOMPARE(valueOf(params, QStringLiteral("mode")), QStringLiteral("FT8"));
    QCOMPARE(valueOf(params, QStringLiteral("rst_sent")), QStringLiteral("599"));
    QCOMPARE(valueOf(params, QStringLiteral("rst_rcvd")), QStringLiteral("599"));
    QCOMPARE(valueOf(params, QStringLiteral("gridsquare")), QStringLiteral("JO70AA"));
    QCOMPARE(valueOf(params, QStringLiteral("qso_date")), QStringLiteral("20260814"));
    QCOMPARE(valueOf(params, QStringLiteral("time_on")), QStringLiteral("120000"));

    // empty ADIF fields (comment here) must never show up as an empty-value pair
    for (const auto &p : params)
        QVERIFY(!p.second.isEmpty());
}

void QSOApiKeyQueryTest::buildParams_omitsFieldsNotPresentInRecord()
{
    QSqlRecord record;
    appendField(record, QStringLiteral("callsign"), QStringLiteral("OK1AA"));
    appendField(record, QStringLiteral("band"), QStringLiteral("40m"));

    const auto params = QSOApiKeyQuery::buildParams(record);

    QCOMPARE(params.size(), 2);
    QCOMPARE(valueOf(params, QStringLiteral("call")), QStringLiteral("OK1AA"));
    QCOMPARE(valueOf(params, QStringLiteral("band")), QStringLiteral("40m"));
}

QTEST_APPLESS_MAIN(QSOApiKeyQueryTest)

#include "tst_qsoapikeyquery.moc"
