#ifndef QLOG_UI_COMPONENT_FREQQSPINBOX_H
#define QLOG_UI_COMPONENT_FREQQSPINBOX_H

#include <data/Band.h>
#include "ui/component/BaseDoubleSpinBox.h"

class FreqQSpinBox : public BaseDoubleSpinBox
{
    Q_OBJECT

public:
    FreqQSpinBox(QWidget *parent = nullptr);
    virtual ~FreqQSpinBox() {};
    void setSelectionModeEnabled(bool enabled);
    void setDebounceEnabled(bool enabled);
    void setDebounceIntervalMs(int ms);

public slots:
    void loadBands();

signals:
    void debouncedValueChanged(double value);

protected:
    virtual void keyPressEvent(QKeyEvent *event) override;
    virtual void wheelEvent(QWheelEvent *event) override;
    virtual void stepBy(int steps) override;

private slots:
    void onValueChangedImmediate(double v);
    void flushDebounced();

private:
    void increaseByBand();
    void decreaseByBand();
    void maybeSelectAll();

    QList<Band> enabledBands;
    bool selectionModeEnabled;
    QTimer debounceTimer;
    int    debounceMs = 150;
    bool   debounceEnabled = false;
    double pendingValue = 0.0;
    bool   hasPending = false;
};

#endif // QLOG_UI_COMPONENT_FREQQSPINBOX_H
