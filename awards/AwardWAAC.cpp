#include <QCoreApplication>
#include "AwardWAAC.h"

// All AF Entities including deleted Penguin (493) and Walwis (488).
// Added also old Southern Sudan (244) what is newly Republic of South Sudan (521)

QString AwardWAAC::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "WAAC");
}

QString AwardWAAC::rulesUrl() const
{
    return QStringLiteral("https://sites.google.com/site/ik7nxm/IK7NXM");
}

QString AwardWAAC::headersColumns(const QString &) const
{
    return QStringLiteral("d.name col1, d.prefix col2 ");
}

QString AwardWAAC::sqlDetailTable(const QString &entity) const
{
    return " FROM dxcc_entities_clublog d"
           "   LEFT OUTER JOIN source_contacts c ON d.id = c.dxcc AND c.my_dxcc = '" + entity + "' AND d.cont = 'AF' AND (d.deleted = 0 OR d.id IN (493, 488, 244)) "
           "   LEFT OUTER JOIN modes m on c.mode = m.name";
}

QString AwardWAAC::additionalWhere(const QString &) const
{
    return " AND d.cont = 'AF' AND (d.deleted = 0 OR d.id IN (493, 488, 244)) ";
}

QString AwardWAAC::clickFilter(const QString &, const QString &col2Value) const
{
    return QString("dxcc = '%1' ").arg(col2Value);
}
