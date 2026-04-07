#ifndef QLOG_UI_QSLLABELDIALOG_H
#define QLOG_UI_QSLLABELDIALOG_H

#include <QDialog>
#include <QSqlRecord>
#include <QList>
#include <QPrinter>
#include <QPageSize>

namespace Ui {
class QSLLabelDialog;
}

class QSLLabelDialog : public QDialog
{
    Q_OBJECT

public:
    // Open for a specific list of selected contacts
    explicit QSLLabelDialog(const QList<QSqlRecord> &contacts, QWidget *parent = nullptr);
    ~QSLLabelDialog();

    // Fetch all contacts with qsl_sent = 'Q' from DB
    static QList<QSqlRecord> fetchQueuedContacts();

private slots:
    void formatTypeChanged();
    void presetChanged(int index);
    void updatePreview();
    void printLabels();
    void savePdf();
    void detectLabelSize();

private:
    // Dimensions are in millimeters
    struct SheetLayout {
        double labelWidth;
        double labelHeight;
        int    labelsPerRow;
        int    labelsPerCol;
        double topMarginMm;
        double leftMarginMm;
        double hSpacingMm;
        double vSpacingMm;
    };

    struct LabelPreset {
        QString               name;
        SheetLayout           layout;
        QPageSize::PageSizeId paperSize;
    };

    // One group = all QSOs with the same DX callsign (first-appearance order)
    struct CallsignGroup {
        QString           callsign;
        QList<QSqlRecord> contacts;
    };

    // Table column type identifier (used inside renderLabel)
    enum ColType { ColDateTime, ColBandMode, ColRST, ColFreq };

    void              loadSettings();
    void              saveSettings();
    void              populatePresets();
    void              populatePrinterCombo();
    SheetLayout       currentSheetLayout() const;
    QList<CallsignGroup> buildGroups() const;
    void              renderLabel(QPainter *painter, const QRectF &rectMm,
                                  const QList<QSqlRecord> &contacts);
    void              printSheet(QPrinter *printer);
    void              printSingle(QPrinter *printer);
    void              promptAndMarkSent();
    void              markContactsAsSent();
    QString           myCallsign(const QSqlRecord &contact) const;
    QString           myGrid(const QSqlRecord &contact) const;

    Ui::QSLLabelDialog     *ui;
    QList<QSqlRecord>       m_contacts;
    QPrinter                m_printer;
    QList<LabelPreset>      m_presets;
};

#endif // QLOG_UI_QSLLABELDIALOG_H
