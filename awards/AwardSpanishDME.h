#ifndef QLOG_AWARDS_AWARDSPANISHDME_H
#define QLOG_AWARDS_AWARDSPANISHDME_H

#include "BandTableAward.h"

class AwardSpanishDME : public BandTableAward
{
public:
    QString key() const override { return QStringLiteral("spanishdme"); }
    QString displayName() const override;
    bool entityInputEnabled() const override { return false; }

protected:
    QString headersColumns(const QString &entity) const override;
    QString sqlDetailTable(const QString &entity) const override;
    QString additionalWhere(const QString &entity) const override;
    QString clickFilter(const QString &col1Value, const QString &col2Value) const override;
};

#endif // QLOG_AWARDS_AWARDSPANISHDME_H
