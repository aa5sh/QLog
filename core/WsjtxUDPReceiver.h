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
    static bool getConfigOutputColorCQSpot();
    static void saveConfigOutputColorCQSpot(bool);

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
    void sendReply(const WsjtxEntry&);
    void sendHighlightCallsign(const WsjtxEntry&);
    void sendClearHighlightCallsign(const WsjtxEntry&);
    void sendClearAllHighlightCallsign();
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
    bool isOutputColorCQSpotEnabled;
    UpdatableSQLRecord wsjtSQLRecord;

    static const int DEFAULT_PORT = 2237;
    const quint32 UDP_MAGIC_NUMBER = 0xadbccbda;
    const quint32 UDP_DEFAULT_SCHEMA_VERSION = 3;
    enum UDP_COMMANDS
    {
        REPLY_CMD = quint32(4),
        HIGHLIGHT_CALLSIGN_CMD = quint32(13),
    };

    void openPort();
    void forwardDatagram(const QNetworkDatagram &);
    void sendHighlightCallsignColor(const WsjtxEntry &entry,
                                    const QColor &fgColor,
                                    const QColor &bgColor,
                                    bool highlightLast = true);
};

#endif // QLOG_CORE_WSJTXUDPRECEIVER_H
