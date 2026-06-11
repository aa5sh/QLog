#include "WaveshareControl.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "core/LogParam.h"

QString WaveshareControl::typeToString(WaveshareAction::Type type)
{
    return type == WaveshareAction::Ping ? QStringLiteral("ping") : QStringLiteral("relay");
}

WaveshareAction::Type WaveshareControl::typeFromString(const QString &value)
{
    return value.compare(QStringLiteral("ping"), Qt::CaseInsensitive) == 0
           ? WaveshareAction::Ping
           : WaveshareAction::Relay;
}

QString WaveshareControl::relayModeToString(WaveshareAction::RelayMode mode)
{
    switch (mode)
    {
    case WaveshareAction::On:
        return QStringLiteral("on");
    case WaveshareAction::Off:
        return QStringLiteral("off");
    case WaveshareAction::Flash:
        return QStringLiteral("flash");
    case WaveshareAction::Toggle:
    default:
        return QStringLiteral("toggle");
    }
}

WaveshareAction::RelayMode WaveshareControl::relayModeFromString(const QString &value)
{
    if ( value.compare(QStringLiteral("on"), Qt::CaseInsensitive) == 0 )
        return WaveshareAction::On;
    if ( value.compare(QStringLiteral("off"), Qt::CaseInsensitive) == 0 )
        return WaveshareAction::Off;
    if ( value.compare(QStringLiteral("flash"), Qt::CaseInsensitive) == 0 )
        return WaveshareAction::Flash;

    return WaveshareAction::Toggle;
}

QList<WaveshareAction> WaveshareControl::loadActions()
{
    QList<WaveshareAction> actions;
    const QJsonDocument doc = QJsonDocument::fromJson(LogParam::getWaveshareActions().toUtf8());

    if ( !doc.isArray() )
        return actions;

    const QJsonArray array = doc.array();
    for ( const QJsonValue &value : array )
    {
        if ( !value.isObject() )
            continue;

        const QJsonObject obj = value.toObject();
        WaveshareAction action;
        action.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
        action.type = typeFromString(obj.value(QStringLiteral("type")).toString());
        action.label = obj.value(QStringLiteral("label")).toString();
        action.host = obj.value(QStringLiteral("host")).toString();
        action.port = obj.value(QStringLiteral("port")).toInt(4196);
        action.relay = obj.value(QStringLiteral("relay")).toInt(1);
        action.relayMode = relayModeFromString(obj.value(QStringLiteral("relayMode")).toString());
        action.pulseMs = obj.value(QStringLiteral("pulseMs")).toInt(500);

        if ( !action.host.isEmpty() )
            actions << action;
    }

    return actions;
}

void WaveshareControl::saveActions(const QList<WaveshareAction> &actions)
{
    QJsonArray array;

    for ( const WaveshareAction &action : actions )
    {
        QJsonObject obj;
        obj.insert(QStringLiteral("enabled"), action.enabled);
        obj.insert(QStringLiteral("type"), typeToString(action.type));
        obj.insert(QStringLiteral("label"), action.label);
        obj.insert(QStringLiteral("host"), action.host);
        obj.insert(QStringLiteral("port"), action.port);
        obj.insert(QStringLiteral("relay"), action.relay);
        obj.insert(QStringLiteral("relayMode"), relayModeToString(action.relayMode));
        obj.insert(QStringLiteral("pulseMs"), action.pulseMs);
        array.append(obj);
    }

    LogParam::setWaveshareActions(QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)));
}
