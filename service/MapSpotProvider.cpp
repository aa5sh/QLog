#include "MapSpotProvider.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

#include "data/BandPlan.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.service.mapspotprovider");

namespace
{
const char pskHost[] = "mqtt.pskreporter.info";
const quint16 pskPort = 1883;
const qint64 maxSpotAgeMs = 30 * 60 * 1000;
}

MapSpotProvider::MapSpotProvider(QObject *parent)
    : QObject(parent),
      pskSocket(new QTcpSocket(this)),
      pskReconnectTimer(new QTimer(this)),
      pskPingTimer(new QTimer(this)),
      mqttPacketId(0),
      networkManager(new QNetworkAccessManager(this)),
      filterByBand(false),
      pskEnabled(false),
      rbnEnabled(false)
{
    FCT_IDENTIFICATION;

    pskReconnectTimer->setSingleShot(true);
    pskReconnectTimer->setInterval(10000);
    pskPingTimer->setInterval(30000);

    connect(pskSocket, &QTcpSocket::connected, this, &MapSpotProvider::sendPskConnect);
    connect(pskSocket, &QTcpSocket::readyRead, this, &MapSpotProvider::readPsk);
    connect(pskSocket, &QTcpSocket::disconnected, this, &MapSpotProvider::pskDisconnected);
#if (QT_VERSION < QT_VERSION_CHECK(5, 15, 0))
    connect(pskSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, [this](QAbstractSocket::SocketError error) {
#else
    connect(pskSocket, &QTcpSocket::errorOccurred,
            this, [this](QAbstractSocket::SocketError error) {
#endif
        qCWarning(runtime) << "PSK Reporter MQTT socket error" << error
                           << pskSocket->errorString();
    });
    connect(pskReconnectTimer, &QTimer::timeout, this, &MapSpotProvider::connectPsk);
    connect(pskPingTimer, &QTimer::timeout, this, [this]() {
        if (pskSocket->state() == QAbstractSocket::ConnectedState)
            pskSocket->write(QByteArray::fromHex("c000"));
    });
    connect(networkManager, &QNetworkAccessManager::finished,
            this, &MapSpotProvider::rbnNodesReceived);
}

MapSpotProvider::~MapSpotProvider()
{
    FCT_IDENTIFICATION;

    pskReconnectTimer->stop();
    pskPingTimer->stop();
    pskSocket->abort();
    stopRbnSockets();
}

void MapSpotProvider::setPskEnabled(bool enabled)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << enabled;

    if (pskEnabled == enabled)
        return;
    pskEnabled = enabled;
    qCDebug(runtime) << "PSK Reporter map spots" << (enabled ? "enabled" : "disabled");
    restartPsk();
    if (!enabled)
        emit sourceCleared(QStringLiteral("psk"));
}

void MapSpotProvider::setRbnEnabled(bool enabled)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << enabled;

    if (rbnEnabled == enabled)
        return;
    rbnEnabled = enabled;
    qCDebug(runtime) << "Reverse Beacon map spots" << (enabled ? "enabled" : "disabled");
    if (enabled && !stationCallsign.isEmpty()) {
        refreshRbnNodes();
        startRbnSocket(7000);
        startRbnSocket(7001);
    } else {
        stopRbnSockets();
        emit sourceCleared(QStringLiteral("rbn"));
    }
}

void MapSpotProvider::setStationCallsign(const QString &callsign)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << callsign;

    const QString value = normalizedCallsign(callsign);
    if (stationCallsign == value)
        return;
    stationCallsign = value;
    qCDebug(runtime) << "Map spot station callsign changed to" << stationCallsign;
    restartPsk();
    if (rbnEnabled) {
        stopRbnSockets();
        emit sourceCleared(QStringLiteral("rbn"));
        if (!stationCallsign.isEmpty()) {
            refreshRbnNodes();
            startRbnSocket(7000);
            startRbnSocket(7001);
        }
    }
}

void MapSpotProvider::setBandFilter(const QString &band, bool enabled)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << band << enabled;

    const bool changed = currentBand != band || filterByBand != enabled;
    currentBand = band;
    filterByBand = enabled && !band.isEmpty();
    if (!changed)
        return;
    qCDebug(runtime) << "Map spot band filter"
                     << (filterByBand ? currentBand : QStringLiteral("all bands"));
    restartPsk();
    emit sourceCleared(QStringLiteral("psk"));
    emit sourceCleared(QStringLiteral("rbn"));
}

QByteArray MapSpotProvider::mqttString(const QByteArray &value)
{
    QByteArray result;
    result.append(char((value.size() >> 8) & 0xff));
    result.append(char(value.size() & 0xff));
    result.append(value);
    return result;
}

QByteArray MapSpotProvider::mqttPacket(quint8 header, const QByteArray &payload)
{
    QByteArray result(1, char(header));
    int remaining = payload.size();
    do {
        int encoded = remaining % 128;
        remaining /= 128;
        if (remaining)
            encoded |= 128;
        result.append(char(encoded));
    } while (remaining);
    result.append(payload);
    return result;
}

QString MapSpotProvider::normalizedCallsign(const QString &callsign)
{
    return callsign.trimmed().toUpper();
}

void MapSpotProvider::restartPsk()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "Restarting PSK Reporter MQTT connection"
                     << "enabled" << pskEnabled
                     << "callsign" << stationCallsign
                     << "band" << (filterByBand ? currentBand : QStringLiteral("all"));
    pskReconnectTimer->stop();
    pskPingTimer->stop();
    pskBuffer.clear();
    pskSocket->abort();
    if (pskEnabled && !stationCallsign.isEmpty())
        QTimer::singleShot(0, this, &MapSpotProvider::connectPsk);
}

void MapSpotProvider::connectPsk()
{
    FCT_IDENTIFICATION;

    if (pskEnabled && !stationCallsign.isEmpty()
        && pskSocket->state() == QAbstractSocket::UnconnectedState) {
        qCDebug(runtime) << "Connecting to PSK Reporter MQTT" << pskHost << pskPort;
        pskSocket->connectToHost(QString::fromLatin1(pskHost), pskPort);
    }
}

void MapSpotProvider::sendPskConnect()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "PSK Reporter TCP connection established; sending MQTT CONNECT";
    const QByteArray clientId = QByteArrayLiteral("qlog-")
                                + QByteArray::number(QRandomGenerator::global()->generate(), 16);
    QByteArray payload = mqttString(QByteArrayLiteral("MQTT"));
    payload.append(char(4)); // MQTT 3.1.1
    payload.append(char(2)); // clean session
    payload.append(char(0));
    payload.append(char(60));
    payload.append(mqttString(clientId));
    pskSocket->write(mqttPacket(0x10, payload));
    pskPingTimer->start();
}

void MapSpotProvider::subscribePsk()
{
    FCT_IDENTIFICATION;

    const QByteArray band = filterByBand ? currentBand.toUtf8() : QByteArrayLiteral("+");
    const QByteArray call = stationCallsign.toUtf8();
    const QList<QByteArray> topics = {
        QByteArrayLiteral("pskr/filter/v2/") + band + QByteArrayLiteral("/+/") + call + QByteArrayLiteral("/#"),
        QByteArrayLiteral("pskr/filter/v2/") + band + QByteArrayLiteral("/+/+/") + call + QByteArrayLiteral("/#")
    };

    QByteArray payload;
    const quint16 id = ++mqttPacketId;
    payload.append(char(id >> 8));
    payload.append(char(id & 0xff));
    for (const QByteArray &topic : topics) {
        payload.append(mqttString(topic));
        payload.append(char(0));
    }
    qCDebug(runtime) << "Subscribing to PSK Reporter MQTT topics" << topics;
    pskSocket->write(mqttPacket(0x82, payload));
}

void MapSpotProvider::readPsk()
{
    FCT_IDENTIFICATION;

    pskBuffer.append(pskSocket->readAll());
    while (pskBuffer.size() >= 2) {
        int multiplier = 1;
        int remaining = 0;
        int index = 1;
        quint8 encoded = 0;
        do {
            if (index >= pskBuffer.size())
                return;
            if (index > 4) {
                qCWarning(runtime) << "Malformed MQTT remaining-length field";
                pskBuffer.clear();
                return;
            }
            encoded = quint8(pskBuffer.at(index++));
            remaining += (encoded & 127) * multiplier;
            multiplier *= 128;
        } while (encoded & 128);
        if (pskBuffer.size() < index + remaining)
            return;
        const quint8 header = quint8(pskBuffer.at(0));
        const QByteArray payload = pskBuffer.mid(index, remaining);
        pskBuffer.remove(0, index + remaining);
        processPskPacket(header, payload);
    }
}

void MapSpotProvider::processPskPacket(quint8 header, const QByteArray &payload)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << "packet type" << (header >> 4)
                                 << "payload bytes" << payload.size();

    switch (header >> 4) {
    case 2: // CONNACK
        if (payload.size() >= 2 && payload.at(1) == 0) {
            qCDebug(runtime) << "PSK Reporter MQTT session established";
            subscribePsk();
        } else {
            qCWarning(runtime) << "PSK Reporter MQTT connection refused; CONNACK payload"
                               << payload.toHex();
        }
        break;
    case 3: { // PUBLISH
        if (payload.size() < 2) {
            qCWarning(runtime) << "Ignoring malformed MQTT PUBLISH packet";
            return;
        }
        const int topicLength = (quint8(payload.at(0)) << 8) | quint8(payload.at(1));
        int offset = 2 + topicLength;
        if (offset > payload.size()) {
            qCWarning(runtime) << "Ignoring MQTT PUBLISH packet with invalid topic length"
                               << topicLength << payload.size();
            return;
        }
        if (((header >> 1) & 3) && payload.size() >= offset + 2)
            offset += 2;
        if (offset <= payload.size())
            processPskMessage(payload.mid(offset));
        break;
    }
    default:
        break;
    }
}

void MapSpotProvider::processPskMessage(const QByteArray &payload)
{
    FCT_IDENTIFICATION;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        qCWarning(runtime) << "Invalid PSK Reporter JSON payload" << parseError.errorString();
        return;
    }

    const QJsonObject object = document.object();
    const QString sender = normalizedCallsign(object.value(QStringLiteral("sc")).toString());
    const QString receiver = normalizedCallsign(object.value(QStringLiteral("rc")).toString());
    MapReceptionSpot spot;
    spot.source = QStringLiteral("psk");
    spot.id = QString::number(object.value(QStringLiteral("sq")).toVariant().toLongLong());
    spot.frequencyMHz = object.value(QStringLiteral("f")).toDouble() / 1000000.0;
    spot.mode = object.value(QStringLiteral("md")).toString();
    spot.band = object.value(QStringLiteral("b")).toString();
    spot.signal = object.value(QStringLiteral("rp")).toInt();
    spot.timestamp = QDateTime::fromSecsSinceEpoch(
                         object.value(QStringLiteral("t")).toVariant().toLongLong()).toUTC();

    if (sender == stationCallsign) {
        spot.remoteCallsign = receiver;
        spot.remoteLocator = object.value(QStringLiteral("rl")).toString();
        spot.direction = QStringLiteral("heard you");
    } else if (receiver == stationCallsign) {
        spot.remoteCallsign = sender;
        spot.remoteLocator = object.value(QStringLiteral("sl")).toString();
        spot.direction = QStringLiteral("you heard");
    } else {
        return;
    }
    if (!spot.timestamp.isValid())
        spot.timestamp = QDateTime::currentDateTimeUtc();
    if (spot.timestamp.msecsTo(QDateTime::currentDateTimeUtc()) <= maxSpotAgeMs) {
        qCDebug(runtime) << "PSK Reporter spot accepted"
                         << spot.direction << spot.remoteCallsign
                         << spot.remoteLocator << spot.band << spot.mode
                         << spot.frequencyMHz << spot.signal;
        emit spotReceived(spot);
    } else {
        qCDebug(runtime) << "Ignoring expired PSK Reporter spot" << spot.id << spot.timestamp;
    }
}

void MapSpotProvider::pskDisconnected()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "PSK Reporter MQTT disconnected";
    pskPingTimer->stop();
    if (pskEnabled && !stationCallsign.isEmpty()) {
        qCDebug(runtime) << "Scheduling PSK Reporter reconnect in"
                         << pskReconnectTimer->interval() << "ms";
        pskReconnectTimer->start();
    }
}

void MapSpotProvider::refreshRbnNodes()
{
    FCT_IDENTIFICATION;

    if (!rbnNodeLocators.isEmpty()) {
        qCDebug(runtime) << "Using cached Reverse Beacon node locators"
                         << rbnNodeLocators.size();
        return;
    }

    const QUrl url(QStringLiteral("https://reversebeacon.net/cont_includes/status.php?t=skt"));
    qCDebug(runtime) << "Downloading Reverse Beacon node locators" << url;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("QLog OnlineMap"));
    networkManager->get(request);
}

QString MapSpotProvider::htmlText(QString value)
{
    value.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
    return value.replace(QStringLiteral("&nbsp;"), QStringLiteral(" ")).trimmed();
}

void MapSpotProvider::rbnNodesReceived(QNetworkReply *reply)
{
    FCT_IDENTIFICATION;

    if (!reply) {
        qCWarning(runtime) << "Reverse Beacon node request returned no reply";
        return;
    }

    if (reply->url().host() != QStringLiteral("reversebeacon.net")) {
        qCDebug(runtime) << "Ignoring unrelated network reply" << reply->url();
        reply->deleteLater();
        return;
    }

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300) {
        const QString html = QString::fromUtf8(reply->readAll());
        const QRegularExpression rowExpression(QStringLiteral("<tr[^>]*>(.*?)</tr>"),
                                               QRegularExpression::DotMatchesEverythingOption);
        const QRegularExpression cellExpression(QStringLiteral("<td[^>]*>(.*?)</td>"),
                                                QRegularExpression::DotMatchesEverythingOption);
        auto row = rowExpression.globalMatch(html);
        while (row.hasNext()) {
            auto cell = cellExpression.globalMatch(row.next().captured(1));
            QStringList cells;
            while (cell.hasNext())
                cells.append(htmlText(cell.next().captured(1)));
            if (cells.size() >= 3 && !cells.at(0).isEmpty() && !cells.at(2).isEmpty())
                rbnNodeLocators.insert(normalizedCallsign(cells.at(0)), cells.at(2));
        }
        qCDebug(runtime) << "Loaded Reverse Beacon node locators"
                         << rbnNodeLocators.size();
        if (rbnNodeLocators.isEmpty())
            qCWarning(runtime) << "Reverse Beacon node response contained no usable locators";
    } else {
        qCWarning(runtime) << "Reverse Beacon node download failed"
                           << reply->errorString() << "HTTP status" << status;
    }
    reply->deleteLater();
}

void MapSpotProvider::startRbnSocket(quint16 port)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << port;

    if (!rbnEnabled || stationCallsign.isEmpty())
        return;
    QTcpSocket *socket = new QTcpSocket(this);
    socket->setProperty("rbnPort", port);
    rbnSockets.append(socket);
    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { readRbn(socket); });
    connect(socket, &QTcpSocket::connected, this, [this, socket]() {
        qCDebug(runtime) << "Reverse Beacon Telnet connection established on port"
                         << socket->property("rbnPort").toUInt();
        socket->write(stationCallsign.toUtf8() + QByteArrayLiteral("\r\n"));
    });
#if (QT_VERSION < QT_VERSION_CHECK(5, 15, 0))
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, [socket](QAbstractSocket::SocketError error) {
#else
    connect(socket, &QTcpSocket::errorOccurred,
            this, [socket](QAbstractSocket::SocketError error) {
#endif
        qCWarning(runtime) << "Reverse Beacon Telnet socket error on port"
                           << socket->property("rbnPort").toUInt()
                           << error << socket->errorString();
    });
    connect(socket, &QTcpSocket::disconnected, this, [this, socket]() { scheduleRbnReconnect(socket); });
    qCDebug(runtime) << "Connecting to Reverse Beacon Telnet port" << port;
    socket->connectToHost(QStringLiteral("telnet.reversebeacon.net"), port);
}

void MapSpotProvider::stopRbnSockets()
{
    FCT_IDENTIFICATION;

    if (!rbnSockets.isEmpty())
        qCDebug(runtime) << "Stopping Reverse Beacon Telnet connections"
                         << rbnSockets.size();
    const QList<QTcpSocket *> sockets = rbnSockets;
    rbnSockets.clear();
    for (QTcpSocket *socket : sockets) {
        rbnBuffers.remove(socket);
        socket->disconnect(this);
        socket->abort();
        socket->deleteLater();
    }
}

void MapSpotProvider::readRbn(QTcpSocket *socket)
{
    FCT_IDENTIFICATION;

    if (!socket) {
        qCWarning(runtime) << "Cannot read Reverse Beacon data from a null socket";
        return;
    }

    QByteArray &buffer = rbnBuffers[socket];
    buffer.append(socket->readAll());
    int newline = -1;
    while ((newline = buffer.indexOf('\n')) >= 0) {
        const QByteArray line = buffer.left(newline).trimmed();
        buffer.remove(0, newline + 1);
        processRbnLine(line);
    }
}

void MapSpotProvider::processRbnLine(const QByteArray &line)
{
    FCT_IDENTIFICATION;

    static const QRegularExpression expression(
        QStringLiteral("^DX de\\s+([^:]+):\\s+([0-9.]+)\\s+(\\S+)\\s+(.*?)\\s+(\\d{4})Z$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = expression.match(QString::fromLatin1(line));
    if (!match.hasMatch() || normalizedCallsign(match.captured(3)) != stationCallsign)
        return;

    const double frequencyMHz = match.captured(2).toDouble() / 1000.0;
    const QString band = BandPlan::freq2Band(frequencyMHz).name;
    if (filterByBand && band != currentBand)
        return;

    QString spotter = normalizedCallsign(match.captured(1));
    spotter.remove(QRegularExpression(QStringLiteral("-#$")));
    QString locator = rbnNodeLocators.value(spotter);
    if (locator.isEmpty()) {
        QString base = spotter;
        base.remove(QRegularExpression(QStringLiteral("-\\d+$")));
        locator = rbnNodeLocators.value(base);
    }
    if (locator.isEmpty()) {
        qCDebug(runtime) << "No locator available for Reverse Beacon node" << spotter;
        return;
    }

    const QString comment = match.captured(4).trimmed();
    const QRegularExpression signalExpression(QStringLiteral("(-?\\d+)\\s*dB"),
                                              QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch signalMatch = signalExpression.match(comment);
    const QString hhmm = match.captured(5);
    QDateTime timestamp = QDateTime::currentDateTimeUtc();
    timestamp.setTime(QTime(hhmm.left(2).toInt(), hhmm.mid(2, 2).toInt()));
    if (timestamp > QDateTime::currentDateTimeUtc().addSecs(60))
        timestamp = timestamp.addDays(-1);

    MapReceptionSpot spot;
    spot.source = QStringLiteral("rbn");
    spot.id = spotter + QLatin1Char('-') + QString::number(timestamp.toMSecsSinceEpoch())
              + QLatin1Char('-') + QString::number(qRound64(frequencyMHz * 1000000.0));
    spot.remoteCallsign = spotter;
    spot.remoteLocator = locator;
    spot.direction = QStringLiteral("heard you");
    spot.frequencyMHz = frequencyMHz;
    spot.band = band;
    spot.mode = comment.section(QLatin1Char(' '), 0, 0);
    spot.signal = signalMatch.hasMatch() ? signalMatch.captured(1).toInt() : 0;
    spot.timestamp = timestamp;
    qCDebug(runtime) << "Reverse Beacon spot accepted"
                     << spot.remoteCallsign << spot.remoteLocator
                     << spot.band << spot.mode << spot.frequencyMHz << spot.signal;
    emit spotReceived(spot);
}

void MapSpotProvider::scheduleRbnReconnect(QTcpSocket *socket)
{
    FCT_IDENTIFICATION;

    if (!socket) {
        qCWarning(runtime) << "Cannot schedule Reverse Beacon reconnect for a null socket";
        return;
    }

    if (!rbnSockets.removeOne(socket))
        return;
    const quint16 port = socket->property("rbnPort").toUInt();
    rbnBuffers.remove(socket);
    socket->deleteLater();
    qCDebug(runtime) << "Reverse Beacon Telnet disconnected from port" << port;
    if (rbnEnabled && !stationCallsign.isEmpty()) {
        qCDebug(runtime) << "Scheduling Reverse Beacon reconnect on port" << port
                         << "in 10000 ms";
        QTimer::singleShot(10000, this, [this, port]() { startRbnSocket(port); });
    }
}
