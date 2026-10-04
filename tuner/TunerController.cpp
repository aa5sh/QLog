#include "tuner/TunerController.h"
#include "core/LogParam.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

namespace {
    QJsonObject toJson(const TunerProfile &profile)
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

    TunerProfile fromJson(const QJsonObject &obj)
    {
        TunerProfile profile;
        profile.profileName = obj["profileName"].toString();
        profile.model = static_cast<TunerProfile::TunerModel>(obj["model"].toInt(TunerProfile::ELECRAFT_KAT500));
        profile.connectionType = static_cast<TunerProfile::ConnectionType>(obj["connectionType"].toInt(TunerProfile::Serial));
        profile.serialPort = obj["serialPort"].toString();
        profile.baudRate = obj["baudRate"].toInt(38400);
        profile.host = obj["host"].toString();
        profile.port = obj["port"].toInt(5000);
        return profile;
    }

}

QList<TunerProfile> TunerProfiles::profiles()
{
    QList<TunerProfile> ret;
    const QJsonDocument doc = QJsonDocument::fromJson(LogParam::getTunerProfiles().toUtf8());
    for (const QJsonValue &value : doc.array())
    {
        TunerProfile profile = fromJson(value.toObject());
        if (!profile.profileName.isEmpty())
            ret << profile;
    }
    return ret;
}

QStringList TunerProfiles::profileNames()
{
    QStringList ret;
    for (const TunerProfile &profile : profiles())
        ret << profile.profileName;
    return ret;
}

TunerProfile TunerProfiles::profile(const QString &profileName)
{
    for (const TunerProfile &profile : profiles())
    {
        if (profile.profileName == profileName)
            return profile;
    }
    return TunerProfile();
}

void TunerProfiles::saveProfiles(const QList<TunerProfile> &profiles)
{
    QJsonArray array;
    for (const TunerProfile &profile : profiles)
        array.append(toJson(profile));

    LogParam::setTunerProfiles(QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)));
}

void TunerProfiles::addOrReplace(const TunerProfile &profile)
{
    QList<TunerProfile> all = profiles();
    bool replaced = false;
    for (TunerProfile &existing : all)
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

void TunerProfiles::remove(const QString &profileName)
{
    QList<TunerProfile> remaining;
    for (const TunerProfile &profile : profiles())
    {
        if (profile.profileName != profileName)
            remaining << profile;
    }
    saveProfiles(remaining);
    if (currentProfileName() == profileName) setCurrentProfileName(QString());
}

QString TunerProfiles::currentProfileName()
{
    return LogParam::getTunerCurrentProfile();
}

void TunerProfiles::setCurrentProfileName(const QString &profileName)
{
    LogParam::setTunerCurrentProfile(profileName);
}

TunerController *TunerController::instance()
{
    static TunerController controller;
    return &controller;
}

TunerController::TunerController(QObject *parent) : QObject(parent)
{
    pollTimer.setInterval(500);
    replyTimer.setSingleShot(true);
    replyTimer.setInterval(1500);
    frequencyTimer.setSingleShot(true);
    frequencyTimer.setInterval(150);
    connect(&socket, &QTcpSocket::connected, this, &TunerController::transportReady);
    connect(&socket, &QTcpSocket::readyRead, this, [this]() { readData(socket.readAll()); });
    connect(&serial, &QSerialPort::readyRead, this, [this]() { readData(serial.readAll()); });
    connect(&socket, &QTcpSocket::disconnected, this, [this]() {
        if (enabledState && !closing) fail(tr("Tuner network connection closed"));
    });
    connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, [this]() { if (!closing && enabledState) fail(socket.errorString()); });
    connect(&serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (error != QSerialPort::NoError && !closing && enabledState) fail(serial.errorString());
    });
    connect(&pollTimer, &QTimer::timeout, this, &TunerController::poll);
    connect(&replyTimer, &QTimer::timeout, this, [this]() {
        if (!connectedState && expectedReply == ";" && ++wakeAttempts < 30)
        {
            write(";");
            replyTimer.start(100);
        }
        else fail(tr("KAT500 did not respond. Check the connection and serial baud rate."));
    });
    connect(&frequencyTimer, &QTimer::timeout, this, [this]() {
        if (connectedState && currentStatus.poweredOn && !currentStatus.tuning
            && pendingFrequency > 0 && pendingFrequency != lastFrequency)
        {
            enqueue("F " + QByteArray::number(pendingFrequency) + ";F;", "F");
            lastFrequency = pendingFrequency;
        }
    });
}

void TunerController::open()
{
    openProfile(TunerProfiles::currentProfileName());
}

void TunerController::openProfile(const QString &name)
{
    close();
    activeProfile = TunerProfiles::profile(name);
    if (activeProfile.profileName.isEmpty())
    {
        fail(tr("Select a tuner profile in Settings → Equipment → Amplifiers → Tuner Profiles."));
        return;
    }
    enabledState = true;
    if (activeProfile.connectionType == TunerProfile::Network)
    {
        if (activeProfile.host.trimmed().isEmpty() || activeProfile.port < 1 || activeProfile.port > 65535)
        {
            fail(tr("Invalid tuner network address or port"));
            return;
        }
        // Bound connection establishment as well as protocol response waits.
        expectedReply = "CONNECT";
        replyTimer.start(5000);
        socket.connectToHost(activeProfile.host.trimmed(), activeProfile.port);
    }
    else
    {
        if (activeProfile.serialPort.trimmed().isEmpty()
            || !QList<int>({4800, 9600, 19200, 38400}).contains(activeProfile.baudRate))
        {
            fail(tr("Select a serial port and a KAT500 baud rate (4800, 9600, 19200 or 38400)."));
            return;
        }
        serial.setPortName(activeProfile.serialPort.trimmed());
        serial.setBaudRate(activeProfile.baudRate);
        serial.setDataBits(QSerialPort::Data8);
        serial.setParity(QSerialPort::NoParity);
        serial.setStopBits(QSerialPort::OneStop);
        serial.setFlowControl(QSerialPort::NoFlowControl);
        if (!serial.open(QIODevice::ReadWrite))
        {
            if (enabledState) fail(serial.errorString());
            return;
        }
        transportReady();
    }
}

void TunerController::transportReady()
{
    // Wake sleeping firmware with single semicolons before identifying the device.
    expectedReply = ";";
    wakeAttempts = 0;
    write(";");
    replyTimer.start(100);
}

void TunerController::close()
{
    closing = true;
    enabledState = false;
    connectedState = false;
    pollTimer.stop();
    replyTimer.stop();
    frequencyTimer.stop();
    requests.clear();
    expectedReply.clear();
    buffer.clear();
    lastFrequency = 0;
    socket.abort();
    serial.close();
    currentStatus = TunerStatus();
    closing = false;
    emit disconnected();
    emit statusChanged(currentStatus);
}

void TunerController::reloadSettings()
{
    if (enabledState && activeProfile != TunerProfiles::profile(TunerProfiles::currentProfileName()))
        open();
}

void TunerController::fail(const QString &message)
{
    close();
    emit errorPresent(tr("KAT500 connection error"), message);
}

void TunerController::write(const QByteArray &data)
{
    if (activeProfile.connectionType == TunerProfile::Network)
        socket.write(data);
    else if (serial.isOpen())
        serial.write(data);
}

void TunerController::enqueue(const QByteArray &command, const QByteArray &reply)
{
    if (!connectedState) return;
    // Keep a stalled peer from accumulating an unbounded queue.
    if (requests.size() >= 32) return;
    requests.enqueue({command, reply});
    nextRequest();
}

void TunerController::nextRequest()
{
    if (!expectedReply.isEmpty() || requests.isEmpty() || !connectedState) return;
    const Request request = requests.dequeue();
    expectedReply = request.reply;
    write(request.command);
    replyTimer.start();
}

void TunerController::poll()
{
    if (!connectedState || !requests.isEmpty() || !expectedReply.isEmpty()) return;
    static const QList<QByteArray> queries = {
        "PS", "TP", "AN", "MD", "F", "BN", "VSWR", "VSWRB", "BYP", "AMPI", "ATTN", "FLT", "L", "C", "SIDE"
    };
    for (const QByteArray &query : queries) enqueue(query + ';', query);
}

void TunerController::readData(const QByteArray &data)
{
    buffer += data;
    if (buffer.size() > 4096)
    {
        fail(tr("Invalid response from tuner"));
        return;
    }
    int end;
    while ((end = buffer.indexOf(';')) >= 0)
    {
        const QByteArray response = buffer.left(end).trimmed().toUpper();
        buffer.remove(0, end + 1);
        if (!connectedState)
        {
            if (expectedReply == ";" && response.isEmpty())
            {
                replyTimer.stop();
                expectedReply = "KAT500";
                write("I;");
                replyTimer.start(1500);
            }
            else if (expectedReply == "KAT500" && response == "KAT500")
            {
                replyTimer.stop();
                expectedReply.clear();
                connectedState = true;
                emit connected();
                enqueue("RV;", "RV");
                enqueue("SN;", "SN");
                pollTimer.start();
            }
            continue;
        }
        const bool valid = parseResponse(response);
        // Only a valid reply to the outstanding GET advances the queue.
        // VSWRB must not satisfy VSWR, nor FLTC satisfy FLT.
        const QByteArray key = response.left(expectedReply.size());
        const QByteArray suffix = response.mid(expectedReply.size());
        bool numericFrequency = false;
        suffix.trimmed().toInt(&numericFrequency);
        const bool boundary = (expectedReply != "VSWR" || !suffix.startsWith('B'))
            && (expectedReply != "F" || numericFrequency);
        if (valid && !expectedReply.isEmpty() && key == expectedReply && boundary)
        {
            replyTimer.stop();
            expectedReply.clear();
            nextRequest();
        }
    }
}

bool TunerController::parseResponse(const QByteArray &response)
{
    bool ok = false;
    auto integer = [&](const QByteArray &key, int minimum, int maximum, int &target, int base = 10) {
        if (!response.startsWith(key)) return false;
        int value = response.mid(key.size()).trimmed().toInt(&ok, base);
        if (!ok || value < minimum || value > maximum) return false;
        target = value;
        return true;
    };
    auto boolean = [&](const QByteArray &key, bool &target) {
        if (response != key + '0' && response != key + '1') return false;
        target = response.endsWith('1');
        return true;
    };
    auto swr = [&](const QByteArray &key, double &target) {
        if (!response.startsWith(key)) return false;
        double value = response.mid(key.size()).trimmed().toDouble(&ok);
        if (!ok || !std::isfinite(value) || value < 0 || value > 99.99) return false;
        target = value;
        return true;
    };
    bool valid = boolean("PS", currentStatus.poweredOn)
        || boolean("TP", currentStatus.tuning)
        || boolean("AMPI", currentStatus.amplifierInterrupted)
        || boolean("ATTN", currentStatus.attenuator)
        || integer("AN", 1, 3, currentStatus.antenna)
        || integer("BN", 0, 10, currentStatus.band)
        || integer("FLT", 0, 4, currentStatus.fault)
        || integer("F", 0, 65535, currentStatus.frequencyKHz)
        || integer("L", 0, 255, currentStatus.inductors, 16)
        || integer("C", 0, 255, currentStatus.capacitors, 16)
        || swr("VSWRB", currentStatus.bypassSwr)
        || swr("VSWR", currentStatus.swr);
    if (response == "MDA" || response == "MDM" || response == "MDB")
    {
        currentStatus.mode = response.at(2);
        valid = true;
    }
    else if (response == "SIDET" || response == "SIDEA")
    {
        currentStatus.capacitorSide = response.at(4);
        valid = true;
    }
    else if (response == "BYPB" || response == "BYPN")
    {
        currentStatus.bypass = response == "BYPB";
        valid = true;
    }
    else if (response.startsWith("RV") && response.size() > 2)
    {
        currentStatus.firmware = QString::fromLatin1(response.mid(2).trimmed());
        valid = true;
    }
    else if (response.startsWith("SN") && response.size() > 2)
    {
        currentStatus.serialNumber = QString::fromLatin1(response.mid(2).trimmed());
        valid = true;
    }
    else if (response == "FT")
    {
        currentStatus.tuning = false;
        valid = true;
    }
    if (valid)
    {
        emit statusChanged(currentStatus);
        if (pendingFrequency > 0 && !frequencyTimer.isActive()) frequencyTimer.start();
    }
    return valid;
}

void TunerController::sendCommand(Command command)
{
    if (!connectedState || (!currentStatus.poweredOn && command != PowerOn)) return;
    switch (command)
    {
    case PowerOn: enqueue("PS1;PS;", "PS"); break;
    case PowerOff: enqueue("PS0;PS;", "PS"); break;
    case Auto: enqueue("MDA;MD;", "MD"); break;
    case Manual: enqueue("MDM;MD;", "MD"); break;
    case Bypass: enqueue("MDB;MD;", "MD"); break;
    case Tune: enqueue("T;TP;", "TP"); break;
    case CancelTune: enqueue("CT;TP;", "TP"); break;
    case RecallMemory: enqueue("MT;TP;", "TP"); break;
    case SaveMemory: enqueue("SM;F;", "F"); break;
    case ClearFault: enqueue("FLTC;FLT;", "FLT"); break;
    case TransmitterSide: enqueue("SIDET;SIDE;", "SIDE"); break;
    case AntennaSide: enqueue("SIDEA;SIDE;", "SIDE"); break;
    }
}

void TunerController::setAntenna(int antenna)
{
    if (connectedState && currentStatus.poweredOn && !currentStatus.tuning && antenna >= 1 && antenna <= 3)
        enqueue("AN" + QByteArray::number(antenna) + ";AN;", "AN");
}

void TunerController::setInductors(int value)
{
    if (connectedState && currentStatus.poweredOn && !currentStatus.tuning && !currentStatus.bypass && value >= 0 && value <= 255)
        enqueue("L" + QByteArray::number(value, 16).rightJustified(2, '0').toUpper() + ";L;", "L");
}

void TunerController::setCapacitors(int value)
{
    if (connectedState && currentStatus.poweredOn && !currentStatus.tuning && !currentStatus.bypass && value >= 0 && value <= 255)
        enqueue("C" + QByteArray::number(value, 16).rightJustified(2, '0').toUpper() + ";C;", "C");
}

void TunerController::setFrequencyKHz(int frequency)
{
    if (frequency <= 0 || frequency > 54000)
    {
        frequencyTimer.stop();
        pendingFrequency = 0;
        lastFrequency = 0;
        return;
    }
    pendingFrequency = frequency;
    frequencyTimer.start();
}
