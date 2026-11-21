#ifndef QLOG_DATA_DXMARATHON_H
#define QLOG_DATA_DXMARATHON_H

#include <QDateTime>
#include <QStringList>

// DX Marathon eligibility rules, shared by the award and the alert filter
// so they cannot drift apart. See https://www.dxmarathon.com/rules/
class DXMarathon
{
public:
    static const QStringList &eligibleBands()
    {
        static const QStringList bands = { QStringLiteral("160m"), QStringLiteral("80m"), QStringLiteral("60m"),
                                           QStringLiteral("40m"), QStringLiteral("30m"), QStringLiteral("20m"),
                                           QStringLiteral("17m"), QStringLiteral("15m"), QStringLiteral("12m"),
                                           QStringLiteral("10m"), QStringLiteral("6m") };
        return bands;
    }

    // Aeronautical and maritime mobile contacts do not count.
    static bool isEligibleCallsign(const QString &callsign)
    {
        return !callsign.endsWith(QLatin1String("/AM"), Qt::CaseInsensitive)
            && !callsign.endsWith(QLatin1String("/MM"), Qt::CaseInsensitive);
    }

    // SQL condition on the contacts table equivalent to the two checks
    // above. Built only from the constant band list, never from user data.
    static QString eligibleContactsCondition()
    {
        return QStringLiteral("band IN ('") + eligibleBands().join(QStringLiteral("','"))
             + QStringLiteral("') AND UPPER(callsign) NOT LIKE '%/AM' AND UPPER(callsign) NOT LIKE '%/MM'");
    }

    // The Marathon year is a UTC calendar year.
    static int currentYear()
    {
        return QDateTime::currentDateTimeUtc().date().year();
    }

    // Lower bound for contacts.start_time in the given year.
    static QString yearStart(int year)
    {
        return QStringLiteral("%1-01-01T00:00:00").arg(year);
    }
};

#endif // QLOG_DATA_DXMARATHON_H
