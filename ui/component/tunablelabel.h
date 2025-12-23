#pragma once

#include <QLabel>

class QWheelEvent;
class QMouseEvent;

class TunableLabel : public QLabel
{
    Q_OBJECT
public:
    explicit TunableLabel(QWidget *parent = nullptr);

    void setBaseStepHz(qint64 hz) { m_baseStepHz = hz; }
    qint64 baseStepHz() const { return m_baseStepHz; }

signals:
    // deltaHz can be positive (scroll up) or negative (scroll down)
    void tuneDeltaRequested(qint64 deltaHz);

    // Optional: click/double-click hooks (nice for “set focus”, “open keypad”, etc.)
    void clicked();
    void doubleClicked();

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    qint64 effectiveStepHz(Qt::KeyboardModifiers mods) const;
    qint64 m_baseStepHz = 10; // default 10 Hz per notch
    double m_wheelRemainder = 0.0;   // accumulate trackpad pixels

};
