#ifndef QLOG_UI_STEPPIRWIDGET_H
#define QLOG_UI_STEPPIRWIDGET_H

#include <QWidget>

#include "antenna/SteppirController.h"
#include "ui/component/ShutdownAwareWidget.h"

namespace Ui {
class SteppirWidget;
}

class QRadioButton;

class SteppirWidget : public QWidget, public ShutdownAwareWidget
{
    Q_OBJECT

public:
    explicit SteppirWidget(QWidget *parent = nullptr);
    ~SteppirWidget() override;
    void finalizeBeforeAppExit() override;

public slots:
    void reloadSettings();
    void refreshProfileCombo();
    void connected();
    void disconnected();
    void updateState();

signals:
    void profileChanged();

private slots:
    void profileComboChanged(const QString &profileName);
    void directionClicked();

private:
    QRadioButton *directionButton(SteppirController::Direction direction) const;

    Ui::SteppirWidget *ui;
};

#endif // QLOG_UI_STEPPIRWIDGET_H
