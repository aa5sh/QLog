#ifndef QLOG_CORE_WSJTXUDPRECEIVER_H
#define QLOG_CORE_WSJTXUDPRECEIVER_H

#include <QObject>
#include <QDateTime>
#include <QHostAddress>
#include <QSqlRecord>
#include <QNetworkDatagram>

#include "data/UpdatableSQLRecord.h"
#include "data/WsjtxStatus.h"
#include "data/WsjtxDecode.h"
#include "data/WsjtxLog.h"
#include "data/WsjtxLogADIF.h"
#include "data/WsjtxEntry.h"

class Data;
class QUdpSocket;

class WsjtxUDPReceiver : public QObject
{
    Q_OBJECT
public:
    explicit WsjtxUDPReceiver(QObject *parent = nullptr);
    static float modePeriodLength(const QString &);

    static quint16 getConfigPort();
    static void saveConfigPort(quint16);
    static QString getConfigForwardAddresses();
    static void saveConfigForwardAddresses(const QString &);
    static void saveConfigMulticastJoin(bool);
    static bool getConfigMulticastJoin();
    static void saveConfigMulticastAddress(const QString &);
    static QString getConfigMulticastAddress();
    static void saveConfigMulticastTTL(int);
    static int getConfigMulticastTTL();

    // identification of different variants of the WSJTX protocol based on packet ID
    static bool isJTDXId(const QString &id)
    {
        return id.contains("JTDX");
    }
    static bool isWriteLogId(const QString &id)
    {
        return id.contains("WRITELOG");
    }
    static bool isCSNSat(const QString &data)
    {
        return data.contains("<programid:6>CSN Sat");
    }

signals:
    void statusReceived(WsjtxStatus);
    void decodeReceived(WsjtxDecode);
    void addContact(QSqlRecord);

public slots:
    void startReply(WsjtxEntry);
    void reloadSetting();

private slots:
    void readPendingDatagrams();
    void insertContact(WsjtxLog log);
    void contactReady(QSqlRecord record);
    void insertContact(WsjtxLogADIF log);

private:
    QUdpSocket* socket;
    QHostAddress wsjtxAddress;
    quint16 wsjtxPort;

    UpdatableSQLRecord wsjtSQLRecord;

    static int DEFAULT_PORT;
    static QString CONFIG_MULTICAST_TTL;

    void openPort();
    void forwardDatagram(const QNetworkDatagram &);
};

#endif // QLOG_CORE_WSJTXUDPRECEIVER_H
