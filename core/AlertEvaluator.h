#ifndef QLOG_CORE_ALERTEVALUATOR_H
#define QLOG_CORE_ALERTEVALUATOR_H

#include <QObject>
#include "data/DxSpot.h"
#include "data/WsjtxEntry.h"
#include "data/SpotAlert.h"
#include <QRegularExpression>

class AlertRule : public QObject
{
    Q_OBJECT

public:
    explicit AlertRule(QObject *parent = nullptr);
    ~AlertRule() {};

    bool save();
    bool load(const QString &);
    bool match(const WsjtxEntry &wsjtx) const;
    bool match(const DxSpot & spot) const;
    bool isValid() const;
    operator QString() const;
public:
    QString ruleName;
    bool enabled;
    int sourceMap;
    QString dxCallsign;
    int dxCountry;
    int dxLogStatusMap;
    QString dxContinent;
    QString dxComment;
    QStringList dxMember;
    QString mode;
    QString band;
    int spotterCountry;
    QString spotterContinent;
    int ituz;
    int cqz;
    bool pota;
    bool sota;
    bool iota;
    bool wwff;

private:
    bool ruleValid;
    QRegularExpression callsignRE;
    QRegularExpression commentRE;
    QSet<QString> dxMemberSet;

};

class AlertEvaluator : public QObject
{
    Q_OBJECT
public:
    explicit AlertEvaluator(QObject *parent = nullptr);
    ~AlertEvaluator() {clearRules();}

    void clearRules();

public slots:
    void dxSpot(const DxSpot&);
    void WSJTXCQSpot(const WsjtxEntry&);
    void loadRules();

signals:
    void spotAlert(SpotAlert alert);

private:
    QList<AlertRule *>ruleList;
};

#endif // QLOG_CORE_ALERTEVALUATOR_H
