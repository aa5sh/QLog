#ifndef QLOG_CORE_ADIFUDPRECEIVER_H
#define QLOG_CORE_ADIFUDPRECEIVER_H

#include <QObject>
#include <QSqlRecord>

class QUdpSocket;

class AdifUDPReceiver : public QObject
{
    Q_OBJECT
public:
    explicit AdifUDPReceiver(QObject *parent = nullptr);

    static quint16 getConfigPort();
    static void saveConfigPort(quint16 port);
    static bool getConfigEnabled();
    static void saveConfigEnabled(bool enabled);

signals:
    void addContact(QSqlRecord record);

public slots:
    void reloadSetting();

private slots:
    void readPendingDatagrams();

private:
    void openPort();
    void closePort();
    void processDatagram(const QByteArray &data);
    void emitRecords(const QString &adif);

    QUdpSocket *socket;

    static const int DEFAULT_PORT = 2333;
};

#endif // QLOG_CORE_ADIFUDPRECEIVER_H
