#ifndef QLOG_UI_DXCCSUBMISSIONDIALOG_H
#define QLOG_UI_DXCCSUBMISSIONDIALOG_H

#include <QDialog>
#include <QSqlQueryModel>
#include <QCheckBox>
#include <QList>
#include <QSqlRecord>
#include "models/SqlListModel.h"
#include "data/Band.h"

namespace Ui {
class DXCCSubmissionDialog;
}

// DXCC band scope presets
enum class DXCCBandScope {
    EntityLevel,   // Any band — one entry per entity (basic DXCC)
    FiveBand,      // 80/40/20/15/10m preset
    AllDXCCBands,  // All enabled DXCC bands, per band
    Custom         // User-selected bands
};

class DXCCSubmissionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DXCCSubmissionDialog(QWidget *parent = nullptr);
    ~DXCCSubmissionDialog();

public slots:
    void refreshTable();
    void onBandScopeChanged(int index);
    void onFiveBandClicked();
    void onAllBandsClicked();
    void exportAsADIF();

private:
    Ui::DXCCSubmissionDialog *ui;
    QSqlQueryModel *tableModel;
    SqlListModel   *entityCallsignModel;

    // Band checkboxes added dynamically
    QList<QCheckBox*> bandCheckBoxes;
    QList<Band>       dxccBands;

    // Helpers
    const QString getSelectedEntity() const;
    QStringList   getSelectedBands() const;
    QString       buildModeGroupFilter() const;
    void          selectBandPreset(const QStringList &bands);
    void          updateStatusLabel(int count);
};

#endif // QLOG_UI_DXCCSUBMISSIONDIALOG_H
