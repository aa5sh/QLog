#include <QKeyEvent>
#include "BaseDoubleSpinBox.h"

BaseDoubleSpinBox::BaseDoubleSpinBox(QWidget *parent) :
    QDoubleSpinBox(parent)
{
    setLocale(QLocale::C);
}

void BaseDoubleSpinBox::keyPressEvent(QKeyEvent *event)
{
    if ( event->key() == ',' ) return; // supress

    QDoubleSpinBox::keyPressEvent(event);
}
