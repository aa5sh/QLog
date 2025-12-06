#ifndef QLOG_UI_COMPONENT_FREQQSPINBOX_H
#define QLOG_UI_COMPONENT_FREQQSPINBOX_H

#include <data/Band.h>
#include "ui/component/BaseDoubleSpinBox.h"

class FreqQSpinBox : public BaseDoubleSpinBox
{
public:
    FreqQSpinBox(QWidget *parent = nullptr);
    virtual ~FreqQSpinBox() {};

public slots:
    void loadBands();

protected:
    virtual void keyPressEvent(QKeyEvent *event) override;
    virtual void wheelEvent(QWheelEvent *event) override;

private:
    void increaseByBand();
    void decreaseByBand();

    QList<Band> enabledBands;
};

#endif // QLOG_UI_COMPONENT_FREQQSPINBOX_H
