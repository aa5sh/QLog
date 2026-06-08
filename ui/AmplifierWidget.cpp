#include "ui/AmplifierWidget.h"
#include "ui_AmplifierWidget.h"

#include <QSignalBlocker>
#include <QtMath>

#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.amplifierwidget");

namespace
{
    const char *const BAND_NAMES[] =
    {
        "160m", "80m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m"
    };
}

AmplifierWidget::AmplifierWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AmplifierWidget)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    refreshProfiles();
    resetStatus();
    updateControlsEnabled(false);

    connect(ui->profileCombo, &QComboBox::currentTextChanged, this, &AmplifierWidget::setProfile);
    connect(AmplifierController::instance(), &AmplifierController::connected, this, &AmplifierWidget::amplifierConnected);
    connect(AmplifierController::instance(), &AmplifierController::disconnected, this, &AmplifierWidget::amplifierDisconnected);
    connect(AmplifierController::instance(), &AmplifierController::statusChanged, this, &AmplifierWidget::statusChanged);

    connect(ui->standbyButton, &QPushButton::clicked, this, []() {
        AmplifierController::instance()->sendCommand(AmplifierController::Operate);
    });
    connect(ui->modeButton, &QPushButton::clicked, this, []() {
        AmplifierController::instance()->sendCommand(AmplifierController::Mode);
    });
    connect(ui->antennaButton, &QPushButton::clicked, this, []() {
        AmplifierController::instance()->sendCommand(AmplifierController::Antenna);
    });
    connect(ui->tuneButton, &QPushButton::clicked, this, []() {
        AmplifierController::instance()->sendCommand(AmplifierController::Tune);
    });
    connect(ui->inputButton, &QPushButton::clicked, this, []() {
        AmplifierController::instance()->sendCommand(AmplifierController::Input);
    });
    connect(ui->leftButton, &QPushButton::clicked, this, []() {
        AmplifierController::instance()->sendCommand(AmplifierController::Left);
    });
    connect(ui->rightButton, &QPushButton::clicked, this, []() {
        AmplifierController::instance()->sendCommand(AmplifierController::Right);
    });
    connect(ui->bandDownButton, &QPushButton::clicked, this, []() {
        AmplifierController::instance()->sendCommand(AmplifierController::BandDown);
    });
    connect(ui->bandUpButton, &QPushButton::clicked, this, []() {
        AmplifierController::instance()->sendCommand(AmplifierController::BandUp);
    });
}

AmplifierWidget::~AmplifierWidget()
{
    delete ui;
}

void AmplifierWidget::reloadSettings()
{
    refreshProfiles();
    AmplifierController::instance()->reloadSettings();
}

void AmplifierWidget::refreshProfiles()
{
    const QSignalBlocker blocker(ui->profileCombo);
    ui->profileCombo->clear();
    ui->profileCombo->addItems(AmplifierProfiles::profileNames());

    const int index = ui->profileCombo->findText(AmplifierProfiles::currentProfileName());
    if (index >= 0)
        ui->profileCombo->setCurrentIndex(index);
}

void AmplifierWidget::setProfile(const QString &profileName)
{
    AmplifierProfiles::setCurrentProfileName(profileName);
    emit profileChanged();
}

void AmplifierWidget::amplifierConnected()
{
    ui->connectedIndicator->setStyleSheet(ledStyle("#00c853"));
    ui->connectedIndicator->setToolTip(tr("Connected"));
    updateControlsEnabled(true);
}

void AmplifierWidget::amplifierDisconnected()
{
    ui->connectedIndicator->setStyleSheet(ledStyle("#d50000"));
    ui->connectedIndicator->setToolTip(tr("Disconnected"));
    updateControlsEnabled(false);
    resetStatus();
}

void AmplifierWidget::statusChanged(const AmplifierStatus &status)
{
    ui->connectedIndicator->setStyleSheet(AmplifierController::instance()->isConnected()
                                          ? ledStyle("#00c853")
                                          : ledStyle("#d50000"));
    ui->tuningIndicator->setStyleSheet(status.tuning ? ledStyle("#ffab00") : ledStyle("#9e9e9e"));
    ui->tuningIndicator->setToolTip(status.tuning ? tr("Tuning") : tr("Idle"));
    ui->txLabel->setText(status.tx ? tr("TX") : tr("RX"));
    ui->txLabel->setStyleSheet(status.tx
                               ? QStringLiteral("background:#d50000; color:white; font-weight:bold; padding:1px 6px;")
                               : QStringLiteral("background:#eeeeee; color:#424242; font-weight:bold; padding:1px 6px;"));
    ui->modeButton->setText(status.fullMode ? tr("FULL") : tr("HALF"));
    ui->standbyButton->setText(status.operate ? tr("STANDBY") : tr("OPERATE"));
    ui->bandLabel->setText(tr("BAND: %1").arg(bandName(status.band)));
    ui->antennaLabel->setText(tr("ANT: %1").arg(status.antenna + 1));
    ui->inputLabel->setText(tr("IN: %1").arg(status.input + 1));
    ui->temperatureLabel->setText(tr("%1° %2").arg(status.temperature).arg(status.tempCelsius ? "C" : "F"));
    ui->swrLabel->setText(status.operate
                          ? tr("%1 dB").arg(status.gainDb, 0, 'f', 1)
                          : tr("%1").arg(status.swr, 0, 'f', 2));
    ui->outputLabel->setText(tr("%1").arg(status.paOutW, 0, 'f', 1));

    ui->powerValueLabel->setText(tr("PWR OUT %1 W").arg(status.paOutW, 0, 'f', 1));
    ui->powerBar->setValue(qRound(status.paOutW * 10.0));

    const double swr = qMax(1.0, status.swr);
    ui->swrValueLabel->setText(status.operate
                               ? tr("GAIN %1 dB").arg(status.gainDb, 0, 'f', 1)
                               : tr("SWR %1").arg(status.swr, 0, 'f', 2));
    ui->swrBar->setValue(qRound((swr - 1.0) * 100.0));

    ui->voltsValueLabel->setText(tr("VOLTS %1").arg(status.voltageV, 0, 'f', 1));
    ui->voltsBar->setValue(qRound(status.voltageV * 10.0));
    ui->ampsValueLabel->setText(tr("AMPS %1").arg(status.currentA, 0, 'f', 1));
    ui->ampsBar->setValue(qRound(status.currentA * 10.0));
}

QString AmplifierWidget::bandName(int band)
{
    if (band >= 0 && band < 10)
        return BAND_NAMES[band];
    return "--";
}

QString AmplifierWidget::ledStyle(const QString &color)
{
    return QStringLiteral("border-radius:6px; min-width:12px; max-width:12px; min-height:12px; max-height:12px; background:%1;")
            .arg(color);
}

void AmplifierWidget::updateControlsEnabled(bool enabled)
{
    const QList<QPushButton *> buttons =
    {
        ui->standbyButton,
        ui->modeButton,
        ui->antennaButton,
        ui->tuneButton,
        ui->inputButton,
        ui->leftButton,
        ui->rightButton,
        ui->bandDownButton,
        ui->bandUpButton
    };

    for (QPushButton *button : buttons)
        button->setEnabled(enabled);
}

void AmplifierWidget::resetStatus()
{
    ui->connectedIndicator->setStyleSheet(ledStyle("#d50000"));
    ui->connectedIndicator->setToolTip(tr("Disconnected"));
    ui->tuningIndicator->setStyleSheet(ledStyle("#9e9e9e"));
    ui->tuningIndicator->setToolTip(tr("Idle"));
    statusChanged(AmplifierStatus());
}
