#include "ui/SteppirWidget.h"

#include <QButtonGroup>
#include <QRadioButton>
#include <QSignalBlocker>

#include "core/debug.h"
#include "ui_SteppirWidget.h"

MODULE_IDENTIFICATION("qlog.ui.steppirwidget");

SteppirWidget::SteppirWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SteppirWidget)
{
    ui->setupUi(this);

    QButtonGroup *directionButtons = new QButtonGroup(this);
    directionButtons->addButton(ui->normalButton);
    directionButtons->addButton(ui->oneEightyButton);
    directionButtons->addButton(ui->biDirButton);
    directionButtons->addButton(ui->threeQuarterButton);

    connect(ui->profileCombo, &QComboBox::currentTextChanged, this, &SteppirWidget::profileComboChanged);
    connect(ui->normalButton, &QRadioButton::clicked, this, &SteppirWidget::directionClicked);
    connect(ui->oneEightyButton, &QRadioButton::clicked, this, &SteppirWidget::directionClicked);
    connect(ui->biDirButton, &QRadioButton::clicked, this, &SteppirWidget::directionClicked);
    connect(ui->threeQuarterButton, &QRadioButton::clicked, this, &SteppirWidget::directionClicked);
    connect(ui->autotrackButton, &QPushButton::clicked, SteppirController::instance(), &SteppirController::setAutotrack);
    connect(ui->calibrateButton, &QPushButton::clicked, SteppirController::instance(), &SteppirController::calibrate);
    connect(ui->homeButton, &QPushButton::clicked, SteppirController::instance(), &SteppirController::home);

    connect(SteppirController::instance(), &SteppirController::connected, this, &SteppirWidget::connected);
    connect(SteppirController::instance(), &SteppirController::disconnected, this, &SteppirWidget::disconnected);
    connect(SteppirController::instance(), &SteppirController::stateChanged, this, &SteppirWidget::updateState);

    refreshProfileCombo();
    updateState();
}

SteppirWidget::~SteppirWidget()
{
    delete ui;
}

void SteppirWidget::finalizeBeforeAppExit()
{
}

void SteppirWidget::reloadSettings()
{
    refreshProfileCombo();
    SteppirController::instance()->reloadSettings();
}

void SteppirWidget::refreshProfileCombo()
{
    const QSignalBlocker blocker(ui->profileCombo);
    const QString current = SteppirProfiles::currentProfileName();
    ui->profileCombo->clear();
    ui->profileCombo->addItems(SteppirProfiles::profileNames());
    const int index = ui->profileCombo->findText(current);
    if (index >= 0)
        ui->profileCombo->setCurrentIndex(index);
}

void SteppirWidget::connected()
{
    updateState();
}

void SteppirWidget::disconnected()
{
    updateState();
}

void SteppirWidget::updateState()
{
    SteppirController *controller = SteppirController::instance();
    ui->connectedValueLabel->setText(controller->isConnected() ? tr("Connected") : tr("Disconnected"));
    ui->connectedValueLabel->setStyleSheet(controller->isConnected() ? QStringLiteral("color: green") : QStringLiteral("color: red"));
    ui->tuningValueLabel->setText(controller->tuning() ? tr("Tuning") : tr("Idle"));
    ui->tuningValueLabel->setStyleSheet(controller->tuning() ? QStringLiteral("color: red") : QStringLiteral("color: green"));

    const int frequency = controller->tunedFrequencyKHz();
    ui->frequencyValueLabel->setText(frequency > 0 ? QString::number(frequency / 1000.0, 'f', 3) + tr(" MHz") : tr("--"));

    const QSignalBlocker blocker(ui->autotrackButton);
    ui->autotrackButton->setChecked(controller->autotrack());

    QRadioButton *button = directionButton(controller->direction());
    if (button)
    {
        const QSignalBlocker buttonBlocker(button);
        button->setChecked(true);
    }
}

void SteppirWidget::profileComboChanged(const QString &profileName)
{
    if (profileName.isEmpty())
        return;
    SteppirProfiles::setCurrentProfileName(profileName);
    emit profileChanged();
}

void SteppirWidget::directionClicked()
{
    QObject *button = sender();
    if (button == ui->oneEightyButton)
        SteppirController::instance()->setDirection(SteppirController::OneEighty);
    else if (button == ui->biDirButton)
        SteppirController::instance()->setDirection(SteppirController::BiDir);
    else if (button == ui->threeQuarterButton)
        SteppirController::instance()->setDirection(SteppirController::ThreeQuarter);
    else
        SteppirController::instance()->setDirection(SteppirController::Normal);
}

QRadioButton *SteppirWidget::directionButton(SteppirController::Direction direction) const
{
    switch (direction)
    {
    case SteppirController::OneEighty:
        return ui->oneEightyButton;
    case SteppirController::BiDir:
        return ui->biDirButton;
    case SteppirController::ThreeQuarter:
        return ui->threeQuarterButton;
    case SteppirController::Normal:
    default:
        return ui->normalButton;
    }
}
