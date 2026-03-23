#ifndef QLOG_UI_CABRILLOTEMPLATEDIALOG_H
#define QLOG_UI_CABRILLOTEMPLATEDIALOG_H

#include <QDialog>
#include <QList>
#include <QMap>
#include <QStringList>

#include "logformat/CabrilloFormat.h"

class QSqlQuery;

namespace Ui {
class CabrilloTemplateDialog;
}

class CabrilloTemplateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CabrilloTemplateDialog(QWidget *parent = nullptr);
    ~CabrilloTemplateDialog();

    void selectTemplate(int templateId);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void accept() override;
    void templateSelectionChanged();
    void newTemplate();
    void copyTemplate();
    void deleteTemplate();
    void addColumn();
    void removeColumn();
    void moveColumnUp();
    void moveColumnDown();

private:
    struct TemplateData
    {
        int id;
        QString name;
        QString contestName;
        QString defaultMode;
        QList<CabrilloFormat::ColumnDef> columns;
        bool isNew;
        bool isModified;
        bool isDeleted;
    };

    void loadTemplatesFromDB();
    void populateTemplateList();
    void showTemplateEditor(int index);
    void saveCurrentTemplate();
    void populateContestCombo();
    void populateColumnsTable(const QList<CabrilloFormat::ColumnDef> &columns);
    void setupColumnRow(int row, const CabrilloFormat::ColumnDef &col);
    void installWheelGuard(QWidget *widget);
    QList<CabrilloFormat::ColumnDef> readColumnsFromTable() const;
    void moveColumn(int direction);
    bool saveTemplateColumns(QSqlQuery &query, int templateId,
                             const QList<CabrilloFormat::ColumnDef> &columns);

    Ui::CabrilloTemplateDialog *ui;
    QList<TemplateData> templates;
    int currentTemplateIndex;
    QList<CabrilloFormat::CategoryItem> dbFieldItems;
    QList<CabrilloFormat::CategoryItem> formatterItems;
};

#endif // QLOG_UI_CABRILLOTEMPLATEDIALOG_H
