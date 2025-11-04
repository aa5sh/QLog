#ifndef QLOG_DATA_HOSTSPORTSTRING_H
#define QLOG_DATA_HOSTSPORTSTRING_H

#include <QObject>
#include <QHostAddress>

class HostPortAddress : public QHostAddress
{
public:
    explicit HostPortAddress(const QString &, quint16);
    HostPortAddress(const QHostAddress &host, quint16 port);
    void setPort(quint16);
    quint16 getPort() const;
    bool operator==(const HostPortAddress &other) const;

private:
    quint16 port;
};

class HostsPortString : public QObject
{
    Q_OBJECT
public:
    static const QRegularExpression hostsPortRegEx();

    explicit HostsPortString(const QString &, QObject *parent=nullptr);
    bool hasLocalIPWithPort(int port) const;
    QList<HostPortAddress> getAddrList() const;

private:

    QList<HostPortAddress> addressList;

};

#endif // QLOG_DATA_HOSTSPORTSTRING_H
