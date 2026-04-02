#ifndef QLOG_UI_SQLQUERYDIALOG_H
#define QLOG_UI_SQLQUERYDIALOG_H

#include <QDialog>
#include <QCompleter>
#include <QStringListModel>
#include <QSqlQueryModel>
#include <QSortFilterProxyModel>

namespace Ui {
class SqlQueryDialog;
}

class SqlHighlighter;

class SqlQueryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SqlQueryDialog(QWidget *parent = nullptr);
    ~SqlQueryDialog();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void openQuery();
    void saveQuery();
    void runQuery();
    void exportAsTxt();
    void exportAsCsv();
    void exportAsAdif();
    void insertCompletion(const QString &completion);
    void onTextChanged();

private:
    Ui::SqlQueryDialog *ui;
    SqlHighlighter     *highlighter;
    QCompleter         *completer;
    QStringListModel   *completerModel;
    QSqlQueryModel     *queryModel;
    QSortFilterProxyModel *sortProxy;

    void loadSchema();
    bool isSafeQuery(const QString &sql) const;
    QString currentWord() const;
    void showCompleterPopup();
};

#endif // QLOG_UI_SQLQUERYDIALOG_H
