#ifndef QLOG_AWARDS_AWARDDXMARATHON_H
#define QLOG_AWARDS_AWARDDXMARATHON_H

#include "AwardDefinition.h"

class QComboBox;
class QSpinBox;
class QSqlQueryModel;
class QTableView;
class QLabel;

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
    QString contactPredicate() const;

    QComboBox *m_profile = nullptr;
    QComboBox *m_profileMatch = nullptr;
    QSpinBox *m_year = nullptr;
    QLabel *m_score = nullptr;
    QTableView *m_table = nullptr;
    QSqlQueryModel *m_model = nullptr;
    QString m_userFilter;
};

#endif
