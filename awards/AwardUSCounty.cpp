#include <QCoreApplication>
#include "AwardUSCounty.h"

QString AwardUSCounty::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "US Counties");
}

QString AwardUSCounty::headersColumns(const QString &) const
{
    return QStringLiteral("d.subdivision_name col1, d.code col2 ");
}

QString AwardUSCounty::sqlDetailTable(const QString &) const
{
    return " FROM adif_enum_secondary_subdivision d "
           "   LEFT OUTER JOIN (SELECT * FROM source_contacts "
           "                    WHERE dxcc IN (291, 6, 110) "
           "                      AND cnty IS NOT NULL AND cnty != '') c "
           "     ON d.dxcc = c.dxcc AND upper(d.code) = upper(c.cnty) "
           "   LEFT OUTER JOIN modes m ON c.mode = m.name "
           " WHERE d.dxcc IN (291, 6, 110) ";
}

QString AwardUSCounty::additionalWhere(const QString &) const
{
    return QString();
}

QString AwardUSCounty::clickFilter(const QString &, const QString &col2Value) const
{
    return QString("upper(cnty) = upper('%1') and dxcc in (291, 6, 110)").arg(col2Value);
}
