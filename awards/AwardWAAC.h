#ifndef QLOG_AWARDS_AWARDWAAC_H
#define QLOG_AWARDS_AWARDWAAC_H

#include "BandTableAward.h"

class AwardWAAC : public BandTableAward
{
public:
    QString key() const override { return QStringLiteral("WAAC"); }
    QString displayName() const override;
    QString rulesUrl() const override;

protected:
    QString headersColumns(const QString &entity) const override;
    QString sqlDetailTable(const QString &entity) const override;
    QString additionalWhere(const QString &entity) const override;
    QString clickFilter(const QString &, const QString &) const override;
    bool clickUsesCountryName() const override;
};

#endif // QLOG_AWARDS_AWARDWAAC_H
