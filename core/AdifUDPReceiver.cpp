#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QHostAddress>
#include <QSqlTableModel>
#include <QTextStream>

#include "AdifUDPReceiver.h"
#include "core/LogParam.h"
#include "core/debug.h"
#include "logformat/AdiFormat.h"

MODULE_IDENTIFICATION("qlog.core.adifudp");

AdifUDPReceiver::AdifUDPReceiver(QObject *parent) :
    QObject(parent),
    socket(new QUdpSocket(this))
{
    FCT_IDENTIFICATION;

    connect(socket, &QUdpSocket::readyRead, this, &AdifUDPReceiver::readPendingDatagrams);
    reloadSetting();
}

quint16 AdifUDPReceiver::getConfigPort()
{
    FCT_IDENTIFICATION;

    const quint16 ret = LogParam::getNetworkAdifListenerPort(DEFAULT_PORT);

    qCDebug(runtime) << "ADIF UDP configured port" << ret;

    return ret;
}

void AdifUDPReceiver::saveConfigPort(quint16 port)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << port;

    LogParam::setNetworkAdifListenerPort(port);
}

bool AdifUDPReceiver::getConfigEnabled()
{
    FCT_IDENTIFICATION;

    const bool ret = LogParam::getNetworkAdifListenerEnabled();

    qCDebug(runtime) << "ADIF UDP listener enabled" << ret;

    return ret;
}

void AdifUDPReceiver::saveConfigEnabled(bool enabled)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << enabled;

    LogParam::setNetworkAdifListenerEnabled(enabled);
}

void AdifUDPReceiver::reloadSetting()
{
    FCT_IDENTIFICATION;

    const bool enabled = getConfigEnabled();

    qCDebug(runtime) << "Reloading ADIF UDP listener settings; enabled" << enabled;

    if ( enabled )
        openPort();
    else
        closePort();
}

void AdifUDPReceiver::openPort()
{
    FCT_IDENTIFICATION;

    if ( !socket )
    {
        qWarning() << "Cannot open ADIF UDP port - socket is not initialized";
        return;
    }

    if ( socket->state() == QAbstractSocket::BoundState )
    {
        qCDebug(runtime) << "Closing previously bound ADIF UDP port";
        socket->close();
    }

    const quint16 newPort = getConfigPort();

    qCDebug(runtime) << "ADIF UDP listen port" << newPort;

    if ( !socket->bind(QHostAddress::Any, newPort) )
    {
        qWarning() << "Cannot bind the Port for ADIF UDP";
        return;
    }

    qCDebug(runtime) << "ADIF UDP listening on all interfaces";
}

void AdifUDPReceiver::closePort()
{
    FCT_IDENTIFICATION;

    if ( socket && socket->state() == QAbstractSocket::BoundState )
    {
        qCDebug(runtime) << "Closing ADIF UDP listener";
        socket->close();
    }
    else
    {
        qCDebug(runtime) << "ADIF UDP listener is already closed";
    }
}

void AdifUDPReceiver::readPendingDatagrams()
{
    FCT_IDENTIFICATION;

    while ( socket->hasPendingDatagrams() )
    {
        const QNetworkDatagram datagram = socket->receiveDatagram();
        qCDebug(runtime) << "Received ADIF UDP datagram from" << datagram.senderAddress()
                         << datagram.senderPort()
                         << "bytes" << datagram.data().size();
        processDatagram(datagram.data());
    }
}

void AdifUDPReceiver::processDatagram(const QByteArray &data)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << "bytes" << data.size();

    QString adif = QString::fromUtf8(data);
    if ( adif.isEmpty() )
    {
        qCDebug(runtime) << "ADIF UDP datagram is not valid UTF-8, trying Latin-1";
        adif = QString::fromLatin1(data);
    }

    const int start = adif.indexOf(QLatin1Char('<'));
    if ( start < 0 )
    {
        qWarning() << "Ignoring ADIF UDP datagram without ADIF tags";
        return;
    }

    if ( start > 0 )
        qCDebug(runtime) << "Skipping ADIF UDP datagram prefix bytes" << start;

    emitRecords(adif.mid(start));
}

void AdifUDPReceiver::emitRecords(const QString &adif)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << "characters" << adif.size();

    QString input(adif);
    QTextStream in(&input);
    AdiFormat adi(in);

    QSqlTableModel model;
    model.setTable("contacts");
    model.removeColumn(model.fieldIndex("id"));

    bool imported = false;
    int recordCount = 0;
    while ( true )
    {
        QSqlRecord record = model.record(0);
        if ( !adi.importNext(record) )
            break;

        if ( record.value("callsign").toString().isEmpty() )
        {
            qWarning() << "Ignoring ADIF UDP record without callsign" << record;
            continue;
        }

        const int qslSentIndex = record.indexOf("qsl_sent");
        if ( qslSentIndex >= 0 )
            record.remove(qslSentIndex);

        const int lotwQslSentIndex = record.indexOf("lotw_qsl_sent");
        if ( lotwQslSentIndex >= 0 )
            record.remove(lotwQslSentIndex);

        const int eqslQslSentIndex = record.indexOf("eqsl_qsl_sent");
        if ( eqslQslSentIndex >= 0 )
            record.remove(eqslQslSentIndex);

        qCDebug(runtime) << "Emitting ADIF UDP contact"
                         << record.value("callsign").toString()
                         << record.value("start_time")
                         << record.value("freq")
                         << record.value("band")
                         << record.value("mode");
        qCDebug(function_parameters) << record;

        emit addContact(record);
        imported = true;
        recordCount++;
    }

    if ( !imported )
        qWarning() << "ADIF UDP datagram did not contain an importable QSO record";
    else
        qCDebug(runtime) << "Imported ADIF UDP records" << recordCount;
}
