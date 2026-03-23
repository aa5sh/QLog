#include "CabrilloTemplateDialog.h"
#include "ui_CabrilloTemplateDialog.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QMessageBox>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QTableWidgetItem>
#include <QListWidgetItem>
#include <QHeaderView>
#include <QEvent>
#include <algorithm>

#include "core/debug.h"
#include "data/Data.h"
#include "models/LogbookModel.h"

MODULE_IDENTIFICATION("qlog.ui.cabrillotemplatedialog");

CabrilloTemplateDialog::CabrilloTemplateDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CabrilloTemplateDialog),
    currentTemplateIndex(-1)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    // Left panel (template list) stays compact, right panel (editor) expands
    ui->mainHLayout->setStretch(0, 0);
    ui->mainHLayout->setStretch(1, 1);

    // Columns table: auto-size columns to content
    QHeaderView *hdr = ui->columnsTable->horizontalHeader();
    hdr->setSectionResizeMode(QHeaderView::ResizeToContents);

    // Populate available DB fields with translated names
    QSqlRecord contactsRecord = QSqlDatabase::database().record("contacts");

    for ( int i = LogbookModel::ColumnID::COLUMN_ID; i < LogbookModel::ColumnID::COLUMN_LAST_ELEMENT; ++i )
    {
        LogbookModel::ColumnID columnID = static_cast<LogbookModel::ColumnID>(i);
        const QString translation = LogbookModel::getFieldNameTranslation(columnID);
        if ( translation.isEmpty() )
            continue;

        const QString dbField = contactsRecord.fieldName(i);
        if ( dbField.isEmpty() )
            continue;

        dbFieldItems.append({dbField, translation});
    }

    std::sort(dbFieldItems.begin(), dbFieldItems.end(),
              [](const CabrilloFormat::CategoryItem &a,
                 const CabrilloFormat::CategoryItem &b)
    {
        return a.label.localeAwareCompare(b.label) < 0;
    });

    formatterItems = CabrilloFormat::formatterTypes();

    for ( const CabrilloFormat::CategoryItem &item : static_cast<const QList<CabrilloFormat::CategoryItem>>(CabrilloFormat::modeCategories()) )
        ui->defaultModeCombo->addItem(item.label, item.value);

    populateContestCombo();

    connect(ui->nameEdit, &QLineEdit::textChanged,
            this, [this](const QString &text)
    {
        QListWidgetItem *item = ui->templateList->currentItem();
        if ( item && currentTemplateIndex >= 0
             && currentTemplateIndex < templates.size() )
            item->setText(text);
    });

    loadTemplatesFromDB();
    populateTemplateList();

    if ( !templates.isEmpty() )
        ui->templateList->setCurrentRow(0);

    // Calculate dialog width so the columns table fits without horizontal scrollbar
    int tableWidth = hdr->length() + ui->columnsTable->frameWidth() * 2;
    int leftPanelWidth = ui->leftLayout->sizeHint().width();
    QMargins m = ui->mainHLayout->contentsMargins();
    int needed = m.left() + m.right()
               + ui->mainHLayout->spacing()
               + leftPanelWidth + tableWidth;

    resize(qMax(needed, width()), height());

    // After sizing, let last column stretch when user resizes the dialog
    hdr->setSectionResizeMode(hdr->count() - 1, QHeaderView::Stretch);
}

CabrilloTemplateDialog::~CabrilloTemplateDialog()
{
    FCT_IDENTIFICATION;

    delete ui;
}

void CabrilloTemplateDialog::installWheelGuard(QWidget *widget)
{
    widget->setFocusPolicy(Qt::StrongFocus);
    widget->installEventFilter(this);
}

// Prevent mouse wheel from changing combo/spin values while scrolling the table;
// wheel events are only handled when the widget has focus (after clicking on it)
bool CabrilloTemplateDialog::eventFilter(QObject *obj, QEvent *event)
{
    if ( event->type() == QEvent::Wheel )
    {
        QWidget *widget = qobject_cast<QWidget *>(obj);
        if ( widget && !widget->hasFocus() )
        {
            event->ignore();
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}

void CabrilloTemplateDialog::selectTemplate(int templateId)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << templateId;

    for ( int i = 0; i < ui->templateList->count(); i++ )
    {
        int idx = ui->templateList->item(i)->data(Qt::UserRole).toInt();
        if ( idx >= 0 && idx < templates.size()
             && templates[idx].id == templateId )
        {
            ui->templateList->setCurrentRow(i);
            return;
        }
    }
}

void CabrilloTemplateDialog::populateContestCombo()
{
    FCT_IDENTIFICATION;

    const QStringList contests = Data::instance()->contestList();
    for ( const QString &name : contests )
        ui->contestCombo->addItem(name);
}

void CabrilloTemplateDialog::loadTemplatesFromDB()
{
    FCT_IDENTIFICATION;

    templates.clear();

    const QList<CabrilloFormat::TemplateInfo> infoList = CabrilloFormat::templateList();
    for ( const CabrilloFormat::TemplateInfo &info : infoList )
    {
        TemplateData tmpl;
        tmpl.id = info.id;
        tmpl.name = info.name;
        tmpl.contestName = info.contestName;
        tmpl.defaultMode = info.defaultCategoryMode;
        tmpl.columns = CabrilloFormat::loadTemplateColumns(info.id);
        tmpl.isNew = false;
        tmpl.isModified = false;
        tmpl.isDeleted = false;
        templates.append(tmpl);
    }
}

void CabrilloTemplateDialog::populateTemplateList()
{
    FCT_IDENTIFICATION;

    ui->templateList->clear();

    for ( int i = 0; i < templates.size(); i++ )
    {
        const TemplateData &tmpl = templates[i];
        if ( tmpl.isDeleted )
            continue;

        QListWidgetItem *item = new QListWidgetItem(tmpl.name, ui->templateList);
        item->setData(Qt::UserRole, i);
    }
}

void CabrilloTemplateDialog::templateSelectionChanged()
{
    FCT_IDENTIFICATION;

    // Save current template before switching
    if ( currentTemplateIndex >= 0 && currentTemplateIndex < templates.size() )
        saveCurrentTemplate();

    QListWidgetItem *item = ui->templateList->currentItem();
    if ( !item )
    {
        currentTemplateIndex = -1;
        return;
    }

    currentTemplateIndex = item->data(Qt::UserRole).toInt();
    showTemplateEditor(currentTemplateIndex);
}

void CabrilloTemplateDialog::showTemplateEditor(int index)
{
    FCT_IDENTIFICATION;

    if ( index < 0 || index >= templates.size() )
        return;

    const TemplateData &tmpl = templates[index];

    ui->nameEdit->setText(tmpl.name);

    int cidIdx = ui->contestCombo->findText(tmpl.contestName);

    if ( cidIdx >= 0 )
        ui->contestCombo->setCurrentIndex(cidIdx);
    else
        ui->contestCombo->setEditText(tmpl.contestName);

    int modeIdx = ui->defaultModeCombo->findData(tmpl.defaultMode);

    if ( modeIdx >= 0 )
        ui->defaultModeCombo->setCurrentIndex(modeIdx);

    populateColumnsTable(tmpl.columns);
}

void CabrilloTemplateDialog::populateColumnsTable(const QList<CabrilloFormat::ColumnDef> &columns)
{
    FCT_IDENTIFICATION;

    ui->columnsTable->setRowCount(0);
    ui->columnsTable->setRowCount(columns.size());

    for ( int row = 0; row < columns.size(); row++ )
        setupColumnRow(row, columns[row]);

    ui->columnsTable->resizeColumnsToContents();
}

void CabrilloTemplateDialog::setupColumnRow(int row, const CabrilloFormat::ColumnDef &col)
{
    FCT_IDENTIFICATION;

    // Position (read-only)
    QTableWidgetItem *posItem = new QTableWidgetItem(QString::number(col.position));
    posItem->setFlags(posItem->flags() & ~Qt::ItemIsEditable);
    ui->columnsTable->setItem(row, 0, posItem);

    // DB Field (ComboBox with translated names)
    QComboBox *fieldCombo = new QComboBox(this);
    installWheelGuard(fieldCombo);
    fieldCombo->addItem(QString(), QLatin1String("")); // empty entry for non-DB fields
    for ( const CabrilloFormat::CategoryItem &item : static_cast<const QList<CabrilloFormat::CategoryItem>&>(dbFieldItems) )
        fieldCombo->addItem(item.label, item.value);

    int fieldIdx = fieldCombo->findData(col.dbField);

    if ( fieldIdx >= 0 )
        fieldCombo->setCurrentIndex(fieldIdx);

    ui->columnsTable->setCellWidget(row, 1, fieldCombo);

    // Formatter (ComboBox)
    QComboBox *fmtCombo = new QComboBox(this);
    installWheelGuard(fmtCombo);
    for ( const CabrilloFormat::CategoryItem &fmt : static_cast<const QList<CabrilloFormat::CategoryItem>&>(formatterItems) )
        fmtCombo->addItem(fmt.label, fmt.value);

    int fmtIdx = fmtCombo->findData(col.formatter);

    if ( fmtIdx >= 0 )
        fmtCombo->setCurrentIndex(fmtIdx);

    ui->columnsTable->setCellWidget(row, 2, fmtCombo);

    // Width (SpinBox)
    QSpinBox *widthSpin = new QSpinBox(this);
    installWheelGuard(widthSpin);
    widthSpin->setRange(1, 30);
    widthSpin->setValue(col.width);
    ui->columnsTable->setCellWidget(row, 3, widthSpin);

    // Label (QLineEdit)
    QLineEdit *labelEdit = new QLineEdit(col.label, this);
    ui->columnsTable->setCellWidget(row, 4, labelEdit);

    // Auto-adjust fields when Transmitter ID formatter is selected
    auto applyTransmitterIdDefaults = [fieldCombo, widthSpin, labelEdit](const QString &fmtValue)
    {
        bool isTxId = (fmtValue == CabrilloFormat::FMT_TRANSMITTER_ID);
        fieldCombo->setEnabled(!isTxId);
        if ( isTxId )
        {
            fieldCombo->setCurrentIndex(0); // empty
            widthSpin->setValue(1);
            labelEdit->setText("t");
        }
    };

    connect(fmtCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [fmtCombo, applyTransmitterIdDefaults](int)
    {
        applyTransmitterIdDefaults(fmtCombo->currentData().toString());
    });

    // Apply defaults for initially loaded transmitter_id columns
    applyTransmitterIdDefaults(col.formatter);
}

QList<CabrilloFormat::ColumnDef> CabrilloTemplateDialog::readColumnsFromTable() const
{
    FCT_IDENTIFICATION;

    QList<CabrilloFormat::ColumnDef> columns;

    for ( int row = 0; row < ui->columnsTable->rowCount(); row++ )
    {
        CabrilloFormat::ColumnDef col;
        col.position = row + 1;

        QComboBox *fieldCombo = qobject_cast<QComboBox *>(ui->columnsTable->cellWidget(row, 1));
        col.dbField = fieldCombo ? fieldCombo->currentData().toString() : QLatin1String("");

        QComboBox *fmtCombo = qobject_cast<QComboBox *>(ui->columnsTable->cellWidget(row, 2));
        col.formatter = fmtCombo ? fmtCombo->currentData().toString() : "";

        QSpinBox *widthSpin = qobject_cast<QSpinBox *>(ui->columnsTable->cellWidget(row, 3));
        col.width = widthSpin ? widthSpin->value() : 5;

        QLineEdit *labelEdit = qobject_cast<QLineEdit *>(ui->columnsTable->cellWidget(row, 4));
        col.label = labelEdit ? labelEdit->text() : "";

        columns.append(col);
    }

    return columns;
}

void CabrilloTemplateDialog::saveCurrentTemplate()
{
    FCT_IDENTIFICATION;

    if ( currentTemplateIndex < 0 || currentTemplateIndex >= templates.size() )
        return;

    TemplateData &tmpl = templates[currentTemplateIndex];

    tmpl.name = ui->nameEdit->text();
    tmpl.contestName = ui->contestCombo->currentText();
    tmpl.defaultMode = ui->defaultModeCombo->currentData().toString();
    tmpl.columns = readColumnsFromTable();
    tmpl.isModified = true;
}

void CabrilloTemplateDialog::newTemplate()
{
    FCT_IDENTIFICATION;

    TemplateData tmpl;
    tmpl.id = -1;
    tmpl.name = tr("New Template");
    tmpl.defaultMode = CabrilloFormat::MODE_CW;
    tmpl.isNew = true;
    tmpl.isModified = true;
    tmpl.isDeleted = false;

    // Default columns (common prefix)
    tmpl.columns =
    {
        {1, "freq",       5,  CabrilloFormat::FMT_FREQ_KHZ,         "Freq"},
        {2, "mode",       2,  CabrilloFormat::FMT_MODE_CABRILLO,    "Mode"},
        {3, "start_time", 10, CabrilloFormat::FMT_DATE_YYYY_MM_DD,  "Date"},
        {4, "start_time", 4,  CabrilloFormat::FMT_TIME_HHMM,        "Time"},
    };

    templates.append(tmpl);
    populateTemplateList();
    ui->templateList->setCurrentRow(ui->templateList->count() - 1);
}

void CabrilloTemplateDialog::copyTemplate()
{
    FCT_IDENTIFICATION;

    if ( currentTemplateIndex < 0 || currentTemplateIndex >= templates.size() )
        return;

    // Save current state first
    saveCurrentTemplate();

    const TemplateData &src = templates[currentTemplateIndex];

    TemplateData tmpl;
    tmpl.id = -1;
    tmpl.name = tr("Copy - %1").arg(src.name);
    tmpl.contestName = src.contestName;
    tmpl.defaultMode = src.defaultMode;
    tmpl.columns = src.columns;
    tmpl.isNew = true;
    tmpl.isModified = true;
    tmpl.isDeleted = false;

    templates.append(tmpl);
    populateTemplateList();
    ui->templateList->setCurrentRow(ui->templateList->count() - 1);
}

void CabrilloTemplateDialog::deleteTemplate()
{
    FCT_IDENTIFICATION;

    if ( currentTemplateIndex < 0 || currentTemplateIndex >= templates.size() )
        return;

    TemplateData &tmpl = templates[currentTemplateIndex];

    int ret = QMessageBox::question(this, tr("Delete Template"),
                                    tr("Delete template '%1'?").arg(tmpl.name),
                                    QMessageBox::Yes | QMessageBox::No);
    if ( ret != QMessageBox::Yes )
        return;

    tmpl.isDeleted = true;
    currentTemplateIndex = -1;
    populateTemplateList();

    if ( ui->templateList->count() > 0 )
        ui->templateList->setCurrentRow(0);
}

void CabrilloTemplateDialog::addColumn()
{
    FCT_IDENTIFICATION;

    int row = ui->columnsTable->rowCount();
    ui->columnsTable->setRowCount(row + 1);

    CabrilloFormat::ColumnDef col;
    col.position = row + 1;
    col.dbField = "callsign";
    col.width = 13;

    setupColumnRow(row, col);
    ui->columnsTable->scrollToItem(ui->columnsTable->item(row, 0));
    ui->columnsTable->setCurrentCell(row, 0);
}

void CabrilloTemplateDialog::removeColumn()
{
    FCT_IDENTIFICATION;

    int row = ui->columnsTable->currentRow();
    if ( row < 0 )
        return;

    ui->columnsTable->removeRow(row);

    // Renumber positions
    for ( int i = 0; i < ui->columnsTable->rowCount(); i++ )
    {
        QTableWidgetItem *posItem = ui->columnsTable->item(i, 0);
        if ( posItem )
            posItem->setText(QString::number(i + 1));
    }
}

void CabrilloTemplateDialog::moveColumnUp()
{
    FCT_IDENTIFICATION;
    moveColumn(-1);
}

void CabrilloTemplateDialog::moveColumnDown()
{
    FCT_IDENTIFICATION;
    moveColumn(1);
}

void CabrilloTemplateDialog::moveColumn(int direction)
{
    FCT_IDENTIFICATION;

    int row = ui->columnsTable->currentRow();
    int targetRow = row + direction;

    if ( row < 0 || targetRow < 0 || targetRow >= ui->columnsTable->rowCount() )
        return;

    QList<CabrilloFormat::ColumnDef> columns = readColumnsFromTable();

#if (QT_VERSION >= QT_VERSION_CHECK(5, 13, 0))
    columns.swapItemsAt(row, targetRow);
#else
    columns.swap(row, targetRow);
#endif

    for ( int i = 0; i < columns.size(); i++ )
        columns[i].position = i + 1;

    populateColumnsTable(columns);
    ui->columnsTable->setCurrentCell(targetRow, 0);
}

bool CabrilloTemplateDialog::saveTemplateColumns(QSqlQuery &query, int templateId,
                                                   const QList<CabrilloFormat::ColumnDef> &columns)
{
    FCT_IDENTIFICATION;

    // Delete existing columns
    if ( !query.prepare("DELETE FROM cabrillo_template_columns WHERE template_id = :tid") )
    {
        qCWarning(runtime) << query.lastError().text();
        return false;
    }
    query.bindValue(":tid", templateId);
    if ( !query.exec() )
    {
        qCWarning(runtime) << query.lastError().text();
        return false;
    }

    // Insert columns — prepare once, bind+exec in loop
    if ( !query.prepare("INSERT INTO cabrillo_template_columns "
                        "(template_id, position, db_field, width, formatter, label) "
                        "VALUES (:tid, :pos, :field, :width, :fmt, :label)") )
    {
        qCWarning(runtime) << query.lastError().text();
        return false;
    }

    for ( const CabrilloFormat::ColumnDef &col : columns )
    {
        query.bindValue(":tid", templateId);
        query.bindValue(":pos", col.position);
        query.bindValue(":field", col.dbField);
        query.bindValue(":width", col.width);
        query.bindValue(":fmt", col.formatter);
        query.bindValue(":label", col.label);

        if ( !query.exec() )
        {
            qCWarning(runtime) << query.lastError().text();
            return false;
        }
    }

    return true;
}

void CabrilloTemplateDialog::accept()
{
    FCT_IDENTIFICATION;

    saveCurrentTemplate();

    QSqlDatabase db = QSqlDatabase::database();
    if ( !db.transaction() )
    {
        qCWarning(runtime) << "Failed to start transaction";
        QMessageBox::critical(this, tr("QLog Error"), tr("Cannot start database transaction."));
        return;
    }

    QSqlQuery query;

    // Process deletions (ON DELETE CASCADE handles columns automatically)
    for ( const TemplateData &tmpl : static_cast<const QList<TemplateData>&>(templates) )
    {
        if ( !tmpl.isDeleted || tmpl.isNew )
            continue;

        if ( !query.prepare("DELETE FROM cabrillo_templates WHERE id = :id") )
        {
            qCWarning(runtime) << query.lastError().text();
            db.rollback();
            return;
        }
        query.bindValue(":id", tmpl.id);
        if ( !query.exec() )
        {
            qCWarning(runtime) << query.lastError().text();
            db.rollback();
            return;
        }
    }

    // Process new and modified templates
    for ( const TemplateData &tmpl : static_cast<const QList<TemplateData>&>(templates) )
    {
        if ( tmpl.isDeleted )
            continue;

        int targetId = tmpl.id;

        if ( tmpl.isNew )
        {
            if ( !query.prepare("INSERT INTO cabrillo_templates "
                                "(name, contest_name_for_header, default_category_mode) "
                                "VALUES (:name, :contest, :mode)") )
            {
                qCWarning(runtime) << query.lastError().text();
                db.rollback();
                return;
            }
            query.bindValue(":name", tmpl.name);
            query.bindValue(":contest", tmpl.contestName);
            query.bindValue(":mode", tmpl.defaultMode);

            if ( !query.exec() )
            {
                qCWarning(runtime) << query.lastError().text();
                QMessageBox::warning(this, tr("QLog Warning"),
                                     tr("Cannot save template '%1': %2").arg(tmpl.name, query.lastError().text()));
                db.rollback();
                return;
            }

            targetId = query.lastInsertId().toInt();
        }
        else if ( tmpl.isModified )
        {
            if ( !query.prepare("UPDATE cabrillo_templates SET name = :name, "
                                "contest_name_for_header = :contest, default_category_mode = :mode "
                                "WHERE id = :id") )
            {
                qCWarning(runtime) << query.lastError().text();
                db.rollback();
                return;
            }
            query.bindValue(":name", tmpl.name);
            query.bindValue(":contest", tmpl.contestName);
            query.bindValue(":mode", tmpl.defaultMode);
            query.bindValue(":id", tmpl.id);

            if ( !query.exec() )
            {
                qCWarning(runtime) << query.lastError().text();
                db.rollback();
                return;
            }
        }
        else
        {
            continue;
        }

        if ( !saveTemplateColumns(query, targetId, tmpl.columns) )
        {
            db.rollback();
            return;
        }
    }

    if ( !db.commit() )
    {
        qCWarning(runtime) << "Failed to commit transaction";
        db.rollback();
        return;
    }

    QDialog::accept();
}
