#ifndef QLOG_CORE_MQTTCLIENT_H
#define QLOG_CORE_MQTTCLIENT_H

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QSslError>
#include <QString>
#include <QStringList>

class QSslSocket;
class QTimer;

/* Minimal MQTT 3.1.1 subscriber over TLS.
 *
 * The client intentionally supports QoS 0 subscriptions only. This keeps the
 * implementation small: incoming PUBLISH packets never require PUBACK/PUBREC
 * state, while the broker still delivers every message available at QoS 0.
 */
class MqttClient : public QObject
{
    Q_OBJECT

public:
    explicit MqttClient(const QString &hostName,
                        quint16 port,
                        QObject *parent = nullptr);

    void setSubscriptions(const QStringList &topics);

signals:
    void messageReceived(QString topic, QByteArray payload);

private slots:
    void connectToBroker();
    void socketEncrypted();
    void socketDisconnected();
    void socketReadyRead();
    void socketError();
    void socketSslErrors(const QList<QSslError> &errors);
    void connectionTimedOut();
    void keepAliveTimedOut();

private:
    /* The first byte of every MQTT packet contains the packet type in its
     * upper four bits and packet-specific flags in its lower four bits. These
     * values are complete first bytes, including the required flags. */
    enum PacketHeader : quint8
    {
        ConnectHeader = 0x10,
        ConnAckHeader = 0x20,
        SubscribeHeader = 0x82, // MQTT requires flags 0010 for SUBSCRIBE.
        SubAckHeader = 0x90,
        PingReqHeader = 0xc0,
        PingRespHeader = 0xd0
    };

    /* Packet type obtained by shifting the first fixed-header byte right by
     * PacketTypeShift. Only packet types used by this subscriber are listed. */
    enum class PacketType : quint8
    {
        ConnAck = 2,
        Publish = 3,
        SubAck = 9,
        PingResp = 13
    };

    // Values defined by the MQTT 3.1.1 wire format.
    enum MqttConstant
    {
        MqttProtocolLevel = 4,
        CleanSessionFlag = 0x02,
        QosAtMostOnce = 0,
        ConnectionAccepted = 0,
        SubscriptionRejected = 0x80,
        PacketTypeShift = 4,
        PublishQosShift = 1,
        PublishQosMask = 0x03,
        RemainingLengthBase = 128,
        RemainingLengthValueMask = 0x7f,
        RemainingLengthContinuationBit = 0x80,
        MaximumRemainingLengthBytes = 4,
        MaximumMqttStringLength = 0xffff
    };

    // Local connection policy; these values are not imposed by MQTT.
    enum ClientPolicy
    {
        MaximumPacketSize = 1024 * 1024,
        KeepAliveSeconds = 60,
        KeepAliveTimerIntervalMs = 30000, // Half of KeepAliveSeconds.
        ConnectionTimeoutMs = 30000,
        InitialReconnectDelayMs = 1000,
        MaximumReconnectDelayMs = 30000,
        ReconnectBackoffMultiplier = 2,
        ClientIdRandomLength = 16
    };

    enum class State
    {
        Disconnected,
        Connecting,
        AwaitingConnAck,
        AwaitingSubAck,
        Subscribed
    };

    void restartConnection();
    void scheduleReconnect();
    void sendConnect();
    void sendSubscribe();
    void processPacket(quint8 header, const QByteArray &body);
    void processPublish(quint8 header, const QByteArray &body);
    void protocolError(const QString &message);
    void writePacket(quint8 header, const QByteArray &body);

    static QByteArray encodeRemainingLength(int length);
    static QByteArray encodeString(const QString &value);
    static void appendUInt16(QByteArray &data, quint16 value);
    static quint16 readUInt16(const QByteArray &data);

    QString hostName;
    quint16 port;
    QStringList subscriptions;
    QString clientId;
    QSslSocket *socket;
    QTimer *reconnectTimer;
    QTimer *connectionTimer;
    QTimer *keepAliveTimer;
    QByteArray receiveBuffer;
    State state;
    quint16 packetIdentifier;
    quint16 pendingSubscribeIdentifier;
    int reconnectDelayMs;
    bool pingOutstanding;
};

#endif // QLOG_CORE_MQTTCLIENT_H
