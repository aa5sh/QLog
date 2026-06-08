#include "antenna/SteppirController.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSerialPort>
#include <QTcpSocket>
#include <QTimer>

#include "core/LogParam.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.antenna.steppircontroller");

namespace
{
QJsonObject toJson(const SteppirProfile &profile)
{
    QJsonObject obj;
    obj["name"] = profile.name;
    obj["type"] = profile.type;
    obj["host"] = profile.host;
    obj["port"] = static_cast<int>(profile.port);
    obj["serialPort"] = profile.serialPort;
    obj["baudRate"] = profile.baudRate;
    return obj;
}

SteppirProfile fromJson(const QJsonObject &obj)
{
    SteppirProfile profile;
    profile.name = obj["name"].toString();
    profile.type = static_cast<SteppirProfile::ConnectionType>(obj["type"].toInt(SteppirProfile::Network));
    profile.host = obj["host"].toString();
    profile.port = static_cast<quint16>(obj["port"].toInt());
    profile.serialPort = obj["serialPort"].toString();
    profile.baudRate = obj["baudRate"].toInt(9600);
    return profile;
}
}

QList<SteppirProfile> SteppirProfiles::profiles()
{
    QList<SteppirProfile> ret;
    const QJsonDocument doc = QJsonDocument::fromJson(LogParam::getSteppirProfiles().toUtf8());
    for (const QJsonValue &value : doc.array())
    {
        SteppirProfile profile = fromJson(value.toObject());
        if (!profile.name.isEmpty())
            ret << profile;
    }
    return ret;
}

QStringList SteppirProfiles::profileNames()
{
    QStringList ret;
    for (const SteppirProfile &profile : profiles())
        ret << profile.name;
    return ret;
}

SteppirProfile SteppirProfiles::profile(const QString &name)
{
    for (const SteppirProfile &profile : profiles())
        if (profile.name == name)
            return profile;
    return SteppirProfile();
}

void SteppirProfiles::saveProfiles(const QList<SteppirProfile> &profiles)
{
    QJsonArray array;
    for (const SteppirProfile &profile : profiles)
        if (!profile.name.isEmpty())
            array << toJson(profile);
    LogParam::setSteppirProfiles(QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)));
}

void SteppirProfiles::addOrReplace(const SteppirProfile &profile)
{
    QList<SteppirProfile> all = profiles();
    bool replaced = false;
    for (SteppirProfile &existing : all)
    {
        if (existing.name == profile.name)
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

void SteppirProfiles::remove(const QString &name)
{
    QList<SteppirProfile> all;
    for (const SteppirProfile &profile : profiles())
        if (profile.name != name)
            all << profile;
    saveProfiles(all);
    if (currentProfileName() == name)
        setCurrentProfileName(QString());
}

QString SteppirProfiles::currentProfileName()
{
    return LogParam::getSteppirCurrentProfile();
}

void SteppirProfiles::setCurrentProfileName(const QString &name)
{
    LogParam::setSteppirCurrentProfile(name);
}

SteppirController *SteppirController::instance()
{
    static SteppirController controller;
    return &controller;
}

SteppirController::SteppirController(QObject *parent) :
    QObject(parent),
    socket(new QTcpSocket(this)),
    serial(new QSerialPort(this)),
    pollTimer(new QTimer(this)),
    hasActiveProfile(false),
    connectedState(false),
    frequencyKHz(0),
    currentDirection(Normal),
    autotrackState(false),
    tuningState(false)
{
    pollTimer->setInterval(1000);
    connect(pollTimer, &QTimer::timeout, this, &SteppirController::poll);
    connect(socket, &QTcpSocket::readyRead, this, &SteppirController::readNetworkData);
    connect(socket, &QTcpSocket::connected, this, &SteppirController::socketConnected);
    connect(socket, &QTcpSocket::disconnected, this, &SteppirController::disconnected);
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &SteppirController::socketError);
    connect(serial, &QSerialPort::readyRead, this, &SteppirController::readSerialData);
    connect(serial, QOverload<QSerialPort::SerialPortError>::of(&QSerialPort::errorOccurred),
            this, &SteppirController::serialError);
}

SteppirController::~SteppirController()
{
    close();
}

bool SteppirController::isConnected() const
{
    return connectedState;
}

QString SteppirController::currentProfile() const
{
    return hasActiveProfile ? activeProfile.name : QString();
}

int SteppirController::tunedFrequencyKHz() const
{
    return frequencyKHz;
}

SteppirController::Direction SteppirController::direction() const
{
    return currentDirection;
}

bool SteppirController::autotrack() const
{
    return autotrackState;
}

bool SteppirController::tuning() const
{
    return tuningState;
}

void SteppirController::open()
{
    openProfile(SteppirProfiles::currentProfileName());
}

void SteppirController::openProfile(const QString &profileName)
{
    close();
    activeProfile = SteppirProfiles::profile(profileName);
    hasActiveProfile = !activeProfile.name.isEmpty();

    if (!hasActiveProfile)
    {
        emit errorPresent(tr("Cannot open SteppIR Controller"), tr("No SteppIR profile selected"));
        return;
    }

    SteppirProfiles::setCurrentProfileName(activeProfile.name);

    if (activeProfile.type == SteppirProfile::Network)
    {
        if (activeProfile.host.isEmpty() || activeProfile.port == 0)
        {
            emit errorPresent(tr("Cannot open SteppIR Controller"), tr("Network host and port must be configured"));
            return;
        }
        socket->connectToHost(activeProfile.host, activeProfile.port);
    }
    else
    {
        if (activeProfile.serialPort.isEmpty())
        {
            emit errorPresent(tr("Cannot open SteppIR Controller"), tr("Serial port must be configured"));
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
            emit errorPresent(tr("Cannot open SteppIR Controller"), serial->errorString());
            return;
        }
        socketConnected();
    }
}

void SteppirController::close()
{
    pollTimer->stop();
    if (socket->state() != QAbstractSocket::UnconnectedState)
        socket->disconnectFromHost();
    if (serial->isOpen())
        serial->close();
    if (connectedState)
    {
        connectedState = false;
        emit disconnected();
        emit stateChanged();
    }
}

void SteppirController::setFrequencyHz(double frequencyHz)
{
    setFrequencyKHz(static_cast<int>(frequencyHz / 1000.0));
}

void SteppirController::setFrequencyKHz(int frequencyKHz)
{
    if (frequencyKHz <= 0)
        return;

    this->frequencyKHz = frequencyKHz;
    writeCommand(commandFrame(0x31));
    emit tunedFrequencyChanged(this->frequencyKHz);
    emit stateChanged();
}

void SteppirController::setDirection(SteppirController::Direction direction)
{
    currentDirection = direction;
    writeCommand(commandFrame(0x00));
    emit directionChanged(currentDirection);
    emit stateChanged();
}

void SteppirController::setAutotrack(bool enabled)
{
    autotrackState = enabled;
    writeCommand(commandFrame(enabled ? 0x52 : 0x55));
    emit autotrackChanged(autotrackState);
    emit stateChanged();
}

void SteppirController::calibrate()
{
    writeCommand(commandFrame(0x56));
}

void SteppirController::home()
{
    writeCommand(commandFrame(0x53));
}

void SteppirController::reloadSettings()
{
    if (connectedState && currentProfile() != SteppirProfiles::currentProfileName())
        open();
}

void SteppirController::poll()
{
    writeCommand(QByteArray::fromHex("3F410D"));
}

void SteppirController::readNetworkData()
{
    buffer.append(socket->readAll());
    processBuffer();
}

void SteppirController::readSerialData()
{
    buffer.append(serial->readAll());
    processBuffer();
}

void SteppirController::socketConnected()
{
    connectedState = true;
    emit connected();
    emit stateChanged();
    poll();
    pollTimer->start();
}

void SteppirController::socketError()
{
    if (socket->error() == QAbstractSocket::RemoteHostClosedError)
        return;
    emit errorPresent(tr("SteppIR Controller Error"), socket->errorString());
}

void SteppirController::serialError()
{
    if (serial->error() == QSerialPort::NoError)
        return;
    emit errorPresent(tr("SteppIR Controller Error"), serial->errorString());
}

void SteppirController::writeCommand(const QByteArray &command)
{
    if (command.isEmpty())
        return;
    if (activeProfile.type == SteppirProfile::Network && socket->state() == QAbstractSocket::ConnectedState)
        socket->write(command);
    else if (activeProfile.type == SteppirProfile::Serial && serial->isOpen())
        serial->write(command);
}

QByteArray SteppirController::commandFrame(quint8 command) const
{
    QByteArray frame;
    frame.append(char(0x40));
    frame.append(char(0x41));
    frame.append(command == 0x31 || command == 0x00 ? char(0x00) : char(0x40));

    int encodedFrequency = qMax(0, frequencyKHz) * 100;
    frame.append(char((encodedFrequency >> 16) & 0xff));
    frame.append(char((encodedFrequency >> 8) & 0xff));
    frame.append(char(encodedFrequency & 0xff));
    frame.append(char(0x00));
    frame.append(char(directionBits(currentDirection)));
    frame.append(char(command));
    frame.append(char(0x00));
    frame.append(char(0x0d));
    return frame;
}

void SteppirController::processBuffer()
{
    int terminator = buffer.indexOf('\r');
    while (terminator >= 0)
    {
        const QByteArray frame = buffer.left(terminator + 1);
        buffer.remove(0, terminator + 1);
        processResponse(frame);
        terminator = buffer.indexOf('\r');
    }
}

void SteppirController::processResponse(const QByteArray &frame)
{
    if (frame.size() < 8 || static_cast<unsigned char>(frame.at(0)) != 0x40)
        return;

    int encodedFrequency = (static_cast<unsigned char>(frame.at(3)) << 16)
                         | (static_cast<unsigned char>(frame.at(4)) << 8)
                         | static_cast<unsigned char>(frame.at(5));
    int newFrequency = encodedFrequency / 100;
    bool newTuning = static_cast<unsigned char>(frame.at(6)) != 0;
    quint8 status = static_cast<unsigned char>(frame.at(7));
    Direction newDirection = directionFromBits((status & 0xe0) >> 5);
    bool newAutotrack = ((status & 0x04) >> 2) == 1;

    if (newFrequency != frequencyKHz)
    {
        frequencyKHz = newFrequency;
        emit tunedFrequencyChanged(frequencyKHz);
    }
    if (newTuning != tuningState)
    {
        tuningState = newTuning;
        emit tuningChanged(tuningState);
    }
    if (newDirection != currentDirection)
    {
        currentDirection = newDirection;
        emit directionChanged(currentDirection);
    }
    if (newAutotrack != autotrackState)
    {
        autotrackState = newAutotrack;
        emit autotrackChanged(autotrackState);
    }
    emit stateChanged();
}

quint8 SteppirController::directionBits(Direction direction) const
{
    switch (direction)
    {
    case OneEighty:
        return 0x40;
    case BiDir:
        return 0x80;
    case ThreeQuarter:
        return 0x20;
    case Normal:
    default:
        return 0x00;
    }
}

SteppirController::Direction SteppirController::directionFromBits(quint8 value) const
{
    switch (value)
    {
    case 2:
        return OneEighty;
    case 4:
        return BiDir;
    case 3:
        return ThreeQuarter;
    case 0:
    default:
        return Normal;
    }
}
