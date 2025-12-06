#ifndef BASEDOUBLESPINBOX_H
#define BASEDOUBLESPINBOX_H

#include <QDoubleSpinBox>

class BaseDoubleSpinBox : public QDoubleSpinBox
{
public:
    BaseDoubleSpinBox(QWidget *parent = nullptr);
    virtual ~BaseDoubleSpinBox(){};

protected:
    virtual void keyPressEvent(QKeyEvent *event) override;
};

#endif // BASEDOUBLESPINBOX_H
