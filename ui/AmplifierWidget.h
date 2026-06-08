#ifndef QLOG_UI_AMPLIFIERWIDGET_H
#define QLOG_UI_AMPLIFIERWIDGET_H

#include <QWidget>

#include "amplifier/AmplifierController.h"

namespace Ui {
class AmplifierWidget;
}

class AmplifierWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AmplifierWidget(QWidget *parent = nullptr);
    ~AmplifierWidget();

signals:
    void profileChanged();

public slots:
    void reloadSettings();

private slots:
    void refreshProfiles();
    void setProfile(const QString &profileName);
    void amplifierConnected();
    void amplifierDisconnected();
    void statusChanged(const AmplifierStatus &status);

private:
    static QString bandName(int band);
    static QString ledStyle(const QString &color);
    void updateControlsEnabled(bool enabled);
    void resetStatus();

    Ui::AmplifierWidget *ui;
};

#endif // QLOG_UI_AMPLIFIERWIDGET_H
