#include "ui/component/tunablelabel.h"

#include <QWheelEvent>
#include <QMouseEvent>

TunableLabel::TunableLabel(QWidget *parent)
    : QLabel(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    m_wheelRemainder = 0.0;
}

qint64 TunableLabel::effectiveStepHz(Qt::KeyboardModifiers mods) const
{
    qint64 step = m_baseStepHz;

    if (mods.testFlag(Qt::ControlModifier)) step *= 5;
    if (mods.testFlag(Qt::ShiftModifier))   step *= 5;   // Ctrl+Shift => ×100
    if (mods.testFlag(Qt::AltModifier))     step *= 5;   // Alt adds another ×10

    return step;
}

void TunableLabel::wheelEvent(QWheelEvent *event)
{
    const QPoint pixel = event->pixelDelta();
    const QPoint angle = event->angleDelta();

    int steps = 0;

    if (!pixel.isNull()) {
        static constexpr double kPixelsPerStep = 20.0; // tune feel
        m_wheelRemainder += pixel.y();
        steps = static_cast<int>(m_wheelRemainder / kPixelsPerStep);
        if (steps != 0)
            m_wheelRemainder -= steps * kPixelsPerStep;
    } else if (!angle.isNull()) {
        steps = angle.y() / 120;
    }

    if (steps != 0) {
        const qint64 stepHz  = effectiveStepHz(event->modifiers());
        const qint64 deltaHz = static_cast<qint64>(steps) * stepHz; // +up/-down
        emit tuneDeltaRequested(deltaHz);
        event->accept();
        return;
    }

    QLabel::wheelEvent(event);
}


void TunableLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked();
        event->accept();
        return;
    }
    QLabel::mousePressEvent(event);
}

void TunableLabel::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit doubleClicked();
        event->accept();
        return;
    }
    QLabel::mouseDoubleClickEvent(event);
}
