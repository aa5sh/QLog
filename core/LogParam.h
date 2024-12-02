#ifndef QLOG_CORE_LOGPARAM_H
#define QLOG_CORE_LOGPARAM_H

#include <QObject>
#include <QDate>
#include <QVariant>
#include <QMutex>

class LogParam : public QObject
{
    Q_OBJECT
public:
    explicit LogParam(QObject *parent = nullptr);

    static bool setParam(const QString&, const QVariant &);
    static QVariant getParam(const QString&, const QVariant &defaultValue = QVariant());
    static void removeParamGroup(const QString&);

private:
    static QCache<QString, QVariant> localCache;
    static QMutex cacheMutex;
};

#endif // QLOG_CORE_LOGPARAM_H
