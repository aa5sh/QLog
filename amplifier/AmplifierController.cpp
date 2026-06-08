#include "amplifier/AmplifierController.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "core/LogParam.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.amplifier.amplifiercontroller");

namespace
{
    constexpr quint8 PC_SYN = 0x55;
    constexpr quint8 AMP_SYN = 0xaa;
    constexpr quint8 STATUS_COUNT = 0x1e;

    constexpr quint8 CMD_KEY_ON = 0x10;
    constexpr quint8 CMD_RCU_ON = 0x80;
    constexpr quint8 CMD_RCU_OFF = 0x81;

    constexpr quint8 KEY_OFF = 0x18;
    constexpr quint8 KEY_MODE = 0x1a;
    constexpr quint8 KEY_OPERATE = 0x1c;
    constexpr quint8 KEY_INPUT = 0x28;
    constexpr quint8 KEY_BAND_MINUS = 0x29;
    constexpr quint8 KEY_BAND_PLUS = 0x2a;
    constexpr quint8 KEY_ANTENNA = 0x2b;
    constexpr quint8 KEY_LEFT = 0x2d;
    constexpr quint8 KEY_RIGHT = 0x2e;
    constexpr quint8 KEY_TUNE = 0x34;

    constexpr quint8 FLAG_TUNE = 0x01;
    constexpr quint8 FLAG_OPERATE = 0x02;
    constexpr quint8 FLAG_TX = 0x04;
    constexpr quint8 FLAG_ALARM = 0x08;
    constexpr quint8 FLAG_FULL_MODE = 0x10;
    constexpr quint8 FLAG_TEMP_CELSIUS = 0x80;

    QJsonObject toJson(const AmplifierProfile &profile)
    {
        QJsonObject obj;
        obj["profileName"] = profile.profileName;
        obj["model"] = profile.model;
        obj["connectionType"] = profile.connectionType;
        obj["serialPort"] = profile.serialPort;
        obj["baudRate"] = profile.baudRate;
        obj["host"] = profile.host;
        obj["port"] = profile.port;
        return obj;
    }

    AmplifierProfile fromJson(const QJsonObject &obj)
    {
        AmplifierProfile profile;
        profile.profileName = obj["profileName"].toString();
        profile.model = static_cast<AmplifierProfile::AmplifierModel>(obj["model"].toInt(AmplifierProfile::SPE_EXPERT_1K));
        profile.connectionType = static_cast<AmplifierProfile::ConnectionType>(obj["connectionType"].toInt(AmplifierProfile::Serial));
        profile.serialPort = obj["serialPort"].toString();
        profile.baudRate = obj["baudRate"].toInt(9600);
        profile.host = obj["host"].toString();
        profile.port = obj["port"].toInt(5000);
        return profile;
    }

    quint8 keyCodeForCommand(AmplifierController::Command command)
    {
        switch (command)
        {
        case AmplifierController::Operate:
            return KEY_OPERATE;
        case AmplifierController::Standby:
            return KEY_OFF;
        case AmplifierController::Tune:
            return KEY_TUNE;
        case AmplifierController::Mode:
            return KEY_MODE;
        case AmplifierController::Antenna:
            return KEY_ANTENNA;
        case AmplifierController::Input:
            return KEY_INPUT;
        case AmplifierController::Left:
            return KEY_LEFT;
        case AmplifierController::Right:
            return KEY_RIGHT;
        case AmplifierController::BandDown:
            return KEY_BAND_MINUS;
        case AmplifierController::BandUp:
            return KEY_BAND_PLUS;
        }
        return KEY_OPERATE;
    }
}

QList<AmplifierProfile> AmplifierProfiles::profiles()
{
    QList<AmplifierProfile> ret;
    const QJsonDocument doc = QJsonDocument::fromJson(LogParam::getAmplifierProfiles().toUtf8());
    for (const QJsonValue &value : doc.array())
    {
        AmplifierProfile profile = fromJson(value.toObject());
        if (!profile.profileName.isEmpty())
            ret << profile;
    }
    return ret;
}

QStringList AmplifierProfiles::profileNames()
{
    QStringList ret;
    for (const AmplifierProfile &profile : profiles())
        ret << profile.profileName;
    return ret;
}

AmplifierProfile AmplifierProfiles::profile(const QString &profileName)
{
    for (const AmplifierProfile &profile : profiles())
    {
        if (profile.profileName == profileName)
            return profile;
    }
    return AmplifierProfile();
}

void AmplifierProfiles::saveProfiles(const QList<AmplifierProfile> &profiles)
{
    QJsonArray array;
    for (const AmplifierProfile &profile : profiles)
        array.append(toJson(profile));

    LogParam::setAmplifierProfiles(QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)));
}

void AmplifierProfiles::addOrReplace(const AmplifierProfile &profile)
{
    QList<AmplifierProfile> all = profiles();
    bool replaced = false;
    for (AmplifierProfile &existing : all)
    {
        if (existing.profileName == profile.profileName)
        {
            existing = profile;
            replaced = true;
            break;
        }
    }
    if (!replaced)
        all << profile;
    saveProfiles(all);
}

void AmplifierProfiles::remove(const QString &profileName)
{
    QList<AmplifierProfile> remaining;
    for (const AmplifierProfile &profile : profiles())
    {
        if (profile.profileName != profileName)
            remaining << profile;
    }
    saveProfiles(remaining);
}

QString AmplifierProfiles::currentProfileName()
{
    return LogParam::getAmplifierCurrentProfile();
}

void AmplifierProfiles::setCurrentProfileName(const QString &profileName)
{
    LogParam::setAmplifierCurrentProfile(profileName);
}

AmplifierController *AmplifierController::instance()
{
    static AmplifierController controller;
    return &controller;
}

AmplifierController::AmplifierController(QObject *parent) :
    QObject(parent),
    socket(new QTcpSocket(this)),
    serial(new QSerialPort(this)),
    rcuRetryTimer(new QTimer(this))
{
    FCT_IDENTIFICATION;

    qRegisterMetaType<AmplifierStatus>("AmplifierStatus");

    rcuRetryTimer->setInterval(1000);
    connect(rcuRetryTimer, &QTimer::timeout, this, &AmplifierController::rcuRetry);
    connect(socket, &QTcpSocket::readyRead, this, &AmplifierController::readNetworkData);
    connect(socket, &QTcpSocket::connected, this, &AmplifierController::socketConnected);
    connect(socket, &QTcpSocket::disconnected, this, [this]() { setConnected(false); });
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    connect(socket, &QTcpSocket::errorOccurred, this, &AmplifierController::socketError);
#else
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error), this, &AmplifierController::socketError);
#endif
    connect(serial, &QSerialPort::readyRead, this, &AmplifierController::readSerialData);
    connect(serial, &QSerialPort::errorOccurred, this, &AmplifierController::serialError);
}

AmplifierController::~AmplifierController()
{
    close();
}

bool AmplifierController::isConnected() const
{
    return connectedState;
}

bool AmplifierController::isEnabled() const
{
    return enabledState;
}

QString AmplifierController::currentProfile() const
{
    return activeProfile.profileName;
}

AmplifierStatus AmplifierController::status() const
{
    return currentStatus;
}

void AmplifierController::open()
{
    openProfile(AmplifierProfiles::currentProfileName());
}

void AmplifierController::openProfile(const QString &profileName)
{
    FCT_IDENTIFICATION;

    close();

    activeProfile = AmplifierProfiles::profile(profileName);
    if (activeProfile.profileName.isEmpty())
    {
        emit errorPresent(tr("Amplifier profile is not configured"),
                          tr("Create or select an amplifier profile in Settings."));
        return;
    }

    AmplifierProfiles::setCurrentProfileName(activeProfile.profileName);
    enabledState = true;
    parseState = ParseState::WaitSyn1;
    packetBuffer.clear();

    if (activeProfile.connectionType == AmplifierProfile::Network)
    {
        if (activeProfile.host.isEmpty() || activeProfile.port <= 0)
        {
            emit errorPresent(tr("Amplifier network settings are incomplete"),
                              tr("Set host and port for the amplifier profile."));
            enabledState = false;
            return;
        }
        socket->connectToHost(activeProfile.host, activeProfile.port);
        return;
    }

    if (activeProfile.serialPort.isEmpty())
    {
        emit errorPresent(tr("Amplifier serial port is not configured"),
                          tr("Select a serial port for the amplifier profile."));
        enabledState = false;
        return;
    }

    serial->setPortName(activeProfile.serialPort);
    serial->setBaudRate(activeProfile.baudRate);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);
    if (!serial->open(QIODevice::ReadWrite))
    {
        emit errorPresent(tr("Cannot open amplifier serial port"), serial->errorString());
        enabledState = false;
        return;
    }

    serial->setDataTerminalReady(true);
    setConnected(true);
}

void AmplifierController::close()
{
    FCT_IDENTIFICATION;

    enabledState = false;
    rcuRetryTimer->stop();

    if (connectedState)
        writeCommand(buildRcuCommand(false));

    if (serial->isOpen())
    {
        serial->setDataTerminalReady(false);
        serial->close();
    }
    if (socket->state() != QAbstractSocket::UnconnectedState)
        socket->disconnectFromHost();

    setConnected(false);
}

void AmplifierController::reloadSettings()
{
    if (connectedState && currentProfile() != AmplifierProfiles::currentProfileName())
        open();
}

void AmplifierController::sendCommand(AmplifierController::Command command)
{
    writeCommand(buildKeyCommand(keyCodeForCommand(command)));
}

void AmplifierController::readNetworkData()
{
    processBytes(socket->readAll());
}

void AmplifierController::readSerialData()
{
    processBytes(serial->readAll());
}

void AmplifierController::socketConnected()
{
    setConnected(true);
}

void AmplifierController::socketError()
{
    if (socket->error() != QAbstractSocket::UnknownSocketError)
        emit errorPresent(tr("Amplifier network error"), socket->errorString());
}

void AmplifierController::serialError(QSerialPort::SerialPortError error)
{
    if (error != QSerialPort::NoError)
        emit errorPresent(tr("Amplifier serial error"), serial->errorString());
}

void AmplifierController::rcuRetry()
{
    if (connectedState)
        writeCommand(buildRcuCommand(true));
}

QByteArray AmplifierController::buildKeyCommand(quint8 keyCode) const
{
    QByteArray command;
    command.reserve(7);
    command.append(char(PC_SYN));
    command.append(char(PC_SYN));
    command.append(char(PC_SYN));
    command.append('\x02');
    command.append(char(CMD_KEY_ON));
    command.append(char(keyCode));
    command.append(char((CMD_KEY_ON + keyCode) & 0xff));
    return command;
}

QByteArray AmplifierController::buildRcuCommand(bool enabled) const
{
    const quint8 cmd = enabled ? CMD_RCU_ON : CMD_RCU_OFF;
    QByteArray command;
    command.reserve(6);
    command.append(char(PC_SYN));
    command.append(char(PC_SYN));
    command.append(char(PC_SYN));
    command.append('\x01');
    command.append(char(cmd));
    command.append(char(cmd));
    return command;
}

void AmplifierController::writeCommand(const QByteArray &command)
{
    if (activeProfile.connectionType == AmplifierProfile::Network && socket->state() == QAbstractSocket::ConnectedState)
        socket->write(command);
    else if (activeProfile.connectionType == AmplifierProfile::Serial && serial->isOpen())
        serial->write(command);
}

void AmplifierController::processBytes(const QByteArray &data)
{
    for (const char ch : data)
    {
        const quint8 byte = static_cast<quint8>(ch);

        switch (parseState)
        {
        case ParseState::WaitSyn1:
            if (byte == AMP_SYN)
            {
                packetBuffer.clear();
                packetBuffer.append(ch);
                parseState = ParseState::WaitSyn2;
            }
            break;
        case ParseState::WaitSyn2:
            if (byte == AMP_SYN)
            {
                packetBuffer.append(ch);
                parseState = ParseState::WaitSyn3;
            }
            else
                parseState = ParseState::WaitSyn1;
            break;
        case ParseState::WaitSyn3:
            if (byte == AMP_SYN)
            {
                packetBuffer.append(ch);
                parseState = ParseState::ReadCount;
            }
            else
                parseState = ParseState::WaitSyn1;
            break;
        case ParseState::ReadCount:
            expectedCount = byte;
            packetBuffer.append(ch);
            parseState = ParseState::ReadData;
            break;
        case ParseState::ReadData:
            packetBuffer.append(ch);
            if (packetBuffer.size() == static_cast<int>(4 + expectedCount))
                parseState = ParseState::ReadChecksum;
            break;
        case ParseState::ReadChecksum:
            packetBuffer.append(ch);
            dispatchPacket(packetBuffer);
            parseState = ParseState::WaitSyn1;
            break;
        }
    }
}

void AmplifierController::dispatchPacket(const QByteArray &packet)
{
    if (expectedCount == STATUS_COUNT && packet.size() == 35 && parseSpeStatus(packet))
    {
        rcuRetryTimer->stop();
        emit statusChanged(currentStatus);
    }
}

bool AmplifierController::parseSpeStatus(const QByteArray &packet)
{
    if (packet.size() < 35)
        return false;

    quint8 checksum = 0;
    for (int i = 4; i <= 33; ++i)
        checksum = (checksum + static_cast<quint8>(packet[i])) & 0xff;

    if (checksum != static_cast<quint8>(packet[34]))
        return false;

    const quint8 flags = static_cast<quint8>(packet[5]);
    currentStatus.tuning = (flags & FLAG_TUNE) != 0;
    currentStatus.operate = (flags & FLAG_OPERATE) != 0;
    currentStatus.tx = (flags & FLAG_TX) != 0;
    currentStatus.alarm = (flags & FLAG_ALARM) != 0;
    currentStatus.fullMode = (flags & FLAG_FULL_MODE) != 0;
    currentStatus.tempCelsius = (flags & FLAG_TEMP_CELSIUS) != 0;

    const quint8 bandInput = static_cast<quint8>(packet[18]);
    currentStatus.band = (bandInput >> 4) & 0x0f;
    currentStatus.input = bandInput & 0x0f;
    currentStatus.freqKHz = quint16(static_cast<quint8>(packet[20])) | (quint16(static_cast<quint8>(packet[21])) << 8);
    currentStatus.antenna = static_cast<quint8>(packet[22]) & 0x0f;

    const quint16 swrGain = quint16(static_cast<quint8>(packet[23])) | (quint16(static_cast<quint8>(packet[24])) << 8);
    currentStatus.swr = swrGain / 100.0;
    currentStatus.gainDb = swrGain / 10.0;
    currentStatus.temperature = static_cast<quint8>(packet[25]);

    const quint16 power = quint16(static_cast<quint8>(packet[26])) | (quint16(static_cast<quint8>(packet[27])) << 8);
    currentStatus.paOutW = power / 10.0;

    const quint16 reverse = quint16(static_cast<quint8>(packet[28])) | (quint16(static_cast<quint8>(packet[29])) << 8);
    currentStatus.reverseW = reverse / 10.0;

    const quint16 volts = quint16(static_cast<quint8>(packet[30])) | (quint16(static_cast<quint8>(packet[31])) << 8);
    currentStatus.voltageV = volts / 10.0;

    const quint16 amps = quint16(static_cast<quint8>(packet[32])) | (quint16(static_cast<quint8>(packet[33])) << 8);
    currentStatus.currentA = amps / 10.0;

    return true;
}

void AmplifierController::setConnected(bool state)
{
    if (connectedState == state)
        return;

    connectedState = state;
    if (connectedState)
    {
        writeCommand(buildRcuCommand(true));
        rcuRetryTimer->start();
        emit connected();
    }
    else
    {
        rcuRetryTimer->stop();
        resetStatus();
        emit disconnected();
    }
}

void AmplifierController::resetStatus()
{
    currentStatus = AmplifierStatus();
    emit statusChanged(currentStatus);
}
