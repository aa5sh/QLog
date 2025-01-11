#ifndef QLOG_CORE_LOGPARAM_H
#define QLOG_CORE_LOGPARAM_H

#include <QObject>
#include <QDate>
#include <QVariant>
#include <QMutex>

#include "data/Data.h"
#include "models/LogbookModel.h"

class LogParam : public QObject
{
    Q_OBJECT
public:
    explicit LogParam(QObject *parent = nullptr);
    static bool setLOVParam(const QString &LOVName, const QVariant &value)
    {
        return setParam(LOVName, value);
    };
    static QDate getLOVaParam(const QString &LOVName)
    {
        return getParam(LOVName).toDate();
    }
    static bool setLastBackupDate(const QDate date)
    {
        return setParam("last_backup", date);
    };
    static QDate getLastBackupDate()
    {
        return getParam("last_backup").toDate();
    };
    static bool setLogID(const QString &id)
    {
        return setParam("logid", id);
    };
    static QString getLogID()
    {
        return getParam("logid").toString();
    };
    static bool setContestSeqnoType(const QVariant &data)
    {
        return setParam("contest/seqnotype", data);
    };
    static int getContestSeqnoType()
    {
        return getParam("contest/seqnotype", Data::SeqType::SINGLE).toInt();
    };
    static bool setContestManuDupeType(const QVariant &data)
    {
        return setParam("contest/dupetype", data);
    };
    static int getContestDupeType()
    {
        return getParam("contest/dupetype", Data::DupeType::ALL_BANDS).toInt();
    };
    static bool setContestLinkExchange(const QVariant &data)
    {
        return setParam("contest/linkexchangetype", data);
    };
    static int getContestLinkExchange()
    {
        return getParam("contest/linkexchangetype", LogbookModel::COLUMN_INVALID).toInt();
    };
    static bool setContestFilter(const QString &filterName)
    {
        return setParam("contest/filter", filterName);
    };
    static QString getContestFilter()
    {
        return getParam("contest/filter").toString();
    };
    static bool setContestID(const QString &contestID)
    {
        return setParam("contest/contestid", contestID);
    };
    static QString getContestID()
    {
        return getParam("contest/contestid", QString()).toString();
    };
    static bool setContestDupeDate(const QDateTime date)
    {
        return setParam("contest/dupeDate", date);
    };
    static QDateTime getContestDupeDate()
    {
        return getParam("contest/dupeDate").toDateTime();
    };
    static void removeConetstDupeDate()
    {
        removeParamGroup("contest/dupeDate");
    }
    static bool getDxccConfirmedByLotwState()
    {
        return getParam("others/dxccconfirmedbylotw", true).toBool();
    };
    static bool setDxccConfirmedByLotwState(bool state)
    {
        return setParam("others/dxccconfirmedbylotw", state);
    };
    static bool setDxccConfirmedByPaperState(bool state)
    {
        return setParam("others/dxccconfirmedbypaper", state);
    };
    static bool getDxccConfirmedByPaperState()
    {
        return getParam("others/dxccconfirmedbypaper", true).toBool();
    };
    static bool setDxccConfirmedByEqslState(bool state)
    {
        return setParam("others/dxccconfirmedbyeqsl", state);
    };
    static bool getDxccConfirmedByEqslState()
    {
        return getParam("others/dxccconfirmedbyeqsl", false).toBool();
    };
    static int getContestSeqno(const QString &band = QString())
    {
        return getParam(( band.isEmpty() ) ? "contest/seqnos/single"
                                           : QString("contest/seqnos/%1").arg(band), 1).toInt();
    }
    static bool setContestSeqno(int value, const QString &band = QString())
    {
        return setParam(( band.isEmpty() ) ? "contest/seqnos/single"
                                           : QString("contest/seqnos/%1").arg(band), value);
    }
    static void removeContestSeqno()
    {
        removeParamGroup("contest/seqnos/");
    }

private:
    static QCache<QString, QVariant> localCache;
    static QMutex cacheMutex;

    static bool setParam(const QString&, const QVariant &);
    static QVariant getParam(const QString&, const QVariant &defaultValue = QVariant());
    static void removeParamGroup(const QString&);
};

#endif // QLOG_CORE_LOGPARAM_H
