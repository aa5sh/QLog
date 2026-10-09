#include "ui/TunerWidget.h"
#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

TunerWidget::TunerWidget(QWidget *parent) : QWidget(parent)
{
    setMaximumWidth(560);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(5);
    advancedDialog = new QDialog(this);
    advancedDialog->setObjectName(QStringLiteral("tunerAdvancedDialog"));
    advancedDialog->setWindowTitle(tr("KAT500 Advanced Options"));
    auto *advancedLayout = new QVBoxLayout(advancedDialog);
    auto *header = new QHBoxLayout;
    profiles = new QComboBox(this);
    profiles->setToolTip(tr("Tuner profile"));
    connectButton = new QToolButton(this);
    connectButton->setMaximumWidth(24);
    connectButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    connectionLabel = new QLabel(this);
    header->addWidget(connectButton);
    header->addWidget(profiles, 1);
    header->addWidget(connectionLabel);
    layout->addLayout(header);
    identityLabel = new QLabel(this);
    advancedLayout->addWidget(identityLabel);
    auto *controller = TunerController::instance();
    powerButton = new QPushButton(this);

    connect(powerButton, &QPushButton::clicked, this, [controller]() {
        controller->sendCommand(controller->status().poweredOn ? TunerController::PowerOff : TunerController::PowerOn);
    });
    auto *antennaLayout = new QHBoxLayout;
    antennaLayout->addWidget(new QLabel(tr("Antenna"), this));
    for (int antenna = 1; antenna <= 3; ++antenna)
    {
        auto *button = new QPushButton(QString::number(antenna), this);
        button->setCheckable(true);
        antennaButtons << button;
        antennaLayout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [controller, antenna]() { controller->setAntenna(antenna); });
    }
    layout->addLayout(antennaLayout);
    auto *modeLayout = new QHBoxLayout;
    modeLayout->addWidget(new QLabel(tr("Mode"), this));
    const QStringList modes = {tr("Auto"), tr("Manual"), tr("Bypass")};
    const QList<TunerController::Command> modeCommands = {TunerController::Auto, TunerController::Manual, TunerController::Bypass};
    for (int i = 0; i < modes.size(); ++i)
    {
        auto *button = new QPushButton(modes[i], this);
        button->setCheckable(true);
        modeButtons << button;
        modeLayout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [controller, modeCommands, i]() { controller->sendCommand(modeCommands[i]); });
    }
    layout->addLayout(modeLayout);
    auto *operations = new QGridLayout;
    tuneButton = new QPushButton(tr("Tune"), this);
    tuneButton->setToolTip(tr("Start a full search tune. Supply about 15–30 W of RF to complete tuning."));
    operations->addWidget(powerButton, 0, 0);
    operations->addWidget(tuneButton, 0, 1);
    auto *memories = new QHBoxLayout;
    connect(tuneButton, &QPushButton::clicked, this, [controller]() {
        controller->sendCommand(controller->status().tuning ? TunerController::CancelTune : TunerController::Tune);
    });
    const QStringList operationNames = {tr("Recall Memory"), tr("Save Memory"), tr("Clear Fault")};
    const QList<TunerController::Command> operationCommands = {TunerController::RecallMemory, TunerController::SaveMemory, TunerController::ClearFault};
    for (int i = 0; i < operationNames.size(); ++i)
    {
        auto *button = new QPushButton(operationNames[i], i < 2 ? static_cast<QWidget *>(advancedDialog) : this);
        operationButtons << button;
        if (i < 2) memories->addWidget(button);
        else operations->addWidget(button, 1, 0);
        connect(button, &QPushButton::clicked, this, [controller, operationCommands, i]() { controller->sendCommand(operationCommands[i]); });
    }
    auto *advancedButton = new QPushButton(tr("Adv Options…"), this);
    advancedButton->setObjectName(QStringLiteral("advancedOptionsButton"));
    operations->addWidget(advancedButton, 1, 1);
    connect(advancedButton, &QPushButton::clicked, this, [this]() {
        advancedDialog->show();
        advancedDialog->raise();
        advancedDialog->activateWindow();
    });
    layout->addLayout(operations);
    auto *network = new QGroupBox(tr("L/C Network"), advancedDialog);
    auto *networkLayout = new QFormLayout(network);
    inductors = new QSpinBox(network);
    capacitors = new QSpinBox(network);
    for (auto *spin : {inductors, capacitors})
    {
        spin->setRange(0, 255);
        spin->setKeyboardTracking(false);
        spin->setToolTip(tr("Relay combination, 0–255. Changes are applied when editing finishes."));
    }
    inductanceLabel = new QLabel(network);
    capacitanceLabel = new QLabel(network);
    auto *lRow = new QHBoxLayout;
    lRow->addWidget(inductors);
    lRow->addWidget(inductanceLabel);
    networkLayout->addRow(tr("Inductors"), lRow);
    auto *cRow = new QHBoxLayout;
    cRow->addWidget(capacitors);
    cRow->addWidget(capacitanceLabel);
    networkLayout->addRow(tr("Capacitors"), cRow);
    connect(inductors, &QSpinBox::editingFinished, this, [this, controller]() { controller->setInductors(inductors->value()); });
    connect(capacitors, &QSpinBox::editingFinished, this, [this, controller]() { controller->setCapacitors(capacitors->value()); });
    auto *sideLayout = new QHBoxLayout;
    for (int i = 0; i < 2; ++i)
    {
        auto *button = new QPushButton(i == 0 ? tr("TX Side") : tr("Antenna Side"), network);
        button->setCheckable(true);
        sideButtons << button;
        sideLayout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [controller, i]() {
            controller->sendCommand(i == 0 ? TunerController::TransmitterSide : TunerController::AntennaSide);
        });
    }
    networkLayout->addRow(tr("Capacitor Side"), sideLayout);
    advancedLayout->addWidget(network);
    advancedLayout->addLayout(memories);
    auto *closeButtons = new QDialogButtonBox(QDialogButtonBox::Close, advancedDialog);
    connect(closeButtons, &QDialogButtonBox::rejected, advancedDialog, &QDialog::hide);
    advancedLayout->addWidget(closeButtons);
    auto *readouts = new QFormLayout;
    frequencyLabel = new QLabel(this);
    swrLabel = new QLabel(this);
    bypassSwrLabel = new QLabel(this);
    readouts->addRow(tr("Frequency / Band"), frequencyLabel);
    readouts->addRow(tr("SWR"), swrLabel);
    readouts->addRow(tr("Bypass SWR"), bypassSwrLabel);
    layout->addLayout(readouts);
    stateLabel = new QLabel(this);
    stateLabel->setWordWrap(true);
    faultLabel = new QLabel(this);
    faultLabel->setWordWrap(true);
    faultLabel->setStyleSheet(QStringLiteral("color: #d50000; font-weight: bold"));
    layout->addWidget(stateLabel);
    layout->addWidget(faultLabel);

    refreshProfiles();
    updateStatus(controller->status());
    connect(profiles, &QComboBox::currentTextChanged, this, [this](const QString &name) {
        TunerProfiles::setCurrentProfileName(name);
        emit profileChanged();
    });
    connect(controller, &TunerController::statusChanged, this, &TunerWidget::updateStatus);
    connect(controller, &TunerController::connected, this, [this, controller]() { updateStatus(controller->status()); });
}

void TunerWidget::setConnectAction(QAction *action)
{
    connectButton->setDefaultAction(action);
}

void TunerWidget::refreshProfiles()
{
    const QSignalBlocker blocker(profiles);
    profiles->clear();
    profiles->addItems(TunerProfiles::profileNames());
    profiles->setCurrentIndex(profiles->findText(TunerProfiles::currentProfileName()));
}

void TunerWidget::reloadSettings()
{
    refreshProfiles();
    TunerController::instance()->reloadSettings();
}

void TunerWidget::updateStatus(const TunerStatus &status)
{
    const bool connected = TunerController::instance()->isConnected();
    connectButton->setStyleSheet(connected
        ? QStringLiteral("QToolButton {background-color: green}") : QString());
    const bool active = connected && status.poweredOn;
    connectionLabel->setText(connected ? tr("Connected") : tr("Disconnected"));
    identityLabel->setText(connected ? tr("S/N: %1   Firmware: %2").arg(status.serialNumber, status.firmware) : QString());
    powerButton->setEnabled(connected);
    powerButton->setText(status.poweredOn ? tr("Power Off") : tr("Power On"));
    tuneButton->setEnabled(active);
    tuneButton->setText(status.tuning ? tr("Cancel Tune") : tr("Tune"));
    for (int i = 0; i < antennaButtons.size(); ++i)
    {
        antennaButtons[i]->setEnabled(active && !status.tuning);
        antennaButtons[i]->setChecked(status.antenna == i + 1);
    }
    const QByteArray modes("AMB");
    for (int i = 0; i < modeButtons.size(); ++i)
    {
        modeButtons[i]->setEnabled(active && !status.tuning);
        modeButtons[i]->setChecked(status.mode == modes[i]);
    }
    for (auto *button : operationButtons) button->setEnabled(active && !status.tuning);
    const bool adjust = active && !status.tuning && !status.bypass;
    inductors->setEnabled(adjust && status.inductors >= 0);
    capacitors->setEnabled(adjust && status.capacitors >= 0);
    // Leave a user's in-progress edit alone during status polling.
    if (!inductors->hasFocus()) inductors->setValue(qMax(0, status.inductors));
    if (!capacitors->hasFocus()) capacitors->setValue(qMax(0, status.capacitors));
    static const int lValues[] = {50, 110, 230, 480, 1000, 2100, 4400, 9000};
    static const int cValues[] = {8, 22, 39, 82, 180, 330, 680, 1360};
    int nh = 0, pf = 0;
    for (int i = 0; i < 8; ++i)
    {
        if (status.inductors >= 0 && (status.inductors & (1 << i))) nh += lValues[i];
        if (status.capacitors >= 0 && (status.capacitors & (1 << i))) pf += cValues[i];
    }
    inductanceLabel->setText(status.inductors < 0 ? QStringLiteral("-- µH") : tr("%1 µH").arg(nh / 1000.0, 0, 'f', 2));
    capacitanceLabel->setText(status.capacitors < 0 ? QStringLiteral("-- pF") : tr("%1 pF").arg(pf));
    for (int i = 0; i < sideButtons.size(); ++i)
    {
        sideButtons[i]->setEnabled(adjust && status.capacitorSide != '\0');
        sideButtons[i]->setChecked(status.capacitorSide == (i == 0 ? 'T' : 'A'));
    }
    static const QStringList bands = {"160m", "80m", "60m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m"};
    frequencyLabel->setText(connected ? tr("%1 kHz / %2").arg(status.frequencyKHz).arg(status.band >= 0 && status.band < bands.size() ? bands[status.band] : QStringLiteral("--")) : QStringLiteral("--"));
    swrLabel->setText(status.swr <= 0 ? QStringLiteral("--") : QString::number(status.swr, 'f', 2) + QStringLiteral(":1"));
    bypassSwrLabel->setText(status.bypassSwr <= 0 ? QStringLiteral("--") : QString::number(status.bypassSwr, 'f', 2) + QStringLiteral(":1"));
    QStringList states;
    if (status.tuning) states << tr("Tuning");
    if (status.bypass) states << tr("ATU bypassed");
    if (status.amplifierInterrupted) states << tr("Amplifier interrupted");
    if (status.attenuator) states << tr("Attenuator active");
    stateLabel->setText(states.join(QStringLiteral(" · ")));
    stateLabel->setVisible(!states.isEmpty());
    const QStringList faults = {QString(), tr("No match"), tr("Power above design limit"), tr("Power above safe relay switching limit"), tr("SWR above amplifier interrupt threshold")};
    faultLabel->setVisible(status.fault > 0);
    faultLabel->setText(status.fault > 0 && status.fault < faults.size() ? tr("Fault: %1").arg(faults[status.fault]) : QString());
}
