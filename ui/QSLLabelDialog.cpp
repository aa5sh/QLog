#include <QPainter>
#include <QPrinter>
#include <QPrinterInfo>
#include <QPixmap>
#include <QSettings>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QHash>
#include <QInputDialog>
#include <QMessageBox>
#include <QFont>
#include <QFontMetricsF>

#include "QSLLabelDialog.h"
#include "ui_QSLLabelDialog.h"
#include "data/StationProfile.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.qsllabeldialog");

// ---------- preset data --------------------------------------------------
// All dimensions in millimetres.  Paper sizes use QPageSize::PageSizeId.

static const double MM_PER_INCH = 25.4;
static const int MAX_QSOS_PER_LABEL = 6;

QSLLabelDialog::QSLLabelDialog(const QList<QSqlRecord> &contacts, QWidget *parent)
    : QDialog(parent),
      ui(new Ui::QSLLabelDialog),
      m_contacts(contacts),
      m_printer(QPrinter::HighResolution)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    populatePresets();
    populatePrinterCombo();
    loadSettings();

    // Contact count label
    ui->contactCountLabel->setText(tr("%n contact(s) to print", "", m_contacts.size()));

    // Format type radio buttons
    connect(ui->sheetRadio,  &QRadioButton::toggled, this, &QSLLabelDialog::formatTypeChanged);
    connect(ui->singleRadio, &QRadioButton::toggled, this, &QSLLabelDialog::formatTypeChanged);

    // Preset combo
    connect(ui->presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QSLLabelDialog::presetChanged);

    // Any dimension / content change → re-render preview
    auto updateSlot = [this]() { updatePreview(); };
    connect(ui->labelWidthSpin,   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, updateSlot);
    connect(ui->labelHeightSpin,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, updateSlot);
    connect(ui->labelsPerRowSpin, QOverload<int>::of(&QSpinBox::valueChanged),          this, updateSlot);
    connect(ui->labelsPerColSpin, QOverload<int>::of(&QSpinBox::valueChanged),          this, updateSlot);
    connect(ui->topMarginSpin,    QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, updateSlot);
    connect(ui->leftMarginSpin,   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, updateSlot);
    connect(ui->hSpacingSpin,     QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, updateSlot);
    connect(ui->vSpacingSpin,     QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, updateSlot);
    connect(ui->singleWidthSpin,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, updateSlot);
    connect(ui->singleHeightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, updateSlot);
    connect(ui->groupQSOsCheck,    &QCheckBox::toggled, this, updateSlot);
    connect(ui->showCallsignCheck, &QCheckBox::toggled, this, updateSlot);
    connect(ui->showDateTimeCheck, &QCheckBox::toggled, this, updateSlot);
    connect(ui->showBandModeCheck, &QCheckBox::toggled, this, updateSlot);
    connect(ui->showRSTCheck,      &QCheckBox::toggled, this, updateSlot);
    connect(ui->showFreqCheck,     &QCheckBox::toggled, this, updateSlot);
    connect(ui->showNameQthCheck,  &QCheckBox::toggled, this, updateSlot);
    connect(ui->showMyCallsignCheck, &QCheckBox::toggled, this, updateSlot);

    // Buttons
    connect(ui->printButton,      &QPushButton::clicked, this, &QSLLabelDialog::printLabels);
    connect(ui->savePdfButton,    &QPushButton::clicked, this, &QSLLabelDialog::savePdf);
    connect(ui->detectSizeButton, &QPushButton::clicked, this, &QSLLabelDialog::detectLabelSize);
    connect(ui->closeButton,      &QPushButton::clicked, this, &QDialog::reject);

    formatTypeChanged();  // sets initial visibility
    updatePreview();
}

QSLLabelDialog::~QSLLabelDialog()
{
    FCT_IDENTIFICATION;
    saveSettings();
    delete ui;
}

// ---------- static helper ------------------------------------------------

QList<QSqlRecord> QSLLabelDialog::fetchQueuedContacts()
{
    FCT_IDENTIFICATION;

    QList<QSqlRecord> list;
    QSqlQuery query;

    if (!query.exec("SELECT * FROM contacts WHERE qsl_sent = 'Q' ORDER BY callsign, start_time"))
    {
        qWarning() << "QSLLabelDialog: failed to fetch queued contacts:" << query.lastError().text();
        return list;
    }

    while (query.next())
        list << query.record();

    return list;
}

// ---------- slots --------------------------------------------------------

void QSLLabelDialog::formatTypeChanged()
{
    FCT_IDENTIFICATION;

    const bool isSheet = ui->sheetRadio->isChecked();
    ui->sheetSettingsBox->setVisible(isSheet);
    ui->singleSettingsBox->setVisible(!isSheet);
    updatePreview();
}

void QSLLabelDialog::presetChanged(int index)
{
    FCT_IDENTIFICATION;

    if (index < 0 || index >= m_presets.size())
        return;

    // Last entry is "Custom" — don't overwrite user-entered values
    if (index == m_presets.size() - 1)
        return;

    const SheetLayout &lay = m_presets.at(index).layout;

    // Block signals while updating spinboxes so we don't retrigger presetChanged
    QSignalBlocker bw(ui->labelWidthSpin);
    QSignalBlocker bh(ui->labelHeightSpin);
    QSignalBlocker br(ui->labelsPerRowSpin);
    QSignalBlocker bc(ui->labelsPerColSpin);
    QSignalBlocker bt(ui->topMarginSpin);
    QSignalBlocker bl(ui->leftMarginSpin);
    QSignalBlocker bhs(ui->hSpacingSpin);
    QSignalBlocker bvs(ui->vSpacingSpin);

    ui->labelWidthSpin->setValue(lay.labelWidth);
    ui->labelHeightSpin->setValue(lay.labelHeight);
    ui->labelsPerRowSpin->setValue(lay.labelsPerRow);
    ui->labelsPerColSpin->setValue(lay.labelsPerCol);
    ui->topMarginSpin->setValue(lay.topMarginMm);
    ui->leftMarginSpin->setValue(lay.leftMarginMm);
    ui->hSpacingSpin->setValue(lay.hSpacingMm);
    ui->vSpacingSpin->setValue(lay.vSpacingMm);

    m_printer.setPageSize(QPageSize(m_presets.at(index).paperSize));

    updatePreview();
}

void QSLLabelDialog::updatePreview()
{
    FCT_IDENTIFICATION;

    if (m_contacts.isEmpty())
    {
        ui->previewLabel->setText(tr("No contacts"));
        return;
    }

    // Determine label dimensions (mm)
    double wMm, hMm;
    if (ui->sheetRadio->isChecked())
    {
        wMm = ui->labelWidthSpin->value();
        hMm = ui->labelHeightSpin->value();
    }
    else
    {
        wMm = ui->singleWidthSpin->value();
        hMm = ui->singleHeightSpin->value();
    }

    // Scale for screen preview: aim for ~420px wide max
    const double screenDPMM = 3.78;  // ~96 DPI
    const double maxPreviewW = 420.0;
    const double scale = qMin(maxPreviewW / wMm, screenDPMM * 3.0);

    const int pxW = qRound(wMm * scale);
    const int pxH = qRound(hMm * scale);

    QPixmap pixmap(pxW, pxH);
    pixmap.fill(Qt::white);

    // Preview the group with the most QSOs (best demonstration of table layout)
    const QList<CallsignGroup> groups = buildGroups();
    const CallsignGroup *previewGroup = &groups.first();
    for (const CallsignGroup &g : groups)
        if (g.contacts.size() > previewGroup->contacts.size())
            previewGroup = &g;

    QPainter painter(&pixmap);
    painter.scale(scale, scale);   // now painter coords are in mm
    renderLabel(&painter, QRectF(0, 0, wMm, hMm), previewGroup->contacts);
    painter.end();

    ui->previewLabel->setPixmap(pixmap);
    ui->previewLabel->setFixedSize(pxW, pxH);
}

void QSLLabelDialog::printLabels()
{
    FCT_IDENTIFICATION;

    if (m_contacts.isEmpty())
    {
        QMessageBox::information(this, tr("Print QSL Labels"), tr("No contacts to print."));
        return;
    }

    const QString chosenName = ui->printerCombo->currentText();
    if (!chosenName.isEmpty())
        m_printer.setPrinterName(chosenName);

    if (ui->sheetRadio->isChecked())
        printSheet(&m_printer);
    else
        printSingle(&m_printer);

    promptAndMarkSent();
    accept();
}

void QSLLabelDialog::savePdf()
{
    FCT_IDENTIFICATION;

    if (m_contacts.isEmpty())
    {
        QMessageBox::information(this, tr("Save PDF"), tr("No contacts to print."));
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this,
        tr("Save QSL Labels as PDF"),
        QDir::homePath(),
        tr("PDF Files (*.pdf)"));

    if (path.isEmpty())
        return;

    QPrinter pdfPrinter(QPrinter::HighResolution);
    pdfPrinter.setOutputFormat(QPrinter::PdfFormat);
    pdfPrinter.setOutputFileName(path);

    if (ui->sheetRadio->isChecked())
    {
        // Match paper size to the current preset
        const int idx = ui->presetCombo->currentIndex();
        if (idx >= 0 && idx < m_presets.size())
            pdfPrinter.setPageSize(QPageSize(m_presets.at(idx).paperSize));
        printSheet(&pdfPrinter);
    }
    else
    {
        // printSingle sets its own page size from the spinboxes
        printSingle(&pdfPrinter);
    }

    QMessageBox::information(this, tr("Save PDF"),
        tr("Labels saved to:\n%1").arg(QDir::toNativeSeparators(path)));

    promptAndMarkSent();
}

void QSLLabelDialog::detectLabelSize()
{
    FCT_IDENTIFICATION;

#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
    const QString printerName = ui->printerCombo->currentText();
    if (printerName.isEmpty())
    {
        QMessageBox::information(this, tr("Detect Label Size"), tr("No printer selected."));
        return;
    }

    const QPrinterInfo info = QPrinterInfo::printerInfo(printerName);
    const QList<QPageSize> sizes = info.supportedPageSizes();

    if (sizes.isEmpty())
    {
        QMessageBox::information(this, tr("Detect Label Size"),
            tr("No page sizes reported by \"%1\".\n"
               "Try setting the label dimensions manually.").arg(printerName));
        return;
    }

    QPageSize chosen;
    if (sizes.size() == 1)
    {
        chosen = sizes.first();
    }
    else
    {
        QStringList names;
        for (const QPageSize &s : sizes)
            names << QString("%1  (%2 \xd7 %3 mm)")
                     .arg(s.name())
                     .arg(s.size(QPageSize::Millimeter).width(),  0, 'f', 1)
                     .arg(s.size(QPageSize::Millimeter).height(), 0, 'f', 1);

        bool ok = false;
        const QString sel = QInputDialog::getItem(
            this,
            tr("Select Label Size"),
            tr("Printer \"%1\" supports multiple sizes:").arg(printerName),
            names, 0, false, &ok);

        if (!ok)
            return;

        const int idx = names.indexOf(sel);
        if (idx >= 0 && idx < sizes.size())
            chosen = sizes.at(idx);
        else
            return;
    }

    const QSizeF mm = chosen.size(QPageSize::Millimeter);
    ui->singleWidthSpin->setValue(mm.width());
    ui->singleHeightSpin->setValue(mm.height());
    // updatePreview() fires automatically via the spinbox valueChanged signals

#else
    QMessageBox::information(this, tr("Detect Label Size"),
        tr("Automatic label size detection requires Qt 5.13 or later.\n"
           "Please enter the label dimensions manually."));
#endif
}

// ---------- private helpers ----------------------------------------------

void QSLLabelDialog::populatePresets()
{
    FCT_IDENTIFICATION;

    // Avery 5160 / 8160  (US Letter, 30 per page — most common QSL label)
    m_presets << LabelPreset{
        tr("Avery 5160/8160 – 30 per page (Letter)"),
        SheetLayout{66.675, 25.4, 3, 10, 12.70, 4.762, 3.175, 0.0},
        QPageSize::Letter
    };

    // Avery 5163 / 8163  (US Letter, 10 per page)
    m_presets << LabelPreset{
        tr("Avery 5163/8163 – 10 per page (Letter)"),
        SheetLayout{101.6, 50.8, 2, 5, 12.70, 4.762, 3.175, 0.0},
        QPageSize::Letter
    };

    // Avery L7160  (A4, 21 per page — Europe)
    m_presets << LabelPreset{
        tr("Avery L7160 – 21 per page (A4)"),
        SheetLayout{63.5, 38.1, 3, 7, 15.15, 7.25, 2.54, 0.0},
        QPageSize::A4
    };

    // Avery L7161  (A4, 14 per page)
    m_presets << LabelPreset{
        tr("Avery L7161 – 14 per page (A4)"),
        SheetLayout{63.5, 46.6, 3, 5, 15.15, 7.25, 2.54, 0.0},
        QPageSize::A4
    };

    // Avery 5164  (US Letter, 6 per page)
    m_presets << LabelPreset{
        tr("Avery 5164 – 6 per page (Letter)"),
        SheetLayout{101.6, 84.67, 2, 3, 12.70, 4.762, 3.175, 0.0},
        QPageSize::Letter
    };

    // Custom (user fills in all values manually)
    m_presets << LabelPreset{
        tr("Custom"),
        SheetLayout{66.675, 25.4, 3, 10, 12.70, 4.762, 3.175, 0.0},
        QPageSize::Letter
    };

    for (const LabelPreset &p : m_presets)
        ui->presetCombo->addItem(p.name);
}

void QSLLabelDialog::populatePrinterCombo()
{
    FCT_IDENTIFICATION;

    ui->printerCombo->clear();

    const QList<QPrinterInfo> printers = QPrinterInfo::availablePrinters();
    const QString defaultName = QPrinterInfo::defaultPrinter().printerName();
    int defaultIdx = 0;

    for (int i = 0; i < printers.size(); ++i)
    {
        ui->printerCombo->addItem(printers.at(i).printerName());
        if (printers.at(i).printerName() == defaultName)
            defaultIdx = i;
    }

    if (!printers.isEmpty())
        ui->printerCombo->setCurrentIndex(defaultIdx);
}

QSLLabelDialog::SheetLayout QSLLabelDialog::currentSheetLayout() const
{
    return SheetLayout{
        ui->labelWidthSpin->value(),
        ui->labelHeightSpin->value(),
        ui->labelsPerRowSpin->value(),
        ui->labelsPerColSpin->value(),
        ui->topMarginSpin->value(),
        ui->leftMarginSpin->value(),
        ui->hSpacingSpin->value(),
        ui->vSpacingSpin->value()
    };
}

QList<QSLLabelDialog::CallsignGroup> QSLLabelDialog::buildGroups() const
{
    QList<CallsignGroup> groups;

    if (!ui->groupQSOsCheck->isChecked())
    {
        // One group per contact (original 1-label-per-QSO behaviour)
        for (const QSqlRecord &rec : m_contacts)
            groups << CallsignGroup{ rec.value("callsign").toString().toUpper(), { rec } };
        return groups;
    }

    // Group by callsign, preserving first-appearance order
    QHash<QString, int> indexByCall;
    indexByCall.reserve(m_contacts.size());

    for (const QSqlRecord &rec : m_contacts)
    {
        const QString cs = rec.value("callsign").toString().toUpper();
        if (!indexByCall.contains(cs))
        {
            indexByCall[cs] = groups.size();
            groups << CallsignGroup{ cs, {} };
        }
        groups[indexByCall[cs]].contacts << rec;
    }

    // Split any group that exceeds the per-label cap into sequential chunks
    // so the font stays legible.  All QSOs still print — just across multiple labels.
    static const int MAX_QSOS_PER_LABEL = 7;
    QList<CallsignGroup> result;
    result.reserve(groups.size());
    for (const CallsignGroup &grp : groups)
    {
        if (grp.contacts.size() <= MAX_QSOS_PER_LABEL)
        {
            result << grp;
        }
        else
        {
            for (int i = 0; i < grp.contacts.size(); i += MAX_QSOS_PER_LABEL)
                result << CallsignGroup{ grp.callsign,
                                         grp.contacts.mid(i, MAX_QSOS_PER_LABEL) };
        }
    }
    return result;
}

QString QSLLabelDialog::myCallsign(const QSqlRecord &contact) const
{
    const QString cs = contact.value("station_callsign").toString();
    if (!cs.isEmpty())
        return cs;
    return StationProfilesManager::instance()->getCurProfile1().callsign;
}

QString QSLLabelDialog::myGrid(const QSqlRecord &contact) const
{
    const QString g = contact.value("my_gridsquare").toString();
    if (!g.isEmpty())
        return g;
    return StationProfilesManager::instance()->getCurProfile1().locator;
}

// ---------- label rendering ----------------------------------------------
//
// All coordinates are in mm — the caller has applied a scaling transform
// so that 1 painter unit == 1 mm.

void QSLLabelDialog::renderLabel(QPainter *painter, const QRectF &rectMm,
                                  const QList<QSqlRecord> &contacts)
{
    painter->save();

    // Border
    painter->setPen(QPen(Qt::black, 0.3));
    painter->setBrush(Qt::white);
    painter->drawRect(rectMm);

    if (contacts.isEmpty()) { painter->restore(); return; }

    const double padH = rectMm.width()  * 0.03;
    const double padV = rectMm.height() * 0.045;
    const QRectF inner = rectMm.adjusted(padH, padV, -padH, -padV);
    const double W = inner.width();

    const QSqlRecord &first = contacts.first();
    const QString dxCall = first.value("callsign").toString().toUpper();
    const int nQSOs = contacts.size();

    const bool showCall  = ui->showCallsignCheck->isChecked();
    const bool showNQ    = ui->showNameQthCheck->isChecked();
    const bool showDT    = ui->showDateTimeCheck->isChecked();
    const bool showBM    = ui->showBandModeCheck->isChecked();
    const bool showRST   = ui->showRSTCheck->isChecked();
    const bool showFreq  = ui->showFreqCheck->isChecked();
    const bool showMyCS  = ui->showMyCallsignCheck->isChecked();
    const bool grouped   = ui->groupQSOsCheck->isChecked();

    // =========================================================
    // PATH A: Single-QSO line-based layout (grouped off, or only 1 QSO)
    // =========================================================
    if (!grouped || nQSOs == 1)
    {
        struct Line { QString text; bool bold; double relSize; };
        QVector<Line> lines;

        if (showCall && !dxCall.isEmpty())
            lines << Line{ tr("To Radio  %1").arg(dxCall), true,  1.6 };

        lines << Line{ tr("Confirming our QSO"), false, 0.85 };

        if (showDT)
        {
            const QString startTime = first.value("start_time").toString();
            if (!startTime.isEmpty())
            {
                QDateTime dt = QDateTime::fromString(startTime, Qt::ISODate);
                if (!dt.isValid())
                    dt = QDateTime::fromString(startTime, "yyyy-MM-dd hh:mm:ss");
                const QString dateStr = dt.isValid()
                    ? dt.toUTC().toString("yyyy-MM-dd  hh:mm") + " UTC"
                    : startTime;
                lines << Line{ dateStr, false, 1.0 };
            }
        }

        if (showBM)
        {
            const QString band = first.value("band").toString();
            const QString mode = first.value("mode").toString();
            const QString sub  = first.value("submode").toString();
            QString bm;
            if (!band.isEmpty()) bm += band;
            if (!mode.isEmpty()) { if (!bm.isEmpty()) bm += "   "; bm += mode; }
            if (!sub.isEmpty())  bm += "/" + sub;
            if (!bm.isEmpty())
                lines << Line{ bm, false, 1.0 };
        }

        if (showRST)
        {
            const QString rst_s = first.value("rst_sent").toString();
            const QString rst_r = first.value("rst_rcvd").toString();
            if (!rst_s.isEmpty() || !rst_r.isEmpty())
            {
                QString rst;
                if (!rst_s.isEmpty()) rst += tr("RST %1").arg(rst_s);
                if (!rst_r.isEmpty())
                {
                    if (!rst.isEmpty()) rst += "   ";
                    rst += tr("Rcvd %1").arg(rst_r);
                }
                lines << Line{ rst, false, 1.0 };
            }
        }

        if (showFreq)
        {
            const double freq = first.value("freq").toDouble();
            if (freq > 0.0)
                lines << Line{ tr("%1 MHz").arg(freq, 0, 'f', 3), false, 0.9 };
        }

        if (showNQ)
        {
            const QString nameField = first.value("name_intl").toString();
            const QString name = nameField.isEmpty() ? first.value("name").toString() : nameField;
            const QString qthField = first.value("qth_intl").toString();
            const QString qth  = qthField.isEmpty() ? first.value("qth").toString() : qthField;
            QString nq;
            if (!name.isEmpty()) nq += name;
            if (!qth.isEmpty())  { if (!nq.isEmpty()) nq += ", "; nq += qth; }
            if (!nq.isEmpty())
                lines << Line{ nq, false, 1.0 };
        }

        if (showMyCS)
        {
            const QString cs = myCallsign(first);
            QString sig = tr("73 de %1").arg(cs.isEmpty() ? "?" : cs);
            lines << Line{ sig, true, 1.1 };
        }

        if (lines.isEmpty()) { painter->restore(); return; }

        const double totalRelH = [&]() {
            double s = 0.0;
            for (const Line &l : lines) s += l.relSize;
            return s;
        }();
        const double lineSpacingFactor = 1.25;
        const double basePxH = inner.height() / (totalRelH * lineSpacingFactor);

        double y = inner.top();
        for (const Line &l : lines)
        {
            const double thisPxH = basePxH * l.relSize;
            QFont f;
            f.setPixelSize(qMax(1, qRound(thisPxH)));
            f.setBold(l.bold);
            painter->setFont(f);
            painter->setPen(Qt::black);
            const QRectF lineRect(inner.left(), y, W, thisPxH * lineSpacingFactor);
            painter->drawText(lineRect, Qt::AlignLeft | Qt::AlignVCenter, l.text);
            y += thisPxH * lineSpacingFactor;
        }
    }
    // =========================================================
    // PATH B: Multi-QSO table layout (grouped on, N >= 2 QSOs)
    // =========================================================
    // PATH B: Multi-QSO table layout (grouped on, N >= 2 QSOs)
    // Font is always sized for MAX_QSOS_PER_LABEL rows so that labels with
    // fewer QSOs use the same size text and simply leave blank rows at the
    // bottom — giving a consistent look across 2-7 QSOs.
    // =========================================================
    else
    {
        // Helper: make a font whose pixel size fits within a row of height h
        auto makeFont = [](double h, bool bold) -> QFont {
            QFont f;
            f.setPixelSize(qMax(1, qRound(h * 0.78)));
            f.setBold(bold);
            return f;
        };

        // ---- Build active column list ----
        struct Col { QString hdr; ColType type; double frac; };
        QVector<Col> cols;
        if (showDT)   cols << Col{ tr("Date / Time"),  ColDateTime, 0.0 };
        if (showBM)   cols << Col{ tr("Band / Mode"),  ColBandMode, 0.0 };
        if (showRST)  cols << Col{ tr("RST"),          ColRST,      0.0 };
        if (showFreq) cols << Col{ tr("Freq MHz"),     ColFreq,     0.0 };

        // Assign and normalise proportional column widths
        {
            double totW = 0.0;
            for (Col &c : cols)
            {
                switch (c.type)
                {
                case ColDateTime: c.frac = 0.36; break;
                case ColBandMode: c.frac = 0.28; break;
                case ColRST:      c.frac = 0.22; break;
                case ColFreq:     c.frac = 0.18; break;
                }
                totW += c.frac;
            }
            if (totW > 0.0)
                for (Col &c : cols) c.frac /= totW;
        }

        // ---- Height budget — always reserve MAX_QSOS_PER_LABEL data rows ----
        // This keeps the font size constant regardless of how many QSOs are on
        // this label; labels with fewer QSOs just have blank rows at the bottom.
        double totalUnits = 0.0;
        if (showCall && !dxCall.isEmpty()) totalUnits += 1.5;
        if (showNQ)                        totalUnits += 1.0;
        totalUnits += 1.0; // "Confirming our N QSOs"
        if (!cols.isEmpty())
        {
            totalUnits += 0.9;                      // column-header row
            totalUnits += MAX_QSOS_PER_LABEL * 1.0; // fixed 7 data rows
        }
        if (showMyCS) totalUnits += 1.0;

        const double unitH   = inner.height() / qMax(totalUnits, 1.0);
        const double callH   = unitH * 1.5;
        const double baseH   = unitH * 1.0;
        const double colHdrH = unitH * 0.9;

        double y = inner.top();

        // 1. DX Callsign
        if (showCall && !dxCall.isEmpty())
        {
            painter->setFont(makeFont(callH, true));
            painter->setPen(Qt::black);
            painter->drawText(QRectF(inner.left(), y, W, callH),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              tr("To Radio  %1").arg(dxCall));
            y += callH;
        }

        // 2. Name / QTH
        if (showNQ)
        {
            QString nameF = first.value("name_intl").toString();
            if (nameF.isEmpty()) nameF = first.value("name").toString();
            QString qthF  = first.value("qth_intl").toString();
            if (qthF.isEmpty()) qthF = first.value("qth").toString();
            QString nq;
            if (!nameF.isEmpty()) nq += nameF;
            if (!qthF.isEmpty())  { if (!nq.isEmpty()) nq += ",  "; nq += qthF; }
            if (!nq.isEmpty())
            {
                painter->setFont(makeFont(baseH, false));
                painter->setPen(Qt::black);
                painter->drawText(QRectF(inner.left(), y, W, baseH),
                                  Qt::AlignLeft | Qt::AlignVCenter, nq);
                y += baseH;
            }
        }

        // 3. "Confirming our N QSOs"
        {
            painter->setFont(makeFont(baseH, false));
            painter->setPen(Qt::black);
            const QString confirmText = (nQSOs > 1)
                ? tr("Confirming our %1 QSOs").arg(nQSOs)
                : tr("Confirming our QSO");
            painter->drawText(QRectF(inner.left(), y, W, baseH),
                              Qt::AlignLeft | Qt::AlignVCenter, confirmText);
            y += baseH;
        }

        // 4. QSO table
        if (!cols.isEmpty())
        {
            // Column header row — light gray background
            painter->fillRect(QRectF(inner.left(), y, W, colHdrH),
                              QColor(210, 210, 210));
            painter->setFont(makeFont(colHdrH, true));
            painter->setPen(Qt::black);
            {
                double cx = inner.left();
                for (const Col &col : cols)
                {
                    const double cw = W * col.frac;
                    painter->drawText(QRectF(cx + 0.8, y, cw - 1.6, colHdrH),
                                      Qt::AlignLeft | Qt::AlignVCenter, col.hdr);
                    cx += cw;
                }
            }
            y += colHdrH;

            // Thin separator line
            painter->setPen(QPen(Qt::black, 0.2));
            painter->drawLine(QPointF(inner.left(), y), QPointF(inner.right(), y));

            // Data rows — render nQSOs filled rows; remaining rows stay blank
            for (int qi = 0; qi < MAX_QSOS_PER_LABEL; ++qi)
            {
                // Alternating row shading for all rows (filled and blank)
                if (qi % 2 == 1)
                    painter->fillRect(QRectF(inner.left(), y, W, baseH),
                                      QColor(242, 242, 252));

                if (qi < nQSOs)
                {
                    const QSqlRecord &rec = contacts.at(qi);
                    painter->setFont(makeFont(baseH, false));
                    painter->setPen(Qt::black);

                    double cx = inner.left();
                    for (const Col &col : cols)
                    {
                        const double cw = W * col.frac;
                        QString cell;

                        switch (col.type)
                        {
                        case ColDateTime:
                        {
                            const QString startTime = rec.value("start_time").toString();
                            QDateTime dt = QDateTime::fromString(startTime, Qt::ISODate);
                            if (!dt.isValid())
                                dt = QDateTime::fromString(startTime, "yyyy-MM-dd hh:mm:ss");
                            cell = dt.isValid()
                                ? dt.toUTC().toString("dd-MMM-yy hh:mm")
                                : startTime.left(16);
                            break;
                        }
                        case ColBandMode:
                        {
                            const QString band = rec.value("band").toString();
                            const QString mode = rec.value("mode").toString();
                            const QString sub  = rec.value("submode").toString();
                            if (!band.isEmpty()) cell += band;
                            if (!mode.isEmpty()) { if (!cell.isEmpty()) cell += " "; cell += mode; }
                            if (!sub.isEmpty())  cell += "/" + sub;
                            break;
                        }
                        case ColRST:
                        {
                            const QString s = rec.value("rst_sent").toString();
                            const QString r = rec.value("rst_rcvd").toString();
                            if (!s.isEmpty()) cell = s;
                            if (!r.isEmpty()) cell += (cell.isEmpty() ? "" : "/") + r;
                            break;
                        }
                        case ColFreq:
                        {
                            const double freq = rec.value("freq").toDouble();
                            if (freq > 0.0)
                                cell = QString::number(freq, 'f', 3);
                            break;
                        }
                        }

                        painter->drawText(QRectF(cx + 0.8, y, cw - 1.6, baseH),
                                          Qt::AlignLeft | Qt::AlignVCenter, cell);
                        cx += cw;
                    }
                }

                y += baseH;
            }
        }

        // 5. Footer "73 de MYCALL" — right-aligned
        if (showMyCS)
        {
            const QString cs = myCallsign(first);
            QString sig = tr("73 de %1").arg(cs.isEmpty() ? "?" : cs);
            painter->setFont(makeFont(baseH, true));
            painter->setPen(Qt::black);
            painter->drawText(QRectF(inner.left(), y, W, baseH),
                              Qt::AlignRight | Qt::AlignVCenter, sig);
        }
    }

    painter->restore();
}

// ---------- printing -----------------------------------------------------

void QSLLabelDialog::printSheet(QPrinter *printer)
{
    FCT_IDENTIFICATION;

    const SheetLayout lay = currentSheetLayout();
    const int labelsPerPage = lay.labelsPerRow * lay.labelsPerCol;
    const int startOffset   = ui->startLabelSpin->value() - 1; // 0-based

    QPainter painter(printer);
    if (!painter.isActive())
    {
        QMessageBox::warning(this, tr("Print Error"), tr("Failed to open painter on printer."));
        return;
    }

    const double dotsPerMM = printer->resolution() / MM_PER_INCH;
    painter.scale(dotsPerMM, dotsPerMM);

    const QList<CallsignGroup> groups = buildGroups();
    int labelIndex = startOffset;
    bool firstPage = true;

    for (int gi = 0; gi < groups.size(); ++gi)
    {
        if (labelIndex >= labelsPerPage)
        {
            printer->newPage();
            labelIndex = 0;
        }
        else if (!firstPage && labelIndex == 0)
        {
            printer->newPage();
        }
        firstPage = false;

        const int col = labelIndex % lay.labelsPerRow;
        const int row = labelIndex / lay.labelsPerRow;

        const double x = lay.leftMarginMm + col * (lay.labelWidth  + lay.hSpacingMm);
        const double y = lay.topMarginMm  + row * (lay.labelHeight + lay.vSpacingMm);

        renderLabel(&painter,
                    QRectF(x, y, lay.labelWidth, lay.labelHeight),
                    groups.at(gi).contacts);
        ++labelIndex;
    }

    painter.end();
}

void QSLLabelDialog::printSingle(QPrinter *printer)
{
    FCT_IDENTIFICATION;

    const double wMm = ui->singleWidthSpin->value();
    const double hMm = ui->singleHeightSpin->value();

    // Set custom page size to match label.
    // Use setFullPage(false) so Qt/the driver clips to the hardware-printable
    // area; this prevents bottom cut-off on Brother and similar label printers
    // whose printable area is slightly smaller than the physical label.
    printer->setPageSize(QPageSize(QSizeF(wMm, hMm), QPageSize::Millimeter));
    printer->setFullPage(false);

    QPainter painter(printer);
    if (!painter.isActive())
    {
        QMessageBox::warning(this, tr("Print Error"), tr("Failed to open painter on printer."));
        return;
    }

    const double dotsPerMM = printer->resolution() / MM_PER_INCH;
    painter.scale(dotsPerMM, dotsPerMM);

    // Use the actual printable rect so content stays within hardware margins.
    const QSizeF printableMm = printer->pageLayout().paintRect(QPageLayout::Millimeter).size();
    const double rW = printableMm.width()  > 0 ? printableMm.width()  : wMm;
    const double rH = printableMm.height() > 0 ? printableMm.height() : hMm;

    const QList<CallsignGroup> groups = buildGroups();

    for (int gi = 0; gi < groups.size(); ++gi)
    {
        if (gi > 0)
            printer->newPage();
        renderLabel(&painter, QRectF(0, 0, rW, rH), groups.at(gi).contacts);
    }

    painter.end();
}

void QSLLabelDialog::promptAndMarkSent()
{
    FCT_IDENTIFICATION;

    const QMessageBox::StandardButton defaultBtn =
        ui->markSentCheck->isChecked() ? QMessageBox::Yes : QMessageBox::No;

    const auto reply = QMessageBox::question(
        this,
        tr("Mark as QSL Sent"),
        tr("Mark %n contact(s) as QSL Sent (Y)?", "", m_contacts.size()),
        QMessageBox::Yes | QMessageBox::No,
        defaultBtn);

    if (reply == QMessageBox::Yes)
        markContactsAsSent();
}

void QSLLabelDialog::markContactsAsSent()
{
    FCT_IDENTIFICATION;

    const QString today = QDate::currentDate().toString("yyyy-MM-dd");

    QSqlQuery query;
    query.prepare("UPDATE contacts SET qsl_sent = 'Y', qsl_sdate = :date WHERE id = :id");

    for (const QSqlRecord &rec : m_contacts)
    {
        query.bindValue(":date", today);
        query.bindValue(":id",   rec.value("id").toLongLong());
        if (!query.exec())
            qWarning() << "QSLLabelDialog: failed to mark contact as sent:" << query.lastError().text();
    }
}

// ---------- settings persistence -----------------------------------------

void QSLLabelDialog::loadSettings()
{
    FCT_IDENTIFICATION;

    QSettings s;
    s.beginGroup("QSLLabel");

    ui->sheetRadio->setChecked( s.value("isSheet", true).toBool());
    ui->singleRadio->setChecked(!s.value("isSheet", true).toBool());

    const int presetIdx = s.value("presetIndex", 0).toInt();
    if (presetIdx >= 0 && presetIdx < ui->presetCombo->count())
    {
        ui->presetCombo->setCurrentIndex(presetIdx);
        presetChanged(presetIdx);  // fill dimensions for saved preset
    }

    // Allow saved custom values to override preset defaults
    ui->labelWidthSpin->setValue( s.value("labelWidth",  ui->labelWidthSpin->value()).toDouble());
    ui->labelHeightSpin->setValue(s.value("labelHeight", ui->labelHeightSpin->value()).toDouble());
    ui->labelsPerRowSpin->setValue(s.value("perRow", ui->labelsPerRowSpin->value()).toInt());
    ui->labelsPerColSpin->setValue(s.value("perCol", ui->labelsPerColSpin->value()).toInt());
    ui->topMarginSpin->setValue(  s.value("topMargin",  ui->topMarginSpin->value()).toDouble());
    ui->leftMarginSpin->setValue( s.value("leftMargin", ui->leftMarginSpin->value()).toDouble());
    ui->hSpacingSpin->setValue(   s.value("hSpacing",   ui->hSpacingSpin->value()).toDouble());
    ui->vSpacingSpin->setValue(   s.value("vSpacing",   ui->vSpacingSpin->value()).toDouble());
    ui->singleWidthSpin->setValue( s.value("singleWidth",  ui->singleWidthSpin->value()).toDouble());
    ui->singleHeightSpin->setValue(s.value("singleHeight", ui->singleHeightSpin->value()).toDouble());

    ui->groupQSOsCheck->setChecked(      s.value("groupByCallsign", true).toBool());
    ui->showCallsignCheck->setChecked(   s.value("showCallsign",    true).toBool());
    ui->showDateTimeCheck->setChecked(   s.value("showDateTime",    true).toBool());
    ui->showBandModeCheck->setChecked(   s.value("showBandMode",    true).toBool());
    ui->showRSTCheck->setChecked(        s.value("showRST",         true).toBool());
    ui->showFreqCheck->setChecked(       s.value("showFreq",       false).toBool());
    ui->showNameQthCheck->setChecked(    s.value("showNameQth",     true).toBool());
    ui->showMyCallsignCheck->setChecked( s.value("showMyCallsign",  true).toBool());
    ui->markSentCheck->setChecked(       s.value("markSent",        true).toBool());

    // Restore last-used printer (printerCombo already populated before loadSettings)
    const QString savedPrinter = s.value("printerName").toString();
    if (!savedPrinter.isEmpty())
    {
        const int idx = ui->printerCombo->findText(savedPrinter);
        if (idx >= 0)
            ui->printerCombo->setCurrentIndex(idx);
    }

    s.endGroup();
}

void QSLLabelDialog::saveSettings()
{
    FCT_IDENTIFICATION;

    QSettings s;
    s.beginGroup("QSLLabel");

    s.setValue("isSheet",         ui->sheetRadio->isChecked());
    s.setValue("presetIndex",     ui->presetCombo->currentIndex());
    s.setValue("labelWidth",      ui->labelWidthSpin->value());
    s.setValue("labelHeight",     ui->labelHeightSpin->value());
    s.setValue("perRow",          ui->labelsPerRowSpin->value());
    s.setValue("perCol",          ui->labelsPerColSpin->value());
    s.setValue("topMargin",       ui->topMarginSpin->value());
    s.setValue("leftMargin",      ui->leftMarginSpin->value());
    s.setValue("hSpacing",        ui->hSpacingSpin->value());
    s.setValue("vSpacing",        ui->vSpacingSpin->value());
    s.setValue("singleWidth",     ui->singleWidthSpin->value());
    s.setValue("singleHeight",    ui->singleHeightSpin->value());
    s.setValue("groupByCallsign", ui->groupQSOsCheck->isChecked());
    s.setValue("showCallsign",    ui->showCallsignCheck->isChecked());
    s.setValue("showDateTime",    ui->showDateTimeCheck->isChecked());
    s.setValue("showBandMode",    ui->showBandModeCheck->isChecked());
    s.setValue("showRST",         ui->showRSTCheck->isChecked());
    s.setValue("showFreq",        ui->showFreqCheck->isChecked());
    s.setValue("showNameQth",     ui->showNameQthCheck->isChecked());
    s.setValue("showMyCallsign",  ui->showMyCallsignCheck->isChecked());
    s.setValue("markSent",        ui->markSentCheck->isChecked());
    s.setValue("printerName",     ui->printerCombo->currentText());

    s.endGroup();
}
