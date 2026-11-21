#include <QtTest>

#include "rig/Rc28Controller.h"
#include "rig/Rc28Protocol.h"
#include "test_stubs.h"

class Rc28Test : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void parseReport();
    void parseReportRejectsOtherReports();
    void ledReportIsActiveLow();
    void tuningStepFollowsMode();
    void tunedFrequencySnapsToGrid();

    void tuningWaitsForRig();

    void pttFollowsTransmitButton();
    void pttReleasedWhenDeviceRemoved();
    void pttReleasedWhenDisabled();
    void pttReleasedWhenRigDisconnects();
    void pttNotKeyedOverOtherTransmission();
    void buttonHeldAtOpenDoesNotKey();
    void reconnectsOnlyWhileEnabled();

private:
    static QByteArray inputReport(int steps, bool txPressed, bool f1Pressed = false);
    static Rig::Status connectedRig();
    static Rc28Device *device();
    static void openDevice();
};

QByteArray Rc28Test::inputReport(int steps, bool txPressed, bool f1Pressed)
{
    QByteArray report(Rc28Protocol::REPORT_SIZE, '\0');
    report[0] = 0x01;
    report[1] = static_cast<char>(qAbs(steps));
    report[3] = static_cast<char>(steps > 0 ? 0x01 : steps < 0 ? 0x02 : 0x00);
    report[5] = static_cast<char>(0x07 & ~( ( txPressed ? 0x01 : 0 ) | ( f1Pressed ? 0x02 : 0 ) ));
    return report;
}

Rig::Status Rc28Test::connectedRig()
{
    Rig::Status status;
    status.profile = QStringLiteral("Test");
    status.isConnected = true;
    status.freq = 14.074;
    status.mode = QStringLiteral("SSB");
    status.ptt = 0;
    return status;
}

Rc28Device *Rc28Test::device()
{
    return Rc28Controller::instance()->findChild<Rc28Device *>();
}

void Rc28Test::openDevice()
{
    Rc28DeviceStub::present = true;
    Rc28Controller::instance()->setEnabled(true);
    QVERIFY(device()->isOpen());
    // The first report after opening decides whether TRANSMIT was held then.
    emit device()->reportReceived(inputReport(0, false));
}

void Rc28Test::init()
{
    Rc28Controller *controller = Rc28Controller::instance();
    controller->setEnabled(false);
    controller->rigDisconnected();
    Rc28DeviceStub::present = false;
    Rc28DeviceStub::writes.clear();
}

void Rc28Test::parseReport()
{
    Rc28Protocol::Report report;

    QVERIFY(Rc28Protocol::parseReport(inputReport(3, false), report));
    QCOMPARE(report.steps, 3);
    QCOMPARE(report.txPressed, false);
    QCOMPARE(report.f1Pressed, false);

    QVERIFY(Rc28Protocol::parseReport(inputReport(-200, true, true), report));
    QCOMPARE(report.steps, -200);
    QCOMPARE(report.txPressed, true);
    QCOMPARE(report.f1Pressed, true);

    QByteArray f2Only = inputReport(0, false);
    f2Only[5] = 0x03;
    QVERIFY(Rc28Protocol::parseReport(f2Only, report));
    QCOMPARE(report.steps, 0);
    QCOMPARE(report.txPressed, false);
    QCOMPARE(report.f1Pressed, false);
}

void Rc28Test::parseReportRejectsOtherReports()
{
    Rc28Protocol::Report report;

    QVERIFY(!Rc28Protocol::parseReport(inputReport(1, false).left(6), report));

    QByteArray firmwareReply = inputReport(1, false);
    firmwareReply[0] = 0x02;
    QVERIFY(!Rc28Protocol::parseReport(firmwareReply, report));

    QByteArray unknownBits = inputReport(1, false);
    unknownBits[5] = static_cast<char>(0xFF);
    QVERIFY(!Rc28Protocol::parseReport(unknownBits, report));
}

void Rc28Test::ledReportIsActiveLow()
{
    const QByteArray allOff = Rc28Protocol::ledReport(0);
    QCOMPARE(allOff.size(), Rc28Protocol::REPORT_SIZE);
    QCOMPARE(static_cast<quint8>(allOff.at(0)), quint8(0x01));
    QCOMPARE(static_cast<quint8>(allOff.at(1)), quint8(0x0F));

    const QByteArray linkAndTx = Rc28Protocol::ledReport(Rc28Protocol::LED_LINK | Rc28Protocol::LED_TX);
    QCOMPARE(static_cast<quint8>(linkAndTx.at(1)), quint8(0x06));
}

void Rc28Test::tuningStepFollowsMode()
{
    QCOMPARE(Rc28Protocol::tuningStepHz(QStringLiteral("SSB"), false), 1);
    QCOMPARE(Rc28Protocol::tuningStepHz(QStringLiteral("CW"), false), 1);
    QCOMPARE(Rc28Protocol::tuningStepHz(QStringLiteral("FT8"), false), 1);
    QCOMPARE(Rc28Protocol::tuningStepHz(QString(), false), 1);
    QCOMPARE(Rc28Protocol::tuningStepHz(QStringLiteral("AM"), false), 10);
    QCOMPARE(Rc28Protocol::tuningStepHz(QStringLiteral("FM"), false), 100);
    QCOMPARE(Rc28Protocol::tuningStepHz(QStringLiteral("CW"), true), 10);
    QCOMPARE(Rc28Protocol::tuningStepHz(QStringLiteral("FM"), true), 1000);
}

void Rc28Test::tunedFrequencySnapsToGrid()
{
    QCOMPARE(Rc28Protocol::tunedFrequencyHz(14074000, 3, 10), qint64(14074030));
    QCOMPARE(Rc28Protocol::tunedFrequencyHz(14074000, -3, 10), qint64(14073970));
    // From off grid, the first count lands on the next grid point.
    QCOMPARE(Rc28Protocol::tunedFrequencyHz(14074012, 1, 10), qint64(14074020));
    QCOMPARE(Rc28Protocol::tunedFrequencyHz(14074012, -1, 10), qint64(14074010));
    QCOMPARE(Rc28Protocol::tunedFrequencyHz(14074012, 2, 100), qint64(14074200));
    // No motion, no known frequency, or a target at or below 0 Hz keep the frequency.
    QCOMPARE(Rc28Protocol::tunedFrequencyHz(14074012, 0, 10), qint64(14074012));
    QCOMPARE(Rc28Protocol::tunedFrequencyHz(0, 5, 10), qint64(0));
    QCOMPARE(Rc28Protocol::tunedFrequencyHz(20, -5, 10), qint64(20));
}

void Rc28Test::tuningWaitsForRig()
{
    Rc28Controller *controller = Rc28Controller::instance();
    controller->updateRigStatus(connectedRig());
    openDevice();
    QSignalSpy tune(controller, &Rc28Controller::frequencyRequested);

    emit device()->reportReceived(inputReport(3, false));
    QCOMPARE(tune.count(), 1);
    QCOMPARE(tune.last().at(0).toDouble(), 14074003.0);

    // Spinning while the rig applies a set queues nothing; steps add up.
    emit device()->reportReceived(inputReport(2, false));
    emit device()->reportReceived(inputReport(4, false));
    QCOMPARE(tune.count(), 1);

    controller->frequencyRequestDone();
    QTRY_COMPARE(tune.count(), 2);
    QCOMPARE(tune.last().at(0).toDouble(), 14074009.0);

    // A report without a new frequency is stale; tuning continues from the request.
    Rig::Status stale = connectedRig();
    stale.mode = QStringLiteral("CW");
    controller->frequencyRequestDone();
    controller->updateRigStatus(stale);
    emit device()->reportReceived(inputReport(1, false));
    QTRY_COMPARE(tune.count(), 3);
    QCOMPARE(tune.last().at(0).toDouble(), 14074010.0);

    // A new frequency after the set completed, e.g. from the radio's own knob, wins.
    controller->frequencyRequestDone();
    Rig::Status moved = connectedRig();
    moved.freq = 7.050;
    controller->updateRigStatus(moved);
    emit device()->reportReceived(inputReport(1, false));
    QTRY_COMPARE(tune.count(), 4);
    QCOMPARE(tune.last().at(0).toDouble(), 7050001.0);
    controller->frequencyRequestDone();
}

void Rc28Test::pttFollowsTransmitButton()
{
    Rc28Controller *controller = Rc28Controller::instance();
    controller->updateRigStatus(connectedRig());
    openDevice();
    QSignalSpy ptt(controller, &Rc28Controller::pttRequested);

    emit device()->reportReceived(inputReport(0, true));
    // Repeated pressed reports, such as knob motion while held, key only once.
    emit device()->reportReceived(inputReport(2, true));
    emit device()->reportReceived(inputReport(0, false));

    QCOMPARE(ptt.count(), 2);
    QCOMPARE(ptt.at(0).at(0).toBool(), true);
    QCOMPARE(ptt.at(1).at(0).toBool(), false);
}

void Rc28Test::pttReleasedWhenDeviceRemoved()
{
    Rc28Controller *controller = Rc28Controller::instance();
    controller->updateRigStatus(connectedRig());
    openDevice();
    QSignalSpy ptt(controller, &Rc28Controller::pttRequested);

    emit device()->reportReceived(inputReport(0, true));
    // Rc28Device closes itself before reporting removal.
    Rc28DeviceStub::present = false;
    device()->close();
    emit device()->removed();

    QCOMPARE(ptt.count(), 2);
    QCOMPARE(ptt.last().at(0).toBool(), false);
}

void Rc28Test::pttReleasedWhenDisabled()
{
    Rc28Controller *controller = Rc28Controller::instance();
    controller->updateRigStatus(connectedRig());
    openDevice();
    QSignalSpy ptt(controller, &Rc28Controller::pttRequested);

    emit device()->reportReceived(inputReport(0, true));
    Rc28DeviceStub::writes.clear();
    controller->setEnabled(false);

    QCOMPARE(ptt.count(), 2);
    QCOMPARE(ptt.last().at(0).toBool(), false);
    QVERIFY(!device()->isOpen());
    // LEDs are turned off before the device is released.
    QCOMPARE(Rc28DeviceStub::writes.size(), 1);
    QCOMPARE(Rc28DeviceStub::writes.last(), Rc28Protocol::ledReport(0));
}

void Rc28Test::pttReleasedWhenRigDisconnects()
{
    Rc28Controller *controller = Rc28Controller::instance();
    controller->updateRigStatus(connectedRig());
    openDevice();
    QSignalSpy ptt(controller, &Rc28Controller::pttRequested);

    emit device()->reportReceived(inputReport(0, true));
    controller->rigDisconnected();
    QCOMPARE(ptt.count(), 2);
    QCOMPARE(ptt.last().at(0).toBool(), false);

    // A button still held through the reconnect must be released first.
    controller->updateRigStatus(connectedRig());
    emit device()->reportReceived(inputReport(0, true));
    QCOMPARE(ptt.count(), 2);
}

void Rc28Test::pttNotKeyedOverOtherTransmission()
{
    Rc28Controller *controller = Rc28Controller::instance();
    Rig::Status transmitting = connectedRig();
    transmitting.ptt = 1;
    controller->updateRigStatus(transmitting);
    openDevice();
    QSignalSpy ptt(controller, &Rc28Controller::pttRequested);

    emit device()->reportReceived(inputReport(0, true));
    emit device()->reportReceived(inputReport(0, false));

    QCOMPARE(ptt.count(), 0);
}

void Rc28Test::buttonHeldAtOpenDoesNotKey()
{
    Rc28Controller *controller = Rc28Controller::instance();
    controller->updateRigStatus(connectedRig());
    Rc28DeviceStub::present = true;
    controller->setEnabled(true);
    QSignalSpy ptt(controller, &Rc28Controller::pttRequested);

    emit device()->reportReceived(inputReport(0, true));
    emit device()->reportReceived(inputReport(1, true));
    QCOMPARE(ptt.count(), 0);

    emit device()->reportReceived(inputReport(0, false));
    emit device()->reportReceived(inputReport(0, true));
    QCOMPARE(ptt.count(), 1);
    QCOMPARE(ptt.last().at(0).toBool(), true);
}

void Rc28Test::reconnectsOnlyWhileEnabled()
{
    Rc28Controller *controller = Rc28Controller::instance();
    controller->setEnabled(true);
    QVERIFY(!device()->isOpen());

    // The periodic check would open it; trigger it directly instead of waiting.
    Rc28DeviceStub::present = true;
    controller->setEnabled(true);
    QVERIFY(device()->isOpen());

    controller->setEnabled(false);
    QVERIFY(!device()->isOpen());
}

QTEST_GUILESS_MAIN(Rc28Test)

#include "tst_rc28.moc"
