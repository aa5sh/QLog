#ifndef QLOG_UI_MODESELECTIONCONTROLLER_H
#define QLOG_UI_MODESELECTIONCONTROLLER_H

#include <QObject>
#include <QComboBox>
#include <QSqlRecord>
#include <QSqlTableModel>
#include <QStringListModel>

class ModeSelectionController : public QObject
{
    Q_OBJECT

public:
    ModeSelectionController(QComboBox *modeCombo,
                            QComboBox *submodeCombo,
                            bool enabledOnly,
                            bool selectFirstSubmode,
                            bool hideSubmodeWhenEmpty,
                            bool fallbackToFirstMode,
                            QObject *parent = nullptr);

    void applyCurrentMode();
    void reloadModel();

signals:
    void defaultReportChanged(const QString &report);

private:
    void applySubmodes(const QStringList &submodeList);
    QSqlRecord currentRecord() const;

    QComboBox *modeCombo;
    QComboBox *submodeCombo;
    QSqlTableModel *modeModel;
    QStringListModel *submodeModel;
    bool enabledOnly;
    bool selectFirstSubmode;
    bool hideSubmodeWhenEmpty;
    bool fallbackToFirstMode;
    int nameColumn;
};

#endif // QLOG_UI_MODESELECTIONCONTROLLER_H
