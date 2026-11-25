#ifndef QLOG_AMPLIFIER_AMPLIFIERCONTROLLER_H
#define QLOG_AMPLIFIER_AMPLIFIERCONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QTcpSocket>
#include <QSerialPort>
#include <QVector>

class QJsonObject;
class QJsonValue;
class QSerialPortInfo;

struct AmplifierProfile
{
    // Band slots in SPE Expert status-packet order (160m ... 6m).
    static const int BAND_COUNT = 10;
    static const int MAX_DRIVE_POWER_W = 200;

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
    // Rig drive power applied while the amplifier is in Operate, per band;
    // 0 leaves the rig power unchanged for that band.
    bool drivePowerControl = false;
    QVector<int> halfDrivePowerW = QVector<int>(BAND_COUNT, 0);
    QVector<int> fullDrivePowerW = QVector<int>(BAND_COUNT, 0);

    int drivePowerW(int band, bool fullMode) const
    {
        if ( band < 0 || band >= BAND_COUNT )
            return 0;
        return fullMode ? fullDrivePowerW.at(band) : halfDrivePowerW.at(band);
    }

    bool operator==(const AmplifierProfile &other) const
    {
        return profileName == other.profileName
               && model == other.model
               && connectionType == other.connectionType
               && serialPort == other.serialPort
               && baudRate == other.baudRate
               && host == other.host
               && port == other.port
               && drivePowerControl == other.drivePowerControl
               && halfDrivePowerW == other.halfDrivePowerW
               && fullDrivePowerW == other.fullDrivePowerW;
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
    // High nibble of status byte 22; 6 means RS-232, -1 means unknown.
    int cat = -1;
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

private:
    static QJsonObject toJson(const AmplifierProfile &profile);
    static AmplifierProfile fromJson(const QJsonObject &obj);
    static QVector<int> drivePowerFromJson(const QJsonValue &value);
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
    void setFrequencyKHz(int frequencyKHz);
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
    // SPE Expert serial protocol.
    static constexpr quint8 PC_SYN = 0x55;
    static constexpr quint8 AMP_SYN = 0xaa;
    static constexpr quint8 STATUS_COUNT = 0x1e;

    static constexpr quint8 CMD_KEY_ON = 0x10;
    static constexpr quint8 CMD_RCU_ON = 0x80;
    static constexpr quint8 CMD_RCU_OFF = 0x81;
    static constexpr int CAT_RS232 = 6;

    static constexpr quint8 KEY_OFF = 0x18;
    static constexpr quint8 KEY_MODE = 0x1a;
    static constexpr quint8 KEY_OPERATE = 0x1c;
    static constexpr quint8 KEY_INPUT = 0x28;
    static constexpr quint8 KEY_BAND_MINUS = 0x29;
    static constexpr quint8 KEY_BAND_PLUS = 0x2a;
    static constexpr quint8 KEY_ANTENNA = 0x2b;
    static constexpr quint8 KEY_LEFT = 0x2d;
    static constexpr quint8 KEY_RIGHT = 0x2e;
    static constexpr quint8 KEY_TUNE = 0x34;

    static constexpr quint8 FLAG_TUNE = 0x01;
    static constexpr quint8 FLAG_OPERATE = 0x02;
    static constexpr quint8 FLAG_TX = 0x04;
    static constexpr quint8 FLAG_ALARM = 0x08;
    static constexpr quint8 FLAG_FULL_MODE = 0x10;
    static constexpr quint8 FLAG_TEMP_CELSIUS = 0x80;

    static QString connectionTypeName(AmplifierProfile::ConnectionType type);
    static QString serialErrorName(QSerialPort::SerialPortError error);
    static QStringList availableSerialPorts();
    static QSerialPortInfo serialPortInfoForName(const QString &configuredPort);
    static QString normalizedSerialPort(const QString &configuredPort);
    static QStringList serialPortOpenCandidates(const QString &primaryPort);
    static quint8 keyCodeForCommand(AmplifierController::Command command);

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
    bool frequencyReportingAllowed() const;
    void updateRigDrivePower(const AmplifierStatus &previous);

    AmplifierProfile activeProfile;
    AmplifierStatus currentStatus;
    int pendingFrequencyKHz = 0;
    int lastReportedFrequencyKHz = 0;
    // Last rig power reading in W (0 when unknown) and the value saved on
    // entering Operate, restored on Standby.
    double rigPowerW = 0.0;
    double savedRigPowerW = 0.0;
    bool connectedState = false;
    bool enabledState = false;
    bool openingSerialPort = false;

    QTcpSocket *socket = nullptr;
    QSerialPort *serial = nullptr;
    QTimer *rcuRetryTimer = nullptr;

    QByteArray packetBuffer;
    ParseState parseState = ParseState::WaitSyn1;
    quint8 expectedCount = 0;
};

Q_DECLARE_METATYPE(AmplifierStatus)

#endif // QLOG_AMPLIFIER_AMPLIFIERCONTROLLER_H
