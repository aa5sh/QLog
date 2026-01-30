#ifndef QLOG_UI_DXFILTER_H
#define QLOG_UI_DXFILTER_H

#include <QDialog>
#include <QCheckBox>

namespace Ui {
class DxFilterDialog;
}

class DxFilterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DxFilterDialog(QWidget *parent = nullptr);
    ~DxFilterDialog();
    void accept() override;

private:
    Ui::DxFilterDialog *ui;
    QList<QCheckBox*> memberListCheckBoxes;
    const quint8 MAXCOLUMNS = 6;
    void generateMembershipCheckboxes();

};

#endif // QLOG_UI_DXFILTER_H
