#include "WaveshareWidget.h"
#include "ui_WaveshareWidget.h"

#include <QAbstractSocket>
#include <QHBoxLayout>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QSharedPointer>
#include <QTcpSocket>
#include <QtMath>
#include <QTimer>

#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.wavesharewidget");

namespace
{
quint16 crc16Modbus(const QByteArray &data)
{
    quint16 crc = 0xffff;
    for ( unsigned char b : data )
    {
        crc ^= quint16(b);
        for ( int i = 0; i < 8; ++i )
        {
            if ( crc & 0x0001 )
                crc = (crc >> 1) ^ 0xa001;
            else
                crc >>= 1;
        }
    }
    return crc;
}

QByteArray rtuFrame(quint8 unitId, const QByteArray &pdu)
{
    QByteArray frame;
    frame.reserve(pdu.size() + 3);
    frame.append(char(unitId));
    frame.append(pdu);

    const quint16 crc = crc16Modbus(frame);
    frame.append(char(crc & 0xff));
    frame.append(char((crc >> 8) & 0xff));
    return frame;
}

bool hasValidCrc(const QByteArray &frame)
{
    if ( frame.size() < 4 )
        return false;

    const QByteArray withoutCrc = frame.left(frame.size() - 2);
    const quint16 expected = crc16Modbus(withoutCrc);
    const quint16 actual = quint8(frame.at(frame.size() - 2))
                           | (quint16(quint8(frame.at(frame.size() - 1))) << 8);
    return expected == actual;
}
}

WaveshareWidget::WaveshareWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WaveshareWidget),
    refreshTimer(new QTimer(this))
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    refreshTimer->setInterval(15000);
    connect(refreshTimer, &QTimer::timeout, this, &WaveshareWidget::refreshAll);

    reloadSettings();
}

WaveshareWidget::~WaveshareWidget()
{
    shuttingDown = true;
    refreshTimer->stop();
    rtuQueue.clear();

    for ( QProcess *process : findChildren<QProcess *>() )
    {
        process->disconnect(this);
        process->disconnect();
        if ( process->state() != QProcess::NotRunning )
            process->kill();
        process->setParent(nullptr);
        process->deleteLater();
    }

    for ( QTcpSocket *socket : findChildren<QTcpSocket *>() )
    {
        socket->disconnect(this);
        socket->disconnect();
        socket->abort();
        socket->setParent(nullptr);
        socket->deleteLater();
    }

    delete ui;
}

void WaveshareWidget::reloadSettings()
{
    FCT_IDENTIFICATION;

    actions = WaveshareControl::loadActions();
    rebuild();
    refreshAll();

    if ( rows.isEmpty() )
        refreshTimer->stop();
    else if ( !refreshTimer->isActive() )
        refreshTimer->start();
}

QString WaveshareWidget::actionLabel(const WaveshareAction &action) const
{
    if ( !action.label.isEmpty() )
        return action.label;

    if ( action.type == WaveshareAction::Ping )
        return action.host;

    return tr("Relay %1").arg(action.relay);
}

void WaveshareWidget::rebuild()
{
    while ( QLayoutItem *item = ui->actionsLayout->takeAt(0) )
    {
        if ( item->widget() )
            item->widget()->deleteLater();
        delete item;
    }

    rows.clear();

    for ( const WaveshareAction &action : actions )
    {
        if ( !action.enabled )
            continue;

        Row row;
        row.action = action;

        QWidget *line = new QWidget(ui->contentWidget);
        QHBoxLayout *lineLayout = new QHBoxLayout(line);
        lineLayout->setContentsMargins(0, 0, 0, 0);
        lineLayout->setSpacing(6);

        QLabel *status = new QLabel(tr("..."), line);
        status->setAlignment(Qt::AlignCenter);
        status->setMinimumWidth(44);
        status->setAutoFillBackground(true);
        row.status = status;

        if ( action.type == WaveshareAction::Relay )
        {
            QPushButton *button = new QPushButton(actionLabel(action), line);
            button->setMinimumHeight(30);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            const int rowIndex = rows.size();
            connect(button, &QPushButton::clicked, this, [this, rowIndex]()
            {
                runRelayAction(rowIndex);
            });
            lineLayout->addWidget(button);
        }
        else
        {
            QLabel *label = new QLabel(actionLabel(action), line);
            label->setMinimumHeight(30);
            label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            lineLayout->addWidget(label);
        }

        lineLayout->addWidget(status);
        ui->actionsLayout->addWidget(line);
        rows << row;
    }

    if ( rows.isEmpty() )
    {
        QLabel *empty = new QLabel(tr("No Waveshare actions are configured."), ui->contentWidget);
        empty->setAlignment(Qt::AlignCenter);
        ui->actionsLayout->addWidget(empty);
    }

    ui->actionsLayout->addStretch();
}

void WaveshareWidget::refreshAll()
{
    if ( shuttingDown )
        return;

    for ( int i = 0; i < rows.size(); ++i )
    {
        if ( rows[i].action.type == WaveshareAction::Ping )
            refreshPing(i);
        else
            refreshRelay(i);
    }
}

void WaveshareWidget::setStatus(Row &row, bool ok, const QString &text)
{
    if ( shuttingDown || !row.status )
        return;

    row.status->setText(text);
    row.status->setToolTip(text);
    row.status->setStyleSheet(QStringLiteral("QLabel { color: white; border-radius: 4px; padding: 4px; background: %1; }")
                              .arg(ok ? QStringLiteral("#1f8f3a") : QStringLiteral("#b3261e")));
}

void WaveshareWidget::refreshRelay(int rowIndex)
{
    if ( shuttingDown )
        return;

    if ( rowIndex < 0 || rowIndex >= rows.size() )
        return;

    readRelay(rows[rowIndex].action, [this, rowIndex](bool ok, bool on)
    {
        if ( shuttingDown )
            return;

        if ( rowIndex < 0 || rowIndex >= rows.size() )
            return;

        setStatus(rows[rowIndex], ok && on, ok ? (on ? tr("ON") : tr("OFF")) : tr("ERR"));
    });
}

void WaveshareWidget::refreshPing(int rowIndex)
{
    if ( shuttingDown )
        return;

    if ( rowIndex < 0 || rowIndex >= rows.size() )
        return;

    QProcess *process = new QProcess(this);
    QTimer *timeout = new QTimer(process);
    timeout->setSingleShot(true);
    timeout->setInterval(3000);

    connect(timeout, &QTimer::timeout, process, [process]()
    {
        process->kill();
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process, rowIndex](int exitCode, QProcess::ExitStatus exitStatus)
    {
        if ( shuttingDown )
            return;

        if ( rowIndex >= 0 && rowIndex < rows.size() )
            setStatus(rows[rowIndex], exitStatus == QProcess::NormalExit && exitCode == 0,
                      exitStatus == QProcess::NormalExit && exitCode == 0 ? tr("UP") : tr("DOWN"));
        process->deleteLater();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process, rowIndex](QProcess::ProcessError)
    {
        if ( shuttingDown )
            return;

        if ( rowIndex >= 0 && rowIndex < rows.size() )
            setStatus(rows[rowIndex], false, tr("ERR"));
        process->deleteLater();
    });

    QStringList args;
#ifdef Q_OS_WIN
    args << QStringLiteral("-n") << QStringLiteral("1") << QStringLiteral("-w") << QStringLiteral("2000");
#elif defined(Q_OS_MACOS)
    args << QStringLiteral("-c") << QStringLiteral("1") << QStringLiteral("-W") << QStringLiteral("2000");
#else
    args << QStringLiteral("-c") << QStringLiteral("1") << QStringLiteral("-W") << QStringLiteral("2");
#endif
    args << rows[rowIndex].action.host;
    timeout->start();
    process->start(QStringLiteral("ping"), args);
}

void WaveshareWidget::runRelayAction(int rowIndex)
{
    if ( shuttingDown )
        return;

    if ( rowIndex < 0 || rowIndex >= rows.size() )
        return;

    const WaveshareAction action = rows[rowIndex].action;
    switch ( action.relayMode )
    {
    case WaveshareAction::On:
        writeRelay(action, true);
        break;
    case WaveshareAction::Off:
        writeRelay(action, false);
        break;
    case WaveshareAction::Flash:
        flashRelay(action);
        break;
    case WaveshareAction::Toggle:
    default:
        readRelay(action, [this, action](bool ok, bool on)
        {
            if ( shuttingDown )
                return;

            if ( ok )
                writeRelay(action, !on);
        });
        break;
    }

    QTimer::singleShot(qMax(250, action.pulseMs + 150), this, &WaveshareWidget::refreshAll);
}

void WaveshareWidget::writeRelay(const WaveshareAction &action, bool on)
{
    const quint16 relayAddress = qBound(1, action.relay, 255) - 1;
    QByteArray pdu;
    pdu.append(char(0x05));
    pdu.append(char((relayAddress >> 8) & 0xff));
    pdu.append(char(relayAddress & 0xff));
    pdu.append(char(on ? 0xff : 0x00));
    pdu.append(char(0x00));

    sendRtuRequest(action, pdu, [](bool, const QByteArray &)
    {
    });
}

void WaveshareWidget::flashRelay(const WaveshareAction &action)
{
    const quint16 flashAddress = quint16(0x0200 + qBound(1, action.relay, 255) - 1);
    const quint16 ticks = quint16(qMax(1, qRound(action.pulseMs / 100.0)));
    QByteArray pdu;
    pdu.append(char(0x05));
    pdu.append(char((flashAddress >> 8) & 0xff));
    pdu.append(char(flashAddress & 0xff));
    pdu.append(char((ticks >> 8) & 0xff));
    pdu.append(char(ticks & 0xff));

    sendRtuRequest(action, pdu, [](bool, const QByteArray &)
    {
    });
}

void WaveshareWidget::readRelay(const WaveshareAction &action, std::function<void(bool, bool)> callback)
{
    const quint16 relayAddress = qBound(1, action.relay, 255) - 1;
    const quint16 coilCount = qMax(action.relay, 8);
    QByteArray pdu;
    pdu.append(char(0x01));
    pdu.append(char(0x00));
    pdu.append(char(0x00));
    pdu.append(char((coilCount >> 8) & 0xff));
    pdu.append(char(coilCount & 0xff));

    sendRtuRequest(action, pdu, [callback, relayAddress](bool ok, const QByteArray &response)
    {
        if ( !ok || response.size() < 5 || quint8(response.at(1)) != 0x01 )
        {
            callback(false, false);
            return;
        }

        const int byteIndex = 3 + relayAddress / 8;
        if ( byteIndex >= response.size() )
        {
            callback(false, false);
            return;
        }

        const bool on = quint8(response.at(byteIndex)) & (1 << (relayAddress % 8));
        callback(true, on);
    });
}

void WaveshareWidget::sendRtuRequest(const WaveshareAction &action,
                                     const QByteArray &pdu,
                                     std::function<void(bool, const QByteArray &)> callback)
{
    if ( shuttingDown )
        return;

    rtuQueue.enqueue(PendingRtuRequest{action, pdu, callback});
    processNextRtuRequest();
}

void WaveshareWidget::processNextRtuRequest()
{
    if ( shuttingDown || rtuRequestActive || rtuQueue.isEmpty() )
        return;

    rtuRequestActive = true;
    QTcpSocket *socket = new QTcpSocket(this);
    QByteArray buffer;
    const PendingRtuRequest pending = rtuQueue.dequeue();
    const QByteArray request = rtuFrame(1, pending.pdu);
    QTimer *timeout = new QTimer(socket);
    timeout->setSingleShot(true);
    timeout->setInterval(3000);
    const QSharedPointer<bool> completed(new bool(false));

    const auto finish = [this, socket, timeout, completed, callback = pending.callback](bool ok, const QByteArray &response)
    {
        if ( *completed )
            return;

        *completed = true;
        timeout->stop();
        if ( !shuttingDown )
            callback(ok, response);
        socket->disconnectFromHost();
        socket->deleteLater();
        rtuRequestActive = false;
        if ( !shuttingDown )
            QTimer::singleShot(75, this, &WaveshareWidget::processNextRtuRequest);
    };

    connect(timeout, &QTimer::timeout, this, [finish]()
    {
        finish(false, QByteArray());
    });

    connect(socket, &QTcpSocket::connected, this, [socket, request]()
    {
        QTimer::singleShot(100, socket, [socket, request]()
        {
            socket->write(request);
            socket->flush();
        });
    });

    connect(socket, &QTcpSocket::readyRead, this, [socket, finish, buffer]() mutable
    {
        buffer.append(socket->readAll());
        if ( buffer.size() < 5 )
            return;

        if ( quint8(buffer.at(1)) == 0x01 )
        {
            const int expectedSize = 3 + quint8(buffer.at(2)) + 2;
            if ( buffer.size() < expectedSize )
                return;
            buffer = buffer.left(expectedSize);
        }
        else if ( quint8(buffer.at(1)) == 0x05 )
        {
            if ( buffer.size() < 8 )
                return;
            buffer = buffer.left(8);
        }

        finish(hasValidCrc(buffer), buffer);
    });

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(socket, &QTcpSocket::errorOccurred, this, [finish](QAbstractSocket::SocketError)
#else
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error), this, [finish](QAbstractSocket::SocketError)
#endif
    {
        finish(false, QByteArray());
    });

    timeout->start();
    socket->connectToHost(pending.action.host, pending.action.port);
}
