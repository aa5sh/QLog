#ifndef QLOG_SERVICE_RBN_RBNNETWORK_H
#define QLOG_SERVICE_RBN_RBNNETWORK_H

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>

#include "data/HeardMeSpot.h"

class QNetworkAccessManager;
class QNetworkReply;
class QTcpSocket;
class QTimer;

class RBNNetwork : public QObject
{
    Q_OBJECT

public:
    explicit RBNNetwork(QObject *parent = nullptr);

    void setCallsign(const QString &callsign);
    void setCurrentMode(const QString &mode);

signals:
    void heardMeSpotReceived(HeardMeSpot spot);

private slots:
    void connectToServer();
    void socketConnected();
    void socketDisconnected();
    void socketReadyRead();
    void socketError();
    void connectionTimedOut();
    void refreshNodeList();
    void nodeListReceived(QNetworkReply *reply);

private:
    enum Configuration
    {
        RbnPort = 7000,
        ConnectionTimeoutMs = 30000,
        InitialReconnectDelayMs = 1000,
        MaximumReconnectDelayMs = 30000,
        ReconnectBackoffMultiplier = 2,
        NodeListRefreshIntervalMs = 24 * 60 * 60 * 1000,
        UnknownNodeRefreshIntervalSeconds = 60 * 60,
        MaximumLineLength = 512
    };

    enum class State
    {
        Disconnected,
        Connecting,
        AwaitingCallsignPrompt,
        AwaitingLoginConfirmation,
        Ready
    };

    static const char RbnHost[];
    static const char RbnNodeListUrl[];
    static const char CallsignPrompt[];

    bool shouldBeConnected() const;
    bool lineMatchesCallsign(const char *line, int length) const;
    void updateConnection();
    void stopConnection();
    void scheduleReconnect();
    void processMatchingSpot(const char *line, int length);
    void processLoginLine(const char *line, int length);
    void requestNodeListForUnknownNode();

    static QString htmlText(QString value);
    static QString stationCallsign(const QString &nodeCallsign);

    QString callsign;
    QByteArray callsignBytes;
    QString currentMode;
    QTcpSocket *socket;
    QTimer *reconnectTimer;
    QTimer *connectionTimer;
    QTimer *nodeListRefreshTimer;
    QNetworkAccessManager *networkManager;
    QByteArray receiveBuffer;
    QHash<QString, QString> nodeLocators;
    QDateTime lastNodeListRequest;
    State state;
    int reconnectDelayMs;
    bool nodeListRequestPending;
};

#endif // QLOG_SERVICE_RBN_RBNNETWORK_H
