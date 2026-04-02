#include "SqlQueryDialog.h"
#include "ui_SqlQueryDialog.h"
#include "ui/component/SqlHighlighter.h"
#include "ui/ExportDialog.h"
#include "core/debug.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTextCursor>
#include <QTextStream>

MODULE_IDENTIFICATION("qlog.ui.sqlquerydialog");

// DML / DDL keywords that must never appear in a safe query
static const QStringList BLOCKED_KEYWORDS = {
    "INSERT", "UPDATE", "DELETE", "DROP", "CREATE", "ALTER",
    "ATTACH", "DETACH", "REPLACE", "UPSERT", "TRUNCATE", "RENAME",
    "PRAGMA", "VACUUM", "REINDEX", "ANALYZE"
};

// Full completion word list (SQL keywords + built-in functions)
static const QStringList SQL_COMPLETIONS = {
    "SELECT", "FROM", "WHERE", "AND", "OR", "NOT", "IN", "LIKE", "BETWEEN",
    "IS", "NULL", "ORDER", "BY", "GROUP", "HAVING", "LIMIT", "OFFSET",
    "DISTINCT", "ALL", "AS", "JOIN", "LEFT", "RIGHT", "INNER", "OUTER",
    "FULL", "CROSS", "ON", "UNION", "INTERSECT", "EXCEPT", "WITH",
    "RECURSIVE", "CASE", "WHEN", "THEN", "ELSE", "END", "EXISTS",
    "ASC", "DESC", "COLLATE", "CAST", "TRUE", "FALSE", "ROWID",
    "NOCASE", "BINARY",
    "COUNT", "SUM", "AVG", "MIN", "MAX", "ABS", "LENGTH", "LOWER", "UPPER",
    "SUBSTR", "TRIM", "LTRIM", "RTRIM", "REPLACE", "INSTR", "PRINTF",
    "ROUND", "COALESCE", "NULLIF", "IFNULL", "IIF", "TYPEOF",
    "DATE", "TIME", "DATETIME", "JULIANDAY", "STRFTIME",
    "RANDOM", "HEX", "QUOTE", "GROUP_CONCAT", "JSON_EXTRACT"
};

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

SqlQueryDialog::SqlQueryDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::SqlQueryDialog),
      highlighter(nullptr),
      completer(nullptr),
      completerModel(new QStringListModel(this)),
      queryModel(new QSqlQueryModel(this)),
      sortProxy(new QSortFilterProxyModel(this))
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);
    setWindowTitle(tr("SQL Query"));

    // Restore geometry & splitter state
    QSettings settings;
    restoreGeometry(settings.value("SqlQueryDialog/geometry").toByteArray());
    ui->splitter->restoreState(settings.value("SqlQueryDialog/splitter").toByteArray());
    if (ui->splitter->sizes().value(0, 0) == 0) {
        // First run — sensible default split
        ui->splitter->setSizes({250, 450});
    }

    // Monospace font for the editor
    QFont editorFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    editorFont.setPointSize(10);
    ui->sqlEditor->setFont(editorFont);

    // Syntax highlighter
    highlighter = new SqlHighlighter(ui->sqlEditor->document());

    // Results model + sortable proxy
    sortProxy->setSourceModel(queryModel);
    ui->resultsTable->setModel(sortProxy);
    ui->resultsTable->horizontalHeader()->setStretchLastSection(true);
    ui->resultsTable->verticalHeader()->setVisible(false);

    // Autocomplete
    completer = new QCompleter(completerModel, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setFilterMode(Qt::MatchContains);
    completer->setWidget(ui->sqlEditor);
    connect(completer,
            QOverload<const QString &>::of(&QCompleter::activated),
            this, &SqlQueryDialog::insertCompletion);

    // Export drop-down menu
    QMenu *exportMenu = new QMenu(this);
    connect(exportMenu->addAction(tr("Export as TXT...")), &QAction::triggered,
            this, &SqlQueryDialog::exportAsTxt);
    connect(exportMenu->addAction(tr("Export as CSV...")), &QAction::triggered,
            this, &SqlQueryDialog::exportAsCsv);
    connect(exportMenu->addAction(tr("Export as ADIF...")), &QAction::triggered,
            this, &SqlQueryDialog::exportAsAdif);
    ui->exportButton->setMenu(exportMenu);

    // Toolbar button signals
    connect(ui->openButton, &QPushButton::clicked, this, &SqlQueryDialog::openQuery);
    connect(ui->saveButton, &QPushButton::clicked, this, &SqlQueryDialog::saveQuery);
    connect(ui->runButton,  &QPushButton::clicked, this, &SqlQueryDialog::runQuery);

    // Text-change drives live autocomplete
    connect(ui->sqlEditor, &QPlainTextEdit::textChanged,
            this, &SqlQueryDialog::onTextChanged);

    // Install event filter for F5 / Ctrl+Return / Ctrl+Space / popup navigation
    ui->sqlEditor->installEventFilter(this);

    // Load db schema into highlighter + completion list
    loadSchema();
}

SqlQueryDialog::~SqlQueryDialog()
{
    QSettings settings;
    settings.setValue("SqlQueryDialog/geometry",  saveGeometry());
    settings.setValue("SqlQueryDialog/splitter",  ui->splitter->saveState());
    delete ui;
}

// ---------------------------------------------------------------------------
// Schema loading
// ---------------------------------------------------------------------------

void SqlQueryDialog::loadSchema()
{
    FCT_IDENTIFICATION;

    QStringList schemaIds;

    QSqlQuery q;
    if (q.exec("SELECT name FROM sqlite_master WHERE type IN ('table','view') ORDER BY name")) {
        while (q.next()) {
            const QString tableName = q.value(0).toString();
            schemaIds.append(tableName);

            QSqlQuery colQ;
            if (colQ.exec(QString("PRAGMA table_info(%1)").arg(tableName))) {
                while (colQ.next()) {
                    const QString col = colQ.value(1).toString();
                    if (!schemaIds.contains(col, Qt::CaseInsensitive))
                        schemaIds.append(col);
                }
            }
        }
    }

    // Combined completion list: SQL keywords + schema identifiers, sorted
    QStringList combined = SQL_COMPLETIONS;
    for (const QString &id : qAsConst(schemaIds)) {
        if (!combined.contains(id, Qt::CaseInsensitive))
            combined.append(id);
    }
    combined.sort(Qt::CaseInsensitive);
    completerModel->setStringList(combined);

    // Highlight schema names in a distinct colour
    highlighter->setUserIdentifiers(schemaIds);
}

// ---------------------------------------------------------------------------
// Safety check — only pure SELECT / WITH … SELECT are allowed
// ---------------------------------------------------------------------------

bool SqlQueryDialog::isSafeQuery(const QString &sql) const
{
    // Strip single-line comments
    static const QRegularExpression slComment("--[^\n]*");
    // Strip block comments (non-greedy, dotall)
    static const QRegularExpression mlComment(
        "/\\*.*?\\*/",
        QRegularExpression::DotMatchesEverythingOption);

    QString stripped = sql;
    stripped.remove(slComment);
    stripped.remove(mlComment);
    stripped = stripped.trimmed();

    // Must start with SELECT or WITH (for CTEs that end in SELECT)
    if (!stripped.startsWith("SELECT", Qt::CaseInsensitive) &&
        !stripped.startsWith("WITH",   Qt::CaseInsensitive))
        return false;

    // Reject any blocked keyword appearing as a whole word
    for (const QString &kw : BLOCKED_KEYWORDS) {
        const QRegularExpression re(
            QString("\\b%1\\b").arg(kw),
            QRegularExpression::CaseInsensitiveOption);
        if (re.match(stripped).hasMatch())
            return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Autocomplete helpers
// ---------------------------------------------------------------------------

QString SqlQueryDialog::currentWord() const
{
    QTextCursor cursor = ui->sqlEditor->textCursor();
    cursor.select(QTextCursor::WordUnderCursor);
    return cursor.selectedText();
}

void SqlQueryDialog::showCompleterPopup()
{
    QRect cr = ui->sqlEditor->cursorRect();
    cr.setWidth(
        completer->popup()->sizeHintForColumn(0)
        + completer->popup()->verticalScrollBar()->sizeHint().width());
    completer->complete(cr);
}

// Called whenever the editor text changes — drives live completion
void SqlQueryDialog::onTextChanged()
{
    // Don't interfere while the user is navigating the popup
    if (completer->popup()->isVisible() &&
        completer->popup()->currentIndex().isValid())
        return;

    const QString word = currentWord();
    if (word.length() < 2) {
        completer->popup()->hide();
        return;
    }

    if (word != completer->completionPrefix()) {
        completer->setCompletionPrefix(word);
        if (completer->completionCount() > 0)
            showCompleterPopup();
        else
            completer->popup()->hide();
    }
}

// Insert the chosen completion, replacing the partial word already typed
void SqlQueryDialog::insertCompletion(const QString &completion)
{
    FCT_IDENTIFICATION;

    QTextCursor cursor = ui->sqlEditor->textCursor();
    const int extra = completion.length() - completer->completionPrefix().length();
    cursor.movePosition(QTextCursor::Left);
    cursor.movePosition(QTextCursor::EndOfWord);
    cursor.insertText(completion.right(extra));
    ui->sqlEditor->setTextCursor(cursor);
}

// ---------------------------------------------------------------------------
// Event filter — keyboard shortcuts & popup navigation
// ---------------------------------------------------------------------------

bool SqlQueryDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (obj != ui->sqlEditor || event->type() != QEvent::KeyPress)
        return QDialog::eventFilter(obj, event);

    QKeyEvent *ke = static_cast<QKeyEvent *>(event);

    // When the completion popup is open, forward navigation keys to it
    if (completer->popup()->isVisible()) {
        switch (ke->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            QApplication::sendEvent(completer->popup(), event);
            return true;
        default:
            break;
        }
    }

    // F5 or Ctrl+Return → run query
    if (ke->key() == Qt::Key_F5 ||
        (ke->key() == Qt::Key_Return &&
         (ke->modifiers() & Qt::ControlModifier))) {
        runQuery();
        return true;
    }

    // Ctrl+Space → force-show completer with current word as prefix
    if (ke->key() == Qt::Key_Space &&
        (ke->modifiers() & Qt::ControlModifier)) {
        const QString word = currentWord();
        completer->setCompletionPrefix(word);
        if (completer->completionCount() == 0)
            completer->setCompletionPrefix("");
        showCompleterPopup();
        return true;
    }

    return false; // let the editor handle everything else normally
}

// ---------------------------------------------------------------------------
// File operations
// ---------------------------------------------------------------------------

void SqlQueryDialog::openQuery()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    const QString lastDir =
        settings.value("SqlQueryDialog/lastDir", QDir::homePath()).toString();

    const QString filename = QFileDialog::getOpenFileName(
        this, tr("Open SQL Query"), lastDir,
        tr("SQL Files (*.sql);;All Files (*)"));

    if (filename.isEmpty())
        return;

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Open Error"),
            tr("Cannot open file:\n%1").arg(file.errorString()));
        return;
    }

    QTextStream in(&file);
    ui->sqlEditor->setPlainText(in.readAll());
    settings.setValue("SqlQueryDialog/lastDir", QFileInfo(filename).absolutePath());
}

void SqlQueryDialog::saveQuery()
{
    FCT_IDENTIFICATION;

    QSettings settings;
    const QString lastDir =
        settings.value("SqlQueryDialog/lastDir", QDir::homePath()).toString();

    QString filename = QFileDialog::getSaveFileName(
        this, tr("Save SQL Query"), lastDir,
        tr("SQL Files (*.sql);;All Files (*)"));

    if (filename.isEmpty())
        return;

    if (!filename.endsWith(".sql", Qt::CaseInsensitive))
        filename += ".sql";

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Save Error"),
            tr("Cannot save file:\n%1").arg(file.errorString()));
        return;
    }

    QTextStream out(&file);
    out << ui->sqlEditor->toPlainText();
    settings.setValue("SqlQueryDialog/lastDir", QFileInfo(filename).absolutePath());
    ui->statusLabel->setText(tr("Query saved to %1").arg(QFileInfo(filename).fileName()));
}

// ---------------------------------------------------------------------------
// Run query
// ---------------------------------------------------------------------------

void SqlQueryDialog::runQuery()
{
    FCT_IDENTIFICATION;

    const QString sql = ui->sqlEditor->toPlainText().trimmed();
    if (sql.isEmpty())
        return;

    if (!isSafeQuery(sql)) {
        ui->statusLabel->setText(
            tr("Blocked — only read-only SELECT queries are allowed."));
        QMessageBox::warning(this, tr("Query Not Allowed"),
            tr("Only read-only SELECT queries are permitted.\n\n"
               "The following statement types are blocked:\n"
               "INSERT, UPDATE, DELETE, DROP, CREATE, ALTER,\n"
               "ATTACH, DETACH, PRAGMA, VACUUM, and similar."));
        return;
    }

    QElapsedTimer timer;
    timer.start();

    queryModel->setQuery(sql, QSqlDatabase::database());

    if (queryModel->lastError().isValid()) {
        ui->statusLabel->setText(
            tr("Error: %1").arg(queryModel->lastError().text()));
        return;
    }

    // Force-fetch all rows (model is lazily populated by default)
    while (queryModel->canFetchMore())
        queryModel->fetchMore();

    const qint64 elapsed = timer.elapsed();
    const int    rows    = queryModel->rowCount();

    ui->resultsTable->resizeColumnsToContents();
    ui->statusLabel->setText(
        tr("%1 row(s) returned in %2 ms").arg(rows).arg(elapsed));
}

// ---------------------------------------------------------------------------
// Export helpers
// ---------------------------------------------------------------------------

static bool openFileForWrite(QWidget *parent,
                             const QString &caption,
                             const QString &filter,
                             const QString &defaultSuffix,
                             QString &outPath)
{
    QSettings settings;
    const QString lastDir =
        settings.value("SqlQueryDialog/lastDir", QDir::homePath()).toString();

    QString path = QFileDialog::getSaveFileName(parent, caption, lastDir, filter);
    if (path.isEmpty())
        return false;

    if (!defaultSuffix.isEmpty() &&
        !path.endsWith('.' + defaultSuffix, Qt::CaseInsensitive))
        path += '.' + defaultSuffix;

    settings.setValue("SqlQueryDialog/lastDir", QFileInfo(path).absolutePath());
    outPath = path;
    return true;
}

void SqlQueryDialog::exportAsTxt()
{
    FCT_IDENTIFICATION;

    if (queryModel->rowCount() == 0) {
        QMessageBox::information(this, tr("Export"),
            tr("No results to export — run a query first."));
        return;
    }

    QString filename;
    if (!openFileForWrite(this, tr("Export as TXT"),
                          tr("Text Files (*.txt);;All Files (*)"), "txt", filename))
        return;

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export Error"),
            tr("Cannot open file for writing:\n%1").arg(file.errorString()));
        return;
    }

    QTextStream out(&file);
    const int cols = queryModel->columnCount();

    // Header row
    QStringList headers;
    for (int c = 0; c < cols; ++c)
        headers << queryModel->headerData(c, Qt::Horizontal).toString();
    out << headers.join("\t") << "\n";

    // Data rows
    for (int r = 0; r < queryModel->rowCount(); ++r) {
        QStringList row;
        for (int c = 0; c < cols; ++c)
            row << queryModel->data(queryModel->index(r, c)).toString();
        out << row.join("\t") << "\n";
    }

    ui->statusLabel->setText(
        tr("Exported %1 row(s) to %2")
        .arg(queryModel->rowCount())
        .arg(QFileInfo(filename).fileName()));
}

void SqlQueryDialog::exportAsCsv()
{
    FCT_IDENTIFICATION;

    if (queryModel->rowCount() == 0) {
        QMessageBox::information(this, tr("Export"),
            tr("No results to export — run a query first."));
        return;
    }

    QString filename;
    if (!openFileForWrite(this, tr("Export as CSV"),
                          tr("CSV Files (*.csv);;All Files (*)"), "csv", filename))
        return;

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export Error"),
            tr("Cannot open file for writing:\n%1").arg(file.errorString()));
        return;
    }

    QTextStream out(&file);
    const int cols = queryModel->columnCount();

    auto csvEscape = [](const QString &s) -> QString {
        if (s.contains(',') || s.contains('"') || s.contains('\n') || s.contains('\r'))
            return '"' + QString(s).replace('"', "\"\"") + '"';
        return s;
    };

    // Header row
    QStringList headers;
    for (int c = 0; c < cols; ++c)
        headers << csvEscape(queryModel->headerData(c, Qt::Horizontal).toString());
    out << headers.join(",") << "\n";

    // Data rows
    for (int r = 0; r < queryModel->rowCount(); ++r) {
        QStringList row;
        for (int c = 0; c < cols; ++c)
            row << csvEscape(queryModel->data(queryModel->index(r, c)).toString());
        out << row.join(",") << "\n";
    }

    ui->statusLabel->setText(
        tr("Exported %1 row(s) to %2")
        .arg(queryModel->rowCount())
        .arg(QFileInfo(filename).fileName()));
}

void SqlQueryDialog::exportAsAdif()
{
    FCT_IDENTIFICATION;

    if (queryModel->rowCount() == 0) {
        QMessageBox::information(this, tr("Export"),
            tr("No results to export — run a query first."));
        return;
    }

    // Locate an 'id' column in the result set
    int idCol = -1;
    for (int c = 0; c < queryModel->columnCount(); ++c) {
        if (queryModel->headerData(c, Qt::Horizontal)
                .toString().compare("id", Qt::CaseInsensitive) == 0) {
            idCol = c;
            break;
        }
    }

    if (idCol < 0) {
        QMessageBox::warning(this, tr("ADIF Export"),
            tr("ADIF export requires the query to include the contacts 'id' column.\n\n"
               "Example:\n"
               "  SELECT id, callsign FROM contacts WHERE ..."));
        return;
    }

    // Collect all contact IDs from the result
    QStringList idStrings;
    for (int r = 0; r < queryModel->rowCount(); ++r) {
        const QVariant id = queryModel->data(queryModel->index(r, idCol));
        if (id.isValid() && !id.isNull())
            idStrings << id.toString();
    }

    if (idStrings.isEmpty()) {
        QMessageBox::information(this, tr("ADIF Export"),
            tr("No valid contact IDs found in the result set."));
        return;
    }

    // Fetch the full contact records for those IDs
    const QString fetchSql =
        QString("SELECT * FROM contacts WHERE id IN (%1) ORDER BY start_time ASC")
        .arg(idStrings.join(','));

    QSqlQuery fetchQ;
    if (!fetchQ.exec(fetchSql)) {
        QMessageBox::warning(this, tr("ADIF Export"),
            tr("Failed to retrieve contact records:\n%1")
            .arg(fetchQ.lastError().text()));
        return;
    }

    QList<QSqlRecord> records;
    while (fetchQ.next())
        records.append(fetchQ.record());

    if (records.isEmpty()) {
        QMessageBox::information(this, tr("ADIF Export"),
            tr("No matching contacts found in the database."));
        return;
    }

    // Hand off to the standard ExportDialog (same path as LogbookWidget right-click)
    ExportDialog dialog(records, this);
    dialog.exec();
}
