#ifndef QLOG_UI_SQLQUERYDIALOG_H
#define QLOG_UI_SQLQUERYDIALOG_H

#include <QDialog>
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

private:
    static const QString READ_ONLY_CONNECTION;

    Ui::SqlQueryDialog *ui;
    SqlHighlighter     *highlighter;
    QSqlQueryModel     *queryModel;
    QSortFilterProxyModel *sortProxy;

    void loadSchema();
};

#endif // QLOG_UI_SQLQUERYDIALOG_H
