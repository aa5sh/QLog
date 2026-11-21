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
    // The rig thread has finished the last frequencyRequested().
    void frequencyRequestDone();

signals:
    void frequencyRequested(double frequencyHz);
    void pttRequested(bool active);

private slots:
    void openDevice();
    void processReport(const QByteArray &data);
    void deviceRemoved();
    void sendTuning();

private:
    explicit Rc28Controller(QObject *parent = nullptr);
    ~Rc28Controller() override;

    void closeDevice();
    void releasePtt();
    void updateLeds();

    // Looking for a newly plugged RC-28 is a cheap enumeration, run only while
    // enabled and disconnected.
    static const int SCAN_INTERVAL_MS = 3000;
    // Only one frequency request is in flight, so requests never queue up
    // behind a slow rig (Hamlib sleeps 200 ms after each set); encoder steps
    // are summed meanwhile. Drivers that return at once are also limited to
    // one request per this interval.
    static const int MIN_TUNE_INTERVAL_MS = 50;
    // A TRANSMIT press in the first report, this soon after opening, was held
    // while the device connected; it must be released before it can key the rig.
    static const int HELD_AT_OPEN_MS = 250;

    Rc28Device *device;
    QTimer *scanTimer;
    QTimer *tuneTimer;
    QElapsedTimer openedTimer;
    bool enabled;
    bool rigConnected;
    bool rigTransmitting;
    QString rigMode;
    qint64 rigFrequencyHz;
    qint64 commandedFrequencyHz;
    int pendingSteps;
    bool frequencyRequestInFlight;
    bool txArmed;
    bool pttKeyed;
    bool f1Pressed;
    bool fastTuning;
    quint8 litLeds;
};

#endif // QLOG_RIG_RC28CONTROLLER_H
