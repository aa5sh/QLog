#include <QAbstractSocket>
#include <QSslSocket>
#include <QTimer>
#include <QtEndian>
#include <QUuid>

#include "MqttClient.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.core.mqttclient");

MqttClient::MqttClient(const QString &hostName,
                       quint16 port,
                       QObject *parent)
    : QObject(parent),
      hostName(hostName),
      port(port),
      /* A short UUID fragment is enough to avoid collisions while keeping the
       * generated client ID readable in broker and application logs. */
      clientId(QStringLiteral("qlog_%1").arg(QUuid::createUuid()
                                             .toString(QUuid::WithoutBraces)
                                             .remove(QLatin1Char('-'))
                                             .left(ClientIdRandomLength))),
      socket(new QSslSocket(this)),
      reconnectTimer(new QTimer(this)),
      connectionTimer(new QTimer(this)),
      keepAliveTimer(new QTimer(this)),
      state(State::Disconnected),
      packetIdentifier(0),
      pendingSubscribeIdentifier(0),
      reconnectDelayMs(InitialReconnectDelayMs),
      pingOutstanding(false)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << hostName << port;

    reconnectTimer->setSingleShot(true);
    connectionTimer->setSingleShot(true);
    /* Check at half of the advertised keepalive interval. This gives one full
     * timer period for PINGRESP before treating the connection as dead. */
    keepAliveTimer->setInterval(KeepAliveTimerIntervalMs);

    connect(reconnectTimer, &QTimer::timeout,
            this, &MqttClient::connectToBroker);
    connect(connectionTimer, &QTimer::timeout,
            this, &MqttClient::connectionTimedOut);
    connect(keepAliveTimer, &QTimer::timeout,
            this, &MqttClient::keepAliveTimedOut);
    connect(socket, &QSslSocket::encrypted,
            this, &MqttClient::socketEncrypted);
    connect(socket, &QSslSocket::readyRead,
            this, &MqttClient::socketReadyRead);
    connect(socket, &QSslSocket::disconnected,
            this, &MqttClient::socketDisconnected);
    connect(socket, QOverload<const QList<QSslError> &>::of(&QSslSocket::sslErrors),
            this, &MqttClient::socketSslErrors);
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QSslSocket::error),
            this, &MqttClient::socketError);
#else
    connect(socket, &QSslSocket::errorOccurred,
            this, &MqttClient::socketError);
#endif
}

void MqttClient::setSubscriptions(const QStringList &topics)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << topics;

    QStringList newSubscriptions = topics;
    newSubscriptions.removeAll(QString());
    newSubscriptions.removeDuplicates();

    if ( subscriptions == newSubscriptions )
        return;

    /* Reconnect instead of maintaining an UNSUBSCRIBE delta. Together with
     * Clean Session this guarantees that the broker keeps exactly this list. */
    subscriptions = newSubscriptions;
    restartConnection();
}

void MqttClient::connectToBroker()
{
    FCT_IDENTIFICATION;

    if ( subscriptions.isEmpty() )
        return;

    if ( socket->state() != QAbstractSocket::UnconnectedState )
    {
        qCDebug(runtime) << "MQTT TLS socket is already connecting or connected";
        return;
    }

    receiveBuffer.clear();
    pingOutstanding = false;
    state = State::Connecting;

    qCDebug(runtime) << "Connecting MQTT over TLS to" << hostName << port;
    socket->connectToHostEncrypted(hostName, port);
    /* The timeout covers DNS, TLS, CONNECT and SUBSCRIBE. A connection is only
     * useful to callers after the broker confirms all subscriptions. */
    connectionTimer->start(ConnectionTimeoutMs);
}

void MqttClient::socketEncrypted()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "MQTT TLS connection established to" << hostName << port;
    state = State::AwaitingConnAck;
    sendConnect();
}

void MqttClient::socketDisconnected()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "MQTT connection disconnected from" << hostName << port;
    scheduleReconnect();
}

void MqttClient::socketReadyRead()
{
    FCT_IDENTIFICATION;

    const QByteArray data = socket->readAll();
    qCDebug(function_parameters) << data.size();
    receiveBuffer.append(data);

    /* Every MQTT control packet transported by this TCP stream has this form:
     *
     *   byte 1       : packet type (bits 7..4) and packet flags (bits 3..0)
     *   bytes 2..5   : Remaining Length, encoded in one to four bytes
     *   following N  : variable header and payload, N = Remaining Length
     *
     * Remaining Length does not include byte 1 or its own encoded bytes. TCP
     * may split one MQTT packet across reads or join several packets into one
     * read, so incomplete data stays in receiveBuffer until N bytes arrive. */
    while ( !receiveBuffer.isEmpty() )
    {
        if ( receiveBuffer.size() < 2 )
            return;

        int remainingLength = 0;
        int multiplier = 1;
        int index = 1;
        int lengthBytes = 0;
        bool lengthComplete = false;

        /* Each Remaining Length byte carries seven value bits. Bit 7 is the
         * continuation flag and the least-significant group is transmitted
         * first. For example, values 0..127 need one byte. */
        while ( index < receiveBuffer.size()
                && lengthBytes < MaximumRemainingLengthBytes )
        {
            const quint8 encodedByte = static_cast<quint8>(receiveBuffer.at(index));
            ++index;
            ++lengthBytes;
            remainingLength += (encodedByte & RemainingLengthValueMask) * multiplier;

            if ( (encodedByte & RemainingLengthContinuationBit) == 0 )
            {
                lengthComplete = true;
                break;
            }

            multiplier *= RemainingLengthBase;
        }

        if ( !lengthComplete )
        {
            if ( lengthBytes == MaximumRemainingLengthBytes )
                protocolError(QStringLiteral("Malformed MQTT remaining length"));
            return;
        }

        /* MQTT permits much larger packets, but this subscriber consumes small
         * messages. The local cap prevents an invalid peer exhausting memory. */
        if ( remainingLength > MaximumPacketSize )
        {
            protocolError(QStringLiteral("MQTT packet exceeds the supported size"));
            return;
        }

        const int packetLength = index + remainingLength;
        if ( receiveBuffer.size() < packetLength )
            return;

        const quint8 header = static_cast<quint8>(receiveBuffer.at(0));
        const QByteArray body = receiveBuffer.mid(index, remainingLength);
        receiveBuffer.remove(0, packetLength);
        processPacket(header, body);

        if ( state == State::Disconnected )
            return;
    }
}

void MqttClient::socketError()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "MQTT socket error:" << socket->errorString();

    if ( socket->state() != QAbstractSocket::UnconnectedState )
        socket->abort();

    scheduleReconnect();
}

void MqttClient::socketSslErrors(const QList<QSslError> &errors)
{
    FCT_IDENTIFICATION;

    /* Do not call ignoreSslErrors(): certificate validation is mandatory.
     * QSslSocket will terminate the handshake and reconnect normally. */
    for ( const QSslError &error : errors )
        qCDebug(runtime) << "MQTT TLS error:" << error.errorString();
}

void MqttClient::connectionTimedOut()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "MQTT connection timed out";
    socket->abort();
    scheduleReconnect();
}

void MqttClient::keepAliveTimedOut()
{
    FCT_IDENTIFICATION;

    if ( state != State::Subscribed )
        return;

    if ( pingOutstanding )
    {
        qCDebug(runtime) << "MQTT keepalive response timed out";
        socket->abort();
        scheduleReconnect();
        return;
    }

    qCDebug(runtime) << "Sending MQTT PINGREQ";
    /* PINGREQ is exactly C0 00: header 0xc0 and Remaining Length 0. It has no
     * variable header or payload; writePacket() appends the zero length byte. */
    writePacket(PingReqHeader, QByteArray());
    pingOutstanding = true;
}

void MqttClient::restartConnection()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "Restarting MQTT connection for subscriptions" << subscriptions;

    reconnectTimer->stop();
    connectionTimer->stop();
    keepAliveTimer->stop();
    receiveBuffer.clear();
    pingOutstanding = false;
    state = State::Disconnected;
    socket->abort();
    reconnectDelayMs = InitialReconnectDelayMs;

    if ( !subscriptions.isEmpty() )
        reconnectTimer->start(0);
}

void MqttClient::scheduleReconnect()
{
    FCT_IDENTIFICATION;

    connectionTimer->stop();
    keepAliveTimer->stop();
    receiveBuffer.clear();
    pingOutstanding = false;
    state = State::Disconnected;

    if ( subscriptions.isEmpty() || reconnectTimer->isActive() )
        return;

    /* Exponential backoff avoids a tight reconnect loop while still recovering
     * quickly from a short network interruption. The upper bound prevents a
     * long outage from making later recovery unnecessarily slow. */
    qCDebug(runtime) << "Reconnecting MQTT in" << reconnectDelayMs << "ms";
    reconnectTimer->start(reconnectDelayMs);
    reconnectDelayMs = qMin(reconnectDelayMs * ReconnectBackoffMultiplier,
                            static_cast<int>(MaximumReconnectDelayMs));
}

void MqttClient::sendConnect()
{
    FCT_IDENTIFICATION;

    /* CONNECT packet sent by this client:
     *
     *   fixed header added by writePacket():
     *     0x10 | Remaining Length
     *   variable header built below:
     *     [0x00 0x04 "MQTT"]       protocol name as an MQTT UTF-8 string
     *     [0x04]                    protocol level: MQTT 3.1.1
     *     [0x02]                    Clean Session, no Will/user/password
     *     [keepalive MSB][LSB]      maximum idle interval in seconds
     *   payload:
     *     [client ID length MSB][LSB][client ID UTF-8 bytes]
     *
     * Clean Session lets restartConnection() replace the complete subscription
     * set without retaining stale broker-side topics. */
    QByteArray body;
    body.append(encodeString(QStringLiteral("MQTT")));
    body.append(char(MqttProtocolLevel));
    body.append(char(CleanSessionFlag));
    appendUInt16(body, KeepAliveSeconds);
    body.append(encodeString(clientId));

    qCDebug(runtime) << "Sending MQTT CONNECT as" << clientId;
    writePacket(ConnectHeader, body);
}

void MqttClient::sendSubscribe()
{
    FCT_IDENTIFICATION;

    /* SUBSCRIBE packet sent by this client:
     *
     *   fixed header added by writePacket():
     *     0x82 | Remaining Length   lower bits 0010 are mandatory
     *   variable header:
     *     [packet identifier MSB][LSB]
     *   payload, repeated for every subscription:
     *     [topic length MSB][LSB][topic UTF-8 bytes][requested QoS]
     *
     * MQTT packet identifier zero is reserved and must never be sent. */
    ++packetIdentifier;
    if ( packetIdentifier == 0 )
        ++packetIdentifier;

    pendingSubscribeIdentifier = packetIdentifier;

    QByteArray body;
    appendUInt16(body, packetIdentifier);

    for ( const QString &topic : static_cast<const QStringList&>(subscriptions) )
    {
        const QByteArray encodedTopic = encodeString(topic);
        if ( encodedTopic.isEmpty() )
        {
            protocolError(QStringLiteral("MQTT subscription topic is too long"));
            return;
        }

        body.append(encodedTopic);
        /* Request QoS 0. processPublish() therefore does not need an ACK state
         * machine and can forward the payload immediately. */
        body.append(char(QosAtMostOnce));
    }

    state = State::AwaitingSubAck;
    qCDebug(runtime) << "Subscribing MQTT topics" << subscriptions;
    writePacket(SubscribeHeader, body);
}

void MqttClient::processPacket(quint8 header, const QByteArray &body)
{
    FCT_IDENTIFICATION;

    /* socketReadyRead() has already removed the fixed header and Remaining
     * Length bytes. body contains only the variable header and payload. */
    const PacketType packetType = static_cast<PacketType>(header >> PacketTypeShift);
    qCDebug(function_parameters) << static_cast<quint8>(packetType) << body.size();

    switch ( packetType )
    {
    case PacketType::ConnAck:
        /* CONNACK is 20 02 followed by two body bytes:
         *
         *   [Connect Acknowledge Flags][Connect Return Code]
         *
         * Return Code 0 means that the broker accepted CONNECT. */
        if ( header != ConnAckHeader
             || state != State::AwaitingConnAck
             || body.size() != 2 )
        {
            protocolError(QStringLiteral("Unexpected MQTT CONNACK"));
            return;
        }

        if ( static_cast<quint8>(body.at(1))
             != ConnectionAccepted )
        {
            protocolError(QStringLiteral("MQTT broker rejected CONNECT with code %1")
                          .arg(static_cast<quint8>(body.at(1))));
            return;
        }

        qCDebug(runtime) << "MQTT CONNECT accepted";
        sendSubscribe();
        return;

    case PacketType::Publish:
        processPublish(header, body);
        return;

    case PacketType::SubAck:
    {
        /* SUBACK has this format after its 0x90 fixed-header byte:
         *
         *   Remaining Length
         *   [packet identifier MSB][LSB]
         *   [return code 1] ... [return code N]
         *
         * Return codes correspond to subscribed topic filters in the same
         * order. Values 0, 1 and 2 grant that maximum QoS; 0x80 rejects it. */
        if ( header != SubAckHeader
             || state != State::AwaitingSubAck
             || body.size() != subscriptions.size() + static_cast<int>(sizeof(quint16)) )
        {
            protocolError(QStringLiteral("Unexpected MQTT SUBACK"));
            return;
        }

        const quint16 identifier = readUInt16(body);
        if ( identifier != pendingSubscribeIdentifier )
        {
            protocolError(QStringLiteral("MQTT SUBACK packet identifier does not match"));
            return;
        }

        for ( int i = static_cast<int>(sizeof(quint16)); i < body.size(); ++i )
        {
            if ( static_cast<quint8>(body.at(i)) == SubscriptionRejected )
            {
                protocolError(QStringLiteral("MQTT broker rejected a subscription"));
                return;
            }
        }

        connectionTimer->stop();
        /* Reset backoff only after the broker confirms that the client is fully
         * usable, not merely after the TCP/TLS connection succeeds. */
        reconnectDelayMs = InitialReconnectDelayMs;
        pingOutstanding = false;
        state = State::Subscribed;
        keepAliveTimer->start();
        qCDebug(runtime) << "MQTT subscriptions active" << subscriptions;
        return;
    }

    case PacketType::PingResp:
        /* PINGRESP is exactly D0 00 and therefore has an empty body. */
        if ( header == PingRespHeader && body.isEmpty() )
        {
            pingOutstanding = false;
            qCDebug(runtime) << "Received MQTT PINGRESP";
        }
        return;

    default:
        qCDebug(runtime) << "Ignoring MQTT packet type"
                         << static_cast<quint8>(packetType);
        return;
    }
}

void MqttClient::processPublish(quint8 header, const QByteArray &body)
{
    FCT_IDENTIFICATION;

    /* PUBLISH has a variable first byte:
     *
     *   bits 7..4 : packet type 3
     *   bit 3     : DUP
     *   bits 2..1 : QoS
     *   bit 0     : RETAIN
     *
     * Its body starts with [topic length MSB][LSB][topic UTF-8 bytes]. For QoS
     * 1 or 2 a packet identifier follows the topic. This client subscribes at
     * QoS 0, so no identifier is present and all remaining bytes are the JSON
     * application payload emitted through messageReceived(). */
    const quint8 qos = (header >> PublishQosShift) & PublishQosMask;
    if ( qos != QosAtMostOnce
         || body.size() < static_cast<int>(sizeof(quint16)) )
    {
        protocolError(QStringLiteral("Unsupported or malformed MQTT PUBLISH"));
        return;
    }

    const int topicLength = readUInt16(body);
    if ( topicLength == 0
         || body.size() < topicLength + static_cast<int>(sizeof(quint16)) )
    {
        protocolError(QStringLiteral("Malformed MQTT PUBLISH topic"));
        return;
    }

    const QString topic = QString::fromUtf8(body.constData() + sizeof(quint16),
                                            topicLength);
    const QByteArray payload = body.mid(topicLength + sizeof(quint16));
    qCDebug(runtime) << "Received MQTT PUBLISH" << topic << payload.size();
    emit messageReceived(topic, payload);
}

void MqttClient::protocolError(const QString &message)
{
    FCT_IDENTIFICATION;

    qCDebug(runtime).noquote() << message;
    socket->abort();
    scheduleReconnect();
}

void MqttClient::writePacket(quint8 header, const QByteArray &body)
{
    FCT_IDENTIFICATION;

    /* Assemble the complete wire packet as:
     *
     *   [first fixed-header byte]
     *   [Remaining Length, one to four bytes]
     *   [variable header and payload from body]
     */
    QByteArray packet;
    packet.append(char(header));
    packet.append(encodeRemainingLength(body.size()));
    packet.append(body);

    qCDebug(function_parameters) << header << body.size();
    socket->write(packet);
}

QByteArray MqttClient::encodeRemainingLength(int length)
{
    FCT_IDENTIFICATION;

    /* Encode the number of body bytes in base 128, least-significant group
     * first. The high bit is set on every byte followed by another byte. */
    QByteArray encoded;
    do
    {
        quint8 value = length % RemainingLengthBase;
        length /= RemainingLengthBase;
        if ( length > 0 )
            value |= RemainingLengthContinuationBit;
        encoded.append(char(value));
    }
    while ( length > 0 );

    return encoded;
}

QByteArray MqttClient::encodeString(const QString &value)
{
    FCT_IDENTIFICATION;

    const QByteArray utf8 = value.toUtf8();
    if ( utf8.size() > MaximumMqttStringLength )
        return QByteArray();

    /* An MQTT UTF-8 string is [length MSB][length LSB][UTF-8 bytes]. The two
     * length bytes count only the UTF-8 data; there is no terminating NUL. */
    QByteArray encoded;
    appendUInt16(encoded, static_cast<quint16>(utf8.size()));
    encoded.append(utf8);
    return encoded;
}

void MqttClient::appendUInt16(QByteArray &data, quint16 value)
{
    FCT_IDENTIFICATION;

    /* All MQTT two-byte integer fields use network order: MSB followed by LSB. */
    char encoded[sizeof(value)];
    qToBigEndian(value, reinterpret_cast<uchar *>(encoded));
    data.append(encoded, sizeof(encoded));
}

quint16 MqttClient::readUInt16(const QByteArray &data)
{
    FCT_IDENTIFICATION;

    /* Callers validate that at least two bytes are available before reading. */
    return qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(data.constData()));
}
