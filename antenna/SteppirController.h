#ifndef QLOG_ANTENNA_STEPPIRCONTROLLER_H
#define QLOG_ANTENNA_STEPPIRCONTROLLER_H

#include <QObject>
#include <QHash>

class QSerialPort;
class QTcpSocket;
class QTimer;

struct SteppirProfile
{
    enum ConnectionType
    {
        Network = 0,
        Serial = 1
    };

    QString name;
    ConnectionType type = Network;
    QString host;
    quint16 port = 0;
    QString serialPort;
    int baudRate = 9600;
};

class SteppirProfiles
{
public:
    static QList<SteppirProfile> profiles();
    static QStringList profileNames();
    static SteppirProfile profile(const QString &name);
    static void saveProfiles(const QList<SteppirProfile> &profiles);
    static void addOrReplace(const SteppirProfile &profile);
    static void remove(const QString &name);
    static QString currentProfileName();
    static void setCurrentProfileName(const QString &name);
};

class SteppirController : public QObject
{
    Q_OBJECT

public:
    enum Direction
    {
        Normal,
        OneEighty,
        BiDir,
        ThreeQuarter
    };
    Q_ENUM(Direction)

    static SteppirController *instance();

    bool isConnected() const;
    QString currentProfile() const;
    int tunedFrequencyKHz() const;
    Direction direction() const;
    bool autotrack() const;
    bool tuning() const;

public slots:
    void open();
    void openProfile(const QString &profileName);
    void close();
    void setFrequencyHz(double frequencyHz);
    void setFrequencyKHz(int frequencyKHz);
    void setDirection(SteppirController::Direction direction);
    void setAutotrack(bool enabled);
    void calibrate();
    void home();
    void reloadSettings();

signals:
    void connected();
    void disconnected();
    void errorPresent(const QString &error, const QString &detail);
    void stateChanged();
    void tunedFrequencyChanged(int frequencyKHz);
    void directionChanged(SteppirController::Direction direction);
    void autotrackChanged(bool enabled);
    void tuningChanged(bool tuning);

private slots:
    void poll();
    void readNetworkData();
    void readSerialData();
    void socketConnected();
    void socketError();
    void serialError();

private:
    explicit SteppirController(QObject *parent = nullptr);
    ~SteppirController();

    void writeCommand(const QByteArray &command);
    QByteArray commandFrame(quint8 command) const;
    void processBuffer();
    void processResponse(const QByteArray &frame);
    quint8 directionBits(Direction direction) const;
    Direction directionFromBits(quint8 value) const;

    QTcpSocket *socket;
    QSerialPort *serial;
    QTimer *pollTimer;
    QByteArray buffer;
    SteppirProfile activeProfile;
    bool hasActiveProfile;
    bool connectedState;
    int frequencyKHz;
    Direction currentDirection;
    bool autotrackState;
    bool tuningState;
};

#endif // QLOG_ANTENNA_STEPPIRCONTROLLER_H
