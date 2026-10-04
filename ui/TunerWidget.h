#ifndef QLOG_UI_TUNERWIDGET_H
#define QLOG_UI_TUNERWIDGET_H
#include <QWidget>
#include "tuner/TunerController.h"
class QAction;
class QDialog;
class QComboBox;
class QToolButton;
class QLabel;
class QPushButton;
class QSpinBox;
class TunerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TunerWidget(QWidget *parent = nullptr);
    void setConnectAction(QAction *action);
public slots:
    void reloadSettings();
signals:
    void profileChanged();
private:
    void refreshProfiles();
    void updateStatus(const TunerStatus &status);
    QDialog *advancedDialog;
    QComboBox *profiles;
    QToolButton *connectButton;
    QLabel *connectionLabel;
    QLabel *identityLabel;
    QLabel *frequencyLabel;
    QLabel *swrLabel;
    QLabel *bypassSwrLabel;
    QLabel *stateLabel;
    QLabel *faultLabel;
    QLabel *inductanceLabel;
    QLabel *capacitanceLabel;
    QPushButton *powerButton;
    QPushButton *tuneButton;
    QList<QPushButton *> antennaButtons;
    QList<QPushButton *> modeButtons;
    QList<QPushButton *> sideButtons;
    QList<QPushButton *> operationButtons;
    QSpinBox *inductors;
    QSpinBox *capacitors;
};
#endif
