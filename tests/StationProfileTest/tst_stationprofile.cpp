#include <QtTest>

#include "data/StationProfile.h"

class StationProfileTest : public QObject
{
    Q_OBJECT

private slots:
    void requiredFieldsAreAlwaysMatched();
    void populatedLocationFieldsUseContactColumns();
    void emptyOptionalFieldsAreIgnored();
};

void StationProfileTest::requiredFieldsAreAlwaysMatched()
{
    StationProfile profile;
    const QString join = profile.getContactInnerJoin();

    QVERIFY(join.contains(QStringLiteral("contacts.station_callsign = station_profiles.callsign")));
    QVERIFY(join.contains(QStringLiteral("contacts.my_gridsquare = station_profiles.locator")));
}

void StationProfileTest::populatedLocationFieldsUseContactColumns()
{
    StationProfile profile;
    profile.county = QStringLiteral("Example Parish");
    profile.darcDOK = QStringLiteral("A01");
    profile.operatorCallsign = QStringLiteral("N0CALL");

    const QString join = profile.getContactInnerJoin();
    QVERIFY(join.contains(QStringLiteral("contacts.my_cnty = station_profiles.county")));
    QVERIFY(join.contains(QStringLiteral("contacts.my_darc_dok = station_profiles.darc_dok")));
    QVERIFY(join.contains(QStringLiteral("contacts.operator = station_profiles.operator_callsign")));
    QVERIFY(!join.contains(QStringLiteral("darc_doc")));
}

void StationProfileTest::emptyOptionalFieldsAreIgnored()
{
    StationProfile profile;
    const QString join = profile.getContactInnerJoin();

    QVERIFY(!join.contains(QStringLiteral("contacts.my_cnty")));
    QVERIFY(!join.contains(QStringLiteral("contacts.my_darc_dok")));
    QVERIFY(!join.contains(QStringLiteral("contacts.operator =")));
}

QTEST_APPLESS_MAIN(StationProfileTest)
#include "tst_stationprofile.moc"
