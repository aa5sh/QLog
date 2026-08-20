#ifndef QLOG_CORE_FLDIGIUDPRECEIVER_H
#define QLOG_CORE_FLDIGIUDPRECEIVER_H

#include <QByteArray>
#include <QObject>
#include <QSqlRecord>

class QUdpSocket;

class FldigiUDPReceiver : public QObject
{
    Q_OBJECT

public:
    explicit FldigiUDPReceiver(QObject *parent = nullptr);

    static quint16 getConfigPort();
    static void saveConfigPort(quint16 port);

signals:
    void addContact(QSqlRecord);
    void testMessageReceived();

public slots:
    void reloadSetting();

private slots:
    void readPendingDatagrams();

private:
    void openPort();
    void processDatagram(QByteArray data);

    QUdpSocket *socket;

    static const int DEFAULT_PORT = 9876;
};

#endif // QLOG_CORE_FLDIGIUDPRECEIVER_H
