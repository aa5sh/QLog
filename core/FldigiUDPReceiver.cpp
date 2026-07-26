#include <QBuffer>
#include <QNetworkDatagram>
#include <QSqlTableModel>
#include <QTextStream>
#include <QUdpSocket>

#include "FldigiUDPReceiver.h"
#include "LogParam.h"
#include "debug.h"
#include "logformat/AdiFormat.h"

MODULE_IDENTIFICATION("qlog.core.fldigiudpreceiver");

FldigiUDPReceiver::FldigiUDPReceiver(QObject *parent)
    : QObject(parent),
      socket(new QUdpSocket(this))
{
    FCT_IDENTIFICATION;

    connect(socket, &QUdpSocket::readyRead,
            this, &FldigiUDPReceiver::readPendingDatagrams);

    openPort();
}

quint16 FldigiUDPReceiver::getConfigPort()
{
    FCT_IDENTIFICATION;

    return LogParam::getNetworkFldigiUDPListenerPort(DEFAULT_PORT);
}

void FldigiUDPReceiver::saveConfigPort(quint16 port)
{
    FCT_IDENTIFICATION;

    LogParam::setNetworkFldigiUDPListenerPort(port);
}

void FldigiUDPReceiver::openPort()
{
    FCT_IDENTIFICATION;

    if ( socket->state() == QAbstractSocket::BoundState )
        socket->close();

    const quint16 port = getConfigPort();

    if ( !socket->bind(QHostAddress::Any, port) )
        qWarning() << "Cannot bind the FLDigi ADIF UDP port" << port;
    else
        qCDebug(runtime) << "Listening for FLDigi ADIF UDP on port" << port;
}

void FldigiUDPReceiver::readPendingDatagrams()
{
    FCT_IDENTIFICATION;

    while ( socket->hasPendingDatagrams() )
        processDatagram(socket->receiveDatagram().data());
}

void FldigiUDPReceiver::processDatagram(QByteArray data)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << data;

    if ( data.startsWith("<FLDIGI_TEST:") )
    {
        emit testMessageReceived();
        return;
    }

    QSqlTableModel model;
    model.setTable("contacts");
    model.removeColumn(model.fieldIndex("id"));

    QSqlRecord record = model.record(0);

    QBuffer buffer(&data);
    buffer.open(QIODevice::ReadOnly);

    QTextStream in(&buffer);
    AdiFormat adif(in);

    if ( adif.importNext(record) )
        emit addContact(record);
}

void FldigiUDPReceiver::reloadSetting()
{
    FCT_IDENTIFICATION;

    openPort();
}
