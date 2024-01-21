#ifndef QLOG_CORE_FLDIGI_H
#define QLOG_CORE_FLDIGI_H

#include <QTcpServer>
#include <QSqlRecord>

class QXmlStreamReader;

class Fldigi : public QTcpServer {
    Q_OBJECT

public:
    explicit Fldigi(QObject *parent = nullptr);

signals:
    void addContact(QSqlRecord);

public slots:
    void readClient();
    void discardClient();

private:
    void processMethodCall(QTcpSocket* sock, QXmlStreamReader& xml);
    QString parseParam(QXmlStreamReader& xml);
    QByteArray listMethods();
    QByteArray addRecord(QString data);

    void incomingConnection(qintptr socket) override;
};

#endif // QLOG_CORE_FLDIGI_H
