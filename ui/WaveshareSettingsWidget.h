#ifndef QLOG_UI_WAVESHARESETTINGSWIDGET_H
#define QLOG_UI_WAVESHARESETTINGSWIDGET_H

#include <QWidget>

#include "core/WaveshareControl.h"

namespace Ui {
class WaveshareSettingsWidget;
}

class WaveshareSettingsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WaveshareSettingsWidget(QWidget *parent = nullptr);
    ~WaveshareSettingsWidget() override;

    void loadSettings();
    void saveSettings() const;

private slots:
    void addAction();
    void removeAction();

private:
    void appendActionRow(const WaveshareAction &action);
    WaveshareAction actionFromRow(int row) const;

    Ui::WaveshareSettingsWidget *ui;
};

#endif // QLOG_UI_WAVESHARESETTINGSWIDGET_H
