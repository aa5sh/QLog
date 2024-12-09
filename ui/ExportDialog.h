#ifndef QLOG_UI_EXPORTDIALOG_H
#define QLOG_UI_EXPORTDIALOG_H

#include <QDialog>
#include <QSqlRecord>
#include <QList>
#include <QSet>
#include <QSettings>

#include "core/LogLocale.h"
#include "models/LogbookModel.h"
#include "logformat/LogFormat.h"

namespace Ui {
class ExportDialog;
}

class ExportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportDialog(QWidget *parent = nullptr);
    explicit ExportDialog(const QList<QSqlRecord>&, QWidget *parent = nullptr);
    ~ExportDialog();

public slots:
    void browse();
    void toggleDateRange();
    void toggleMyCallsign();
    void toggleMyGridsquare();
    void toggleQslSendVia();
    void toggleSentStatus();
    void toggleUserFilter();
    void runExport();
    void myCallsignChanged(const QString &myCallsign);
    void showColumnsSetting();
    void exportedColumnStateChanged(int index, bool state);
    void exportTypeChanged(int index);
    void exportedColumnsComboChanged(int);

private:
    Ui::ExportDialog *ui;
    LogLocale locale;
    QSet<int> exportedColumns;
    const QSet<int> minColumns{
        LogbookModel::COLUMN_TIME_ON,
        LogbookModel::COLUMN_CALL,
        LogbookModel::COLUMN_FREQUENCY,
        LogbookModel::COLUMN_MODE,
        LogbookModel::COLUMN_SUBMODE
    };
    const QSet<int> qslColumns{
        LogbookModel::COLUMN_TIME_ON,
        LogbookModel::COLUMN_CALL,
        LogbookModel::COLUMN_FREQUENCY,
        LogbookModel::COLUMN_MODE,
        LogbookModel::COLUMN_SUBMODE,
        LogbookModel::COLUMN_RST_SENT,
        LogbookModel::COLUMN_RST_RCVD
    };
    LogbookModel logbookmodel;
    QSettings settings;
    const QList<QSqlRecord> qsos4export;

    void setProgress(float);
    void fillQSLSendViaCombo();
    void fillExportTypeCombo();
    void fillExportedColumnsCombo();
    bool markQSOAsSent(LogFormat *format);
};

#endif // QLOG_UI_EXPORTDIALOG_H
