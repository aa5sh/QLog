#ifndef QLOG_CORE_WAVESHARECONTROL_H
#define QLOG_CORE_WAVESHARECONTROL_H

#include <QList>
#include <QString>

struct WaveshareAction
{
    enum Type
    {
        Relay,
        Ping
    };

    enum RelayMode
    {
        Toggle,
        On,
        Off,
        Flash
    };

    bool enabled = true;
    Type type = Relay;
    QString label;
    QString host;
    int port = 4196;
    int relay = 1;
    RelayMode relayMode = Toggle;
    int pulseMs = 500;
};

class WaveshareControl
{
public:
    static QList<WaveshareAction> loadActions();
    static void saveActions(const QList<WaveshareAction> &actions);

    static QString typeToString(WaveshareAction::Type type);
    static WaveshareAction::Type typeFromString(const QString &value);
    static QString relayModeToString(WaveshareAction::RelayMode mode);
    static WaveshareAction::RelayMode relayModeFromString(const QString &value);
};

#endif // QLOG_CORE_WAVESHARECONTROL_H
