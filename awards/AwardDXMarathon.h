#ifndef QLOG_AWARDS_AWARDDXMARATHON_H
#define QLOG_AWARDS_AWARDDXMARATHON_H

#include "AwardDefinition.h"

class QComboBox;
class QSpinBox;
class QSqlQueryModel;
class QTableView;
class QLabel;
class QSqlQuery;

class AwardDXMarathon : public AwardDefinition
{
public:
    QString key() const override { return QStringLiteral("dxmarathon"); }
    QString displayName() const override;
    QString rulesUrl() const override;
    bool entityInputEnabled() const override { return false; }
    bool notWorkedEnabled() const override { return false; }
    QWidget *createWidget(QWidget *parent) override;
    void updateData(const AwardFilterParams &params) override;
    ConditionResult getConditionSelected(const QModelIndex &index) const override;

private:
    void refresh();
    void exportAdif();
    // SQL condition for eligible contacts. With inlineValues the station
    // and year values are quoted into the text; otherwise they are
    // placeholders that bindPredicate() fills.
    QString contactPredicate(bool inlineValues) const;
    void bindPredicate(QSqlQuery &query) const;
    static QString entityKindLabel();
    static QString zoneKindLabel();
    static QString workedLabel();

    QComboBox *profileCombo = nullptr;
    QComboBox *profileMatchCombo = nullptr;
    QSpinBox *yearSpin = nullptr;
    QLabel *scoreLabel = nullptr;
    QTableView *resultTable = nullptr;
    QSqlQueryModel *resultModel = nullptr;
    QString userFilter;
};

#endif
