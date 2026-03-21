#ifndef QLOG_UI_QSLPRINTLABELDIALOG_H
#define QLOG_UI_QSLPRINTLABELDIALOG_H

#include <QDialog>

#include "core/LogLocale.h"
#include "core/QslPrintLabelRenderer.h"

class QSqlQuery;

namespace Ui {
class QslPrintLabelDialog;
}

class QslPrintLabelDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QslPrintLabelDialog(QWidget *parent = nullptr);
    ~QslPrintLabelDialog();

private slots:
    void toggleDateRange();
    void toggleMyCallsign();
    void toggleStationProfile();
    void toggleQslSent();
    void toggleUserFilter();
    void refreshData();
    void updatePreview();
    void prevPage();
    void nextPage();
    void print();
    void exportPdf();
    void templateChanged(int index);
    void skipChanged(int value);
    void zoomChanged(int value);
    void customTemplateFieldChanged();

private:
    Ui::QslPrintLabelDialog *ui;
    LogLocale locale;
    QslPrintLabelRenderer renderer;
    QList<QslLabelData> labelsData;
    int currentPage = 0;
    int zoomPercent = 100;

    void buildLabels();
    QString buildWhereClause() const;
    void bindWhereClause(QSqlQuery &query) const;
    void askAndMarkQslSent();
    void updatePageNavigation();
    void saveSettings();
    void loadSettings();
    void populateTemplateFields(const LabelTemplate &tmpl);
    void setTemplateFieldsEnabled(bool enabled);
    LabelTemplate buildCustomTemplate() const;
};

#endif // QLOG_UI_QSLPRINTLABELDIALOG_H
