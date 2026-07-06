#include "WaveshareSettingsWidget.h"
#include "ui_WaveshareSettingsWidget.h"

#include <QComboBox>
#include <QHeaderView>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
#include <QTableWidgetItem>

#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.wavesharesettingswidget");

WaveshareSettingsWidget::WaveshareSettingsWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WaveshareSettingsWidget)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    ui->actionsTable->setColumnCount(8);
    ui->actionsTable->setHorizontalHeaderLabels(QStringList()
                                                << tr("Enabled")
                                                << tr("Type")
                                                << tr("Label")
                                                << tr("Host")
                                                << tr("Port")
                                                << tr("Relay")
                                                << tr("Action")
                                                << tr("Pulse ms"));
    ui->actionsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->actionsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->actionsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    ui->actionsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    ui->actionsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->actionsTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    ui->actionsTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    ui->actionsTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    ui->actionsTable->verticalHeader()->setVisible(false);

    connect(ui->addButton, &QPushButton::clicked, this, &WaveshareSettingsWidget::addAction);
    connect(ui->deleteButton, &QPushButton::clicked, this, &WaveshareSettingsWidget::removeAction);
}

WaveshareSettingsWidget::~WaveshareSettingsWidget()
{
    delete ui;
}

void WaveshareSettingsWidget::loadSettings()
{
    ui->actionsTable->setRowCount(0);

    const QList<WaveshareAction> actions = WaveshareControl::loadActions();
    for ( const WaveshareAction &action : actions )
        appendActionRow(action);
}

void WaveshareSettingsWidget::saveSettings() const
{
    QList<WaveshareAction> actions;

    for ( int row = 0; row < ui->actionsTable->rowCount(); ++row )
    {
        const WaveshareAction action = actionFromRow(row);
        if ( !action.host.isEmpty() )
            actions << action;
    }

    WaveshareControl::saveActions(actions);
}

void WaveshareSettingsWidget::appendActionRow(const WaveshareAction &action)
{
    const int row = ui->actionsTable->rowCount();
    ui->actionsTable->insertRow(row);

    QTableWidgetItem *enabledItem = new QTableWidgetItem;
    enabledItem->setFlags(enabledItem->flags() | Qt::ItemIsUserCheckable);
    enabledItem->setCheckState(action.enabled ? Qt::Checked : Qt::Unchecked);
    ui->actionsTable->setItem(row, 0, enabledItem);

    QComboBox *typeCombo = new QComboBox(ui->actionsTable);
    typeCombo->addItem(tr("Relay"), WaveshareControl::typeToString(WaveshareAction::Relay));
    typeCombo->addItem(tr("Ping"), WaveshareControl::typeToString(WaveshareAction::Ping));
    typeCombo->setCurrentIndex(typeCombo->findData(WaveshareControl::typeToString(action.type)));
    ui->actionsTable->setCellWidget(row, 1, typeCombo);

    ui->actionsTable->setItem(row, 2, new QTableWidgetItem(action.label));
    ui->actionsTable->setItem(row, 3, new QTableWidgetItem(action.host));

    QSpinBox *portSpin = new QSpinBox(ui->actionsTable);
    portSpin->setRange(1, 65535);
    portSpin->setValue(action.port);
    ui->actionsTable->setCellWidget(row, 4, portSpin);

    QSpinBox *relaySpin = new QSpinBox(ui->actionsTable);
    relaySpin->setRange(1, 255);
    relaySpin->setValue(action.relay);
    ui->actionsTable->setCellWidget(row, 5, relaySpin);

    QComboBox *modeCombo = new QComboBox(ui->actionsTable);
    modeCombo->addItem(tr("Toggle"), WaveshareControl::relayModeToString(WaveshareAction::Toggle));
    modeCombo->addItem(tr("On"), WaveshareControl::relayModeToString(WaveshareAction::On));
    modeCombo->addItem(tr("Off"), WaveshareControl::relayModeToString(WaveshareAction::Off));
    modeCombo->addItem(tr("Flash"), WaveshareControl::relayModeToString(WaveshareAction::Flash));
    modeCombo->setCurrentIndex(modeCombo->findData(WaveshareControl::relayModeToString(action.relayMode)));
    ui->actionsTable->setCellWidget(row, 6, modeCombo);

    QSpinBox *pulseSpin = new QSpinBox(ui->actionsTable);
    pulseSpin->setRange(100, 60000);
    pulseSpin->setSingleStep(100);
    pulseSpin->setValue(action.pulseMs);
    ui->actionsTable->setCellWidget(row, 7, pulseSpin);
}

WaveshareAction WaveshareSettingsWidget::actionFromRow(int row) const
{
    WaveshareAction action;

    QTableWidgetItem *enabledItem = ui->actionsTable->item(row, 0);
    action.enabled = !enabledItem || enabledItem->checkState() == Qt::Checked;

    QComboBox *typeCombo = qobject_cast<QComboBox *>(ui->actionsTable->cellWidget(row, 1));
    if ( typeCombo )
        action.type = WaveshareControl::typeFromString(typeCombo->currentData().toString());

    QTableWidgetItem *labelItem = ui->actionsTable->item(row, 2);
    if ( labelItem )
        action.label = labelItem->text().trimmed();

    QTableWidgetItem *hostItem = ui->actionsTable->item(row, 3);
    if ( hostItem )
        action.host = hostItem->text().trimmed();

    QSpinBox *portSpin = qobject_cast<QSpinBox *>(ui->actionsTable->cellWidget(row, 4));
    if ( portSpin )
        action.port = portSpin->value();

    QSpinBox *relaySpin = qobject_cast<QSpinBox *>(ui->actionsTable->cellWidget(row, 5));
    if ( relaySpin )
        action.relay = relaySpin->value();

    QComboBox *modeCombo = qobject_cast<QComboBox *>(ui->actionsTable->cellWidget(row, 6));
    if ( modeCombo )
        action.relayMode = WaveshareControl::relayModeFromString(modeCombo->currentData().toString());

    QSpinBox *pulseSpin = qobject_cast<QSpinBox *>(ui->actionsTable->cellWidget(row, 7));
    if ( pulseSpin )
        action.pulseMs = pulseSpin->value();

    return action;
}

void WaveshareSettingsWidget::addAction()
{
    WaveshareAction action;
    action.label = tr("New Relay");
    appendActionRow(action);
    ui->actionsTable->selectRow(ui->actionsTable->rowCount() - 1);
}

void WaveshareSettingsWidget::removeAction()
{
    const int row = ui->actionsTable->currentRow();
    if ( row >= 0 )
        ui->actionsTable->removeRow(row);
}
