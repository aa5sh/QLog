#include <QCoreApplication>
#include "AwardJapan.h"

QString AwardJapan::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "Japanese Cities/Ku/Guns");
}

QString AwardJapan::headersColumns(const QString &) const
{
    return QStringLiteral("d.subdivision_name col1, d.code col2 ");
}

QString AwardJapan::sqlDetailTable(const QString &) const
{
    return " FROM adif_enum_secondary_subdivision d "
           "   LEFT OUTER JOIN (SELECT * FROM source_contacts "
           "                    WHERE dxcc = 339 "
           "                      AND cnty IS NOT NULL AND cnty != '') c "
           "     ON d.dxcc = c.dxcc AND upper(d.code) = upper(c.cnty) "
           "   LEFT OUTER JOIN modes m ON c.mode = m.name "
           " WHERE d.dxcc = 339 ";
}

QString AwardJapan::additionalWhere(const QString &) const
{
    return QString();
}

QString AwardJapan::clickFilter(const QString &, const QString &col2Value) const
{
    return QString("upper(cnty) = upper('%1') and dxcc = 339").arg(col2Value);
}
