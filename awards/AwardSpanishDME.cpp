#include <QCoreApplication>
#include "AwardSpanishDME.h"

QString AwardSpanishDME::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "Spanish DMEs");
}

QString AwardSpanishDME::headersColumns(const QString &) const
{
    return QStringLiteral("d.subdivision_name col1, d.code col2 ");
}

QString AwardSpanishDME::sqlDetailTable(const QString &) const
{
    return " FROM adif_enum_secondary_subdivision d "
           "   LEFT OUTER JOIN (SELECT * FROM source_contacts "
           "                    WHERE dxcc IN (21, 29, 32, 281) "
           "                      AND cnty IS NOT NULL AND cnty != '') c "
           "     ON d.dxcc = c.dxcc AND upper(d.code) = upper(c.cnty) "
           "   LEFT OUTER JOIN modes m ON c.mode = m.name "
           " WHERE d.dxcc IN (21, 29, 32, 281) ";
}

QString AwardSpanishDME::additionalWhere(const QString &) const
{
    return QString();
}

QString AwardSpanishDME::clickFilter(const QString &, const QString &col2Value) const
{
    return QString("upper(cnty) = upper('%1') and dxcc in (21, 29, 32, 281)").arg(col2Value);
}
