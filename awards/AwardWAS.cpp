#include <QCoreApplication>
#include "AwardWAS.h"

QString AwardWAS::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "WAS");
}

QString AwardWAS::headersColumns(const QString &) const
{
    return QStringLiteral("d.subdivision_name col1, d.code col2 ");
}

QString AwardWAS::sqlDetailTable(const QString &entity) const
{
    return " FROM adif_enum_primary_subdivision d"
           "   LEFT OUTER JOIN source_contacts c ON d.dxcc = c.dxcc AND d.code = c.state"
           "   LEFT OUTER JOIN modes m on c.mode = m.name"
           " WHERE (c.id is NULL or c.my_dxcc = '" + entity + "' AND d.dxcc in (6, 110, 291)) ";
}

QString AwardWAS::additionalWhere(const QString &entity) const
{
    return " AND (c.id is NULL or c.my_dxcc = '" + entity + "' AND c.dxcc in (6, 110, 291)) ";
}

QString AwardWAS::clickFilter(const QString &, const QString &col2Value) const
{
    return QString("state = '%1' and dxcc in (6, 110, 291)").arg(col2Value);
}
