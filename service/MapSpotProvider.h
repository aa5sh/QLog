#ifndef QLOG_SERVICE_MAPSPOTPROVIDER_H
#define QLOG_SERVICE_MAPSPOTPROVIDER_H

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QTcpSocket;
class QTimer;

struct MapReceptionSpot
{
    QString source;
    QString id;
    QString remoteCallsign;
    QString remoteLocator;
    QString mode;
    QString band;
    QString direction;
    double frequencyMHz = 0.0;
    int signal = 0;
    QDateTime timestamp;
};

class MapSpotProvider : public QObject
{
    Q_OBJECT

public:
    explicit MapSpotProvider(QObject *parent = nullptr);
    ~MapSpotProvider() override;

    void setPskEnabled(bool enabled);
    void setRbnEnabled(bool enabled);
    void setStationCallsign(const QString &callsign);
    void setBandFilter(const QString &band, bool enabled);

signals:
    void spotReceived(const MapReceptionSpot &spot);
    void sourceCleared(const QString &source);

private slots:
    void connectPsk();
    void readPsk();
    void pskDisconnected();
    void refreshRbnNodes();
    void rbnNodesReceived(QNetworkReply *reply);

private:
    static QByteArray mqttString(const QByteArray &value);
    static QByteArray mqttPacket(quint8 header, const QByteArray &payload);
    static QString normalizedCallsign(const QString &callsign);
    static QString htmlText(QString value);

    void restartPsk();
    void sendPskConnect();
    void subscribePsk();
    void processPskPacket(quint8 header, const QByteArray &payload);
    void processPskMessage(const QByteArray &payload);

    void startRbnSocket(quint16 port);
    void stopRbnSockets();
    void readRbn(QTcpSocket *socket);
    void processRbnLine(const QByteArray &line);
    void scheduleRbnReconnect(QTcpSocket *socket);

    QTcpSocket *pskSocket;
    QTimer *pskReconnectTimer;
    QTimer *pskPingTimer;
    QByteArray pskBuffer;
    quint16 mqttPacketId;

    QNetworkAccessManager *networkManager;
    QList<QTcpSocket *> rbnSockets;
    QHash<QTcpSocket *, QByteArray> rbnBuffers;
    QHash<QString, QString> rbnNodeLocators;

    QString stationCallsign;
    QString currentBand;
    bool filterByBand;
    bool pskEnabled;
    bool rbnEnabled;
};

#endif // QLOG_SERVICE_MAPSPOTPROVIDER_H
