#include <QCoreApplication>
#include "AwardNZ.h"

QString AwardNZ::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "NZ Counties");
}

QString AwardNZ::headersColumns(const QString &) const
{
    return QStringLiteral("d.subdivision_name col1, d.code col2 ");
}

QString AwardNZ::sqlDetailTable(const QString &) const
{
    return " FROM adif_enum_secondary_subdivision d "
           "   LEFT OUTER JOIN (SELECT * FROM source_contacts "
           "                    WHERE dxcc = 170 "
           "                      AND cnty IS NOT NULL AND cnty != '') c "
           "     ON d.dxcc = c.dxcc AND upper(d.code) = upper(c.cnty) "
           "   LEFT OUTER JOIN modes m ON c.mode = m.name "
           " WHERE d.dxcc = 170 ";
}

QString AwardNZ::additionalWhere(const QString &) const
{
    return QString();
}

QString AwardNZ::clickFilter(const QString &, const QString &col2Value) const
{
    return QString("upper(cnty) = upper('%1') and dxcc = 170").arg(col2Value);
}
