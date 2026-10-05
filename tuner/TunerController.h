#ifndef QLOG_TUNER_TUNERCONTROLLER_H
#define QLOG_TUNER_TUNERCONTROLLER_H

#include <QObject>
#include <QSerialPort>
#include <QTcpSocket>
#include <QTimer>
#include <QElapsedTimer>
#include <QQueue>

struct TunerProfile
{
    enum TunerModel { ELECRAFT_KAT500 = 0 };
    enum ConnectionType { Serial = 0, Network = 1 };
    QString profileName;
    TunerModel model = ELECRAFT_KAT500;
    ConnectionType connectionType = Serial;
    QString serialPort;
    int baudRate = 38400;
    QString host;
    int port = 5000;
    bool operator==(const TunerProfile &other) const
    {
        return profileName == other.profileName && model == other.model
            && connectionType == other.connectionType && serialPort == other.serialPort
            && baudRate == other.baudRate && host == other.host && port == other.port;
    }
    bool operator!=(const TunerProfile &other) const { return !(*this == other); }
};

struct TunerStatus
{
    bool poweredOn = false;
    bool tuning = false;
    bool bypass = false;
    bool amplifierInterrupted = false;
    bool attenuator = false;
    char mode = '\0';
    char capacitorSide = '\0';
    int antenna = 0;
    int band = -1;
    int frequencyKHz = 0;
    int inductors = -1;
    int capacitors = -1;
    int fault = 0;
    double swr = -1;
    double bypassSwr = -1;
    QString firmware;
    QString serialNumber;
};

class TunerProfiles
{
public:
    static QList<TunerProfile> profiles();
    static QStringList profileNames();
    static TunerProfile profile(const QString &name);
    static void saveProfiles(const QList<TunerProfile> &profiles);
    static void addOrReplace(const TunerProfile &profile);
    static void remove(const QString &name);
    static QString currentProfileName();
    static void setCurrentProfileName(const QString &name);
};

class TunerController : public QObject
{
    Q_OBJECT
public:
    enum Command { PowerOn, PowerOff, Auto, Manual, Bypass, Tune, CancelTune,
                   RecallMemory, SaveMemory, ClearFault, TransmitterSide, AntennaSide };
    Q_ENUM(Command)
    static TunerController *instance();
    bool isConnected() const { return connectedState; }
    bool isEnabled() const { return enabledState; }
    TunerStatus status() const { return currentStatus; }
public slots:
    void open();
    void openProfile(const QString &name);
    void close();
    void reloadSettings();
    void sendCommand(TunerController::Command command);
    void setAntenna(int antenna);
    void setInductors(int value);
    void setCapacitors(int value);
signals:
    void connected();
    void disconnected();
    void statusChanged(const TunerStatus &status);
    void errorPresent(const QString &error, const QString &detail);
private:
    explicit TunerController(QObject *parent = nullptr);
    void transportReady();
    void readData(const QByteArray &data);
    bool parseResponse(const QByteArray &response);
    void poll();
    void write(const QByteArray &data);
    void enqueue(const QByteArray &command, const QByteArray &reply);
    void nextRequest();
    void fail(const QString &message);
    struct Request { QByteArray command; QByteArray reply; };
    QQueue<Request> requests;
    QByteArray expectedReply;
    QByteArray buffer;
    TunerProfile activeProfile;
    TunerStatus currentStatus;
    QSerialPort serial;
    QTcpSocket socket;
    QElapsedTimer sessionElapsed;
    QElapsedTimer requestElapsed;
    QElapsedTimer receiveElapsed;
    QTimer pollTimer;
    QTimer replyTimer;
    bool connectedState = false;
    bool enabledState = false;
    bool closing = false;
    int wakeAttempts = 0;
    int replyRetries = 0;
};
Q_DECLARE_METATYPE(TunerStatus)
#endif
