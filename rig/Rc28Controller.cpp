#include "rig/Rc28Controller.h"

#include <QTimer>

#include "rig/Rc28Device.h"
#include "rig/Rc28Protocol.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.rig.rc28controller");

Rc28Controller *Rc28Controller::instance()
{
    FCT_IDENTIFICATION;

    static Rc28Controller controller;
    return &controller;
}

Rc28Controller::Rc28Controller(QObject *parent) :
    QObject(parent),
    device(new Rc28Device(this)),
    scanTimer(new QTimer(this)),
    tuneTimer(new QTimer(this)),
    enabled(false),
    rigConnected(false),
    rigTransmitting(false),
    rigFrequencyHz(0),
    commandedFrequencyHz(0),
    pendingSteps(0),
    frequencyRequestInFlight(false),
    txArmed(false),
    pttKeyed(false),
    f1Pressed(false),
    fastTuning(false),
    litLeds(0)
{
    FCT_IDENTIFICATION;

    scanTimer->setInterval(SCAN_INTERVAL_MS);
    tuneTimer->setInterval(MIN_TUNE_INTERVAL_MS);
    tuneTimer->setSingleShot(true);

    connect(scanTimer, &QTimer::timeout, this, &Rc28Controller::openDevice);
    connect(tuneTimer, &QTimer::timeout, this, &Rc28Controller::sendTuning);
    connect(device, &Rc28Device::reportReceived, this, &Rc28Controller::processReport);
    connect(device, &Rc28Device::removed, this, &Rc28Controller::deviceRemoved);
}

Rc28Controller::~Rc28Controller()
{
    FCT_IDENTIFICATION;

    closeDevice();
}

void Rc28Controller::setEnabled(bool newEnabled)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << newEnabled;

    enabled = newEnabled;

    if ( enabled )
    {
        openDevice();
        return;
    }

    scanTimer->stop();
    closeDevice();
}

void Rc28Controller::updateRigStatus(const Rig::Status &status)
{
    FCT_IDENTIFICATION;

    const qint64 frequencyHz = qRound64(status.freq * 1e6);

    // Reports reach this thread in rig-thread order, so a frequency change
    // reported while no request is in flight is the rig's state after our
    // last request, including its own VFO knob or rounding.
    if ( frequencyHz != rigFrequencyHz && !frequencyRequestInFlight )
        commandedFrequencyHz = 0;

    rigConnected = status.isConnected;
    rigTransmitting = status.ptt > 0;
    rigMode = status.mode;
    rigFrequencyHz = frequencyHz;

    updateLeds();
}

void Rc28Controller::rigDisconnected()
{
    FCT_IDENTIFICATION;

    rigConnected = false;
    rigTransmitting = false;
    rigFrequencyHz = 0;
    commandedFrequencyHz = 0;
    frequencyRequestInFlight = false;
    pendingSteps = 0;
    tuneTimer->stop();
    releasePtt();
    updateLeds();
}

void Rc28Controller::openDevice()
{
    FCT_IDENTIFICATION;

    if ( !enabled || device->isOpen() )
    {
        scanTimer->stop();
        return;
    }

    if ( !device->open() )
    {
        if ( !scanTimer->isActive() )
            scanTimer->start();
        return;
    }

    qCDebug(runtime) << "RC-28 connected";

    scanTimer->stop();
    openedTimer.start();
    txArmed = true;
    f1Pressed = false;
    fastTuning = false;
    // Force the first LED report; the device state is unknown after opening.
    litLeds = 0xFF;
    updateLeds();
}

void Rc28Controller::processReport(const QByteArray &data)
{
    FCT_IDENTIFICATION;

    Rc28Protocol::Report report;

    if ( !Rc28Protocol::parseReport(data, report) )
        return;

    if ( report.f1Pressed && !f1Pressed )
        fastTuning = !fastTuning;
    f1Pressed = report.f1Pressed;

    // Only the first report after opening can show a button held while connecting.
    const bool heldAtOpen = openedTimer.isValid() && openedTimer.elapsed() < HELD_AT_OPEN_MS;
    openedTimer.invalidate();

    if ( !report.txPressed )
    {
        releasePtt();
        txArmed = true;
    }
    else if ( heldAtOpen )
    {
        txArmed = false;
    }
    else if ( txArmed && !pttKeyed && rigConnected && !rigTransmitting )
    {
        // Never key over a transmission started elsewhere; releasing TRANSMIT
        // would otherwise unkey it.
        qCDebug(runtime) << "RC-28 PTT on";
        pttKeyed = true;
        emit pttRequested(true);
    }

    if ( report.steps != 0 && rigConnected && !pttKeyed && !rigTransmitting )
    {
        pendingSteps += report.steps;
        sendTuning();
    }

    updateLeds();
}

void Rc28Controller::deviceRemoved()
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "RC-28 disconnected";

    pendingSteps = 0;
    tuneTimer->stop();
    releasePtt();

    if ( enabled )
        scanTimer->start();
}

void Rc28Controller::frequencyRequestDone()
{
    FCT_IDENTIFICATION;

    frequencyRequestInFlight = false;
    sendTuning();
}

void Rc28Controller::sendTuning()
{
    FCT_IDENTIFICATION;

    if ( pendingSteps == 0 || frequencyRequestInFlight || tuneTimer->isActive() )
        return;

    const int steps = pendingSteps;
    pendingSteps = 0;

    if ( !rigConnected || pttKeyed || rigTransmitting )
        return;

    // Continue from the last request until the rig reports a newer frequency;
    // its polled report lags behind the requests.
    const qint64 baseHz = commandedFrequencyHz > 0 ? commandedFrequencyHz : rigFrequencyHz;
    const qint64 targetHz = Rc28Protocol::tunedFrequencyHz(baseHz, steps,
                                                           Rc28Protocol::tuningStepHz(rigMode, fastTuning));
    if ( targetHz == baseHz )
        return;

    commandedFrequencyHz = targetHz;
    frequencyRequestInFlight = true;
    tuneTimer->start();
    emit frequencyRequested(static_cast<double>(targetHz));
}

void Rc28Controller::closeDevice()
{
    FCT_IDENTIFICATION;

    pendingSteps = 0;
    tuneTimer->stop();
    releasePtt();

    if ( !device->isOpen() )
        return;

    device->write(Rc28Protocol::ledReport(0));
    device->close();
    qCDebug(runtime) << "RC-28 released";
}

void Rc28Controller::releasePtt()
{
    FCT_IDENTIFICATION;

    if ( !pttKeyed )
        return;

    qCDebug(runtime) << "RC-28 PTT off";

    pttKeyed = false;
    // A still-held button must be released before it can key again.
    txArmed = false;
    emit pttRequested(false);
}

void Rc28Controller::updateLeds()
{
    FCT_IDENTIFICATION;

    if ( !device->isOpen() )
        return;

    quint8 leds = 0;

    if ( rigConnected )
        leds |= Rc28Protocol::LED_LINK;
    if ( pttKeyed || rigTransmitting )
        leds |= Rc28Protocol::LED_TX;
    if ( fastTuning )
        leds |= Rc28Protocol::LED_F1;

    if ( leds == litLeds )
        return;

    litLeds = leds;
    device->write(Rc28Protocol::ledReport(leds));
}
