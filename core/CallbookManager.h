#ifndef QLOG_CORE_CALLBOOKMANAGER_H
#define QLOG_CORE_CALLBOOKMANAGER_H

#include <QObject>
#include <QPointer>
#include "service/GenericCallbook.h"

class CallbookManager : public QObject
{
    Q_OBJECT
public:
    explicit CallbookManager(QObject *parent = nullptr);

    void queryCallsign(const QString &callsign);
    bool isActive();

signals:
    void loginFailed(QString);
    void callsignResult(CallbookResponseData);
    void callsignNotFound(QString);
    void lookupError(QString);

public slots:
    void initCallbooks();
    void abortQuery();

private slots:
    void primaryCallbookCallsignNotFound(const QString&);
    void secondaryCallbookCallsignNotFound(const QString&);
    void processCallsignResult(const CallbookResponseData &data);
    void processQSLInfoResult(const CallbookResponseData &data);
    void qslInfoCallsignNotFound(const QString &callsign);
    void qslInfoLookupError(const QString &error);

private:
    GenericCallbook *createCallbook(const QString&);
    void requestQSLInfo(const CallbookResponseData &data,
                        bool baseResultAvailable);
    void completeCallsignResult(const CallbookResponseData &data);
    void completeCallsignNotFound(const QString &callsign);
    void disposeCallbook(QPointer<GenericCallbook> &callbook);

private:
    QPointer<GenericCallbook> primaryCallbook;
    bool primaryCallbookAuthSuccess;
    QPointer<GenericCallbook> secondaryCallbook;
    bool secondaryCallbookAuthSuccess;
    QPointer<GenericCallbook> qslInfoCallbook;
    CallbookResponseData pendingCallbookData;
    bool pendingCallbookResultAvailable;
    bool waitingForQSLInfo;
    QString currentQueryCallsign;
    static QCache<QString, CallbookResponseData> queryCache;

};

#endif // QLOG_CORE_CALLBOOKMANAGER_H
