#ifndef QLOG_AMPLIFIER_AMPLIFIERCONTROLLER_H
#define QLOG_AMPLIFIER_AMPLIFIERCONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QTcpSocket>
#include <QSerialPort>

struct AmplifierProfile
{
    enum AmplifierModel
    {
        SPE_EXPERT_1K = 0
    };

    enum ConnectionType
    {
        Serial = 0,
        Network = 1
    };

    QString profileName;
    AmplifierModel model = SPE_EXPERT_1K;
    ConnectionType connectionType = Serial;
    QString serialPort;
    int baudRate = 9600;
    QString host;
    int port = 5000;

    bool operator==(const AmplifierProfile &other) const
    {
        return profileName == other.profileName
               && model == other.model
               && connectionType == other.connectionType
               && serialPort == other.serialPort
               && baudRate == other.baudRate
               && host == other.host
               && port == other.port;
    }

    bool operator!=(const AmplifierProfile &other) const { return !operator==(other); }
};

struct AmplifierStatus
{
    bool operate = false;
    bool tuning = false;
    bool tx = false;
    bool alarm = false;
    bool fullMode = false;
    bool tempCelsius = false;

    int band = -1;
    int input = 0;
    int antenna = 0;
    quint16 freqKHz = 0;

    double swr = 1.0;
    double gainDb = 0.0;
    int temperature = 0;
    double paOutW = 0.0;
    double reverseW = 0.0;
    double voltageV = 0.0;
    double currentA = 0.0;
};

class AmplifierProfiles
{
public:
    static QList<AmplifierProfile> profiles();
    static QStringList profileNames();
    static AmplifierProfile profile(const QString &profileName);
    static void saveProfiles(const QList<AmplifierProfile> &profiles);
    static void addOrReplace(const AmplifierProfile &profile);
    static void remove(const QString &profileName);
    static QString currentProfileName();
    static void setCurrentProfileName(const QString &profileName);
};

class AmplifierController : public QObject
{
    Q_OBJECT

public:
    enum Command
    {
        Operate,
        Standby,
        Tune,
        Mode,
        Antenna,
        Input,
        Left,
        Right,
        BandDown,
        BandUp
    };
    Q_ENUM(Command)

    static AmplifierController *instance();

    bool isConnected() const;
    bool isEnabled() const;
    QString currentProfile() const;
    AmplifierStatus status() const;

public slots:
    void open();
    void openProfile(const QString &profileName);
    void close();
    void reloadSettings();
    void sendCommand(AmplifierController::Command command);

signals:
    void connected();
    void disconnected();
    void statusChanged(const AmplifierStatus &status);
    void errorPresent(const QString &error, const QString &errorDetail);

private slots:
    void readNetworkData();
    void readSerialData();
    void socketConnected();
    void socketError();
    void serialError(QSerialPort::SerialPortError error);
    void rcuRetry();

private:
    explicit AmplifierController(QObject *parent = nullptr);
    ~AmplifierController();

    enum class ParseState { WaitSyn1, WaitSyn2, WaitSyn3, ReadCount, ReadData, ReadChecksum };

    QByteArray buildKeyCommand(quint8 keyCode) const;
    QByteArray buildRcuCommand(bool enabled) const;
    void writeCommand(const QByteArray &command);
    void processBytes(const QByteArray &data);
    void dispatchPacket(const QByteArray &packet);
    bool parseSpeStatus(const QByteArray &packet);
    void setConnected(bool connected);
    void resetStatus();

    AmplifierProfile activeProfile;
    AmplifierStatus currentStatus;
    bool connectedState = false;
    bool enabledState = false;

    QTcpSocket *socket = nullptr;
    QSerialPort *serial = nullptr;
    QTimer *rcuRetryTimer = nullptr;

    QByteArray packetBuffer;
    ParseState parseState = ParseState::WaitSyn1;
    quint8 expectedCount = 0;
};

Q_DECLARE_METATYPE(AmplifierStatus)

#endif // QLOG_AMPLIFIER_AMPLIFIERCONTROLLER_H
