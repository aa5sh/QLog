#ifndef QLOG_RIG_RC28CONTROLLER_H
#define QLOG_RIG_RC28CONTROLLER_H

#include <QObject>
#include <QElapsedTimer>
#include <QString>

#include "rig/Rig.h"

class QTimer;
class Rc28Device;

// Icom RC-28 remote encoder: the knob tunes the rig and TRANSMIT keys PTT.
// Lives in the GUI thread. Rig requests leave through signals that MainWindow
// connects to Rig's thread-safe slots.
class Rc28Controller : public QObject
{
    Q_OBJECT

public:
    static Rc28Controller *instance();

public slots:
    // While enabled, an RC-28 is used if present and looked for while absent.
    // Disabling releases PTT and the device.
    void setEnabled(bool enabled);
    void updateRigStatus(const Rig::Status &status);
    void rigDisconnected();

signals:
    void frequencyRequested(double frequencyHz);
    void pttRequested(bool active);

private slots:
    void openDevice();
    void processReport(const QByteArray &data);
    void deviceRemoved();
    void flushTuning();

private:
    explicit Rc28Controller(QObject *parent = nullptr);
    ~Rc28Controller() override;

    void closeDevice();
    void releasePtt();
    void updateLeds();

    // Looking for a newly plugged RC-28 is a cheap enumeration, run only while
    // enabled and disconnected.
    static const int SCAN_INTERVAL_MS = 3000;
    // Encoder reports are summed and sent to the rig at most this often, so a
    // fast spin cannot queue more CAT commands than a slow rig can process.
    static const int TUNE_INTERVAL_MS = 100;
    // A TRANSMIT press in the first report, this soon after opening, was held
    // while the device connected; it must be released before it can key the rig.
    static const int HELD_AT_OPEN_MS = 250;
    // How long a requested frequency is the tuning base before the rig reports
    // it, so consecutive flushes do not restart from a stale rig report.
    static const int COMMAND_HOLD_MS = 1000;

    Rc28Device *device;
    QTimer *scanTimer;
    QTimer *tuneTimer;
    QElapsedTimer openedTimer;
    QElapsedTimer commandTimer;
    bool enabled;
    bool rigConnected;
    bool rigTransmitting;
    QString rigMode;
    qint64 rigFrequencyHz;
    qint64 commandedFrequencyHz;
    int pendingSteps;
    bool txArmed;
    bool pttKeyed;
    bool f1Pressed;
    bool fastTuning;
    quint8 litLeds;
};

#endif // QLOG_RIG_RC28CONTROLLER_H
