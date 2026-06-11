#ifndef QLOG_UI_WAVESHAREWIDGET_H
#define QLOG_UI_WAVESHAREWIDGET_H

#include <QByteArray>
#include <QQueue>
#include <QWidget>
#include <functional>

#include "core/WaveshareControl.h"

class QLabel;
class QTimer;

namespace Ui {
class WaveshareWidget;
}

class WaveshareWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WaveshareWidget(QWidget *parent = nullptr);
    ~WaveshareWidget() override;

public slots:
    void reloadSettings();

private:
    struct Row
    {
        WaveshareAction action;
        QLabel *status = nullptr;
    };

    struct PendingRtuRequest
    {
        WaveshareAction action;
        QByteArray pdu;
        std::function<void(bool, const QByteArray &)> callback;
    };

    void rebuild();
    void refreshAll();
    void refreshRelay(int rowIndex);
    void refreshPing(int rowIndex);
    void runRelayAction(int rowIndex);
    void setStatus(Row &row, bool ok, const QString &text);
    void writeRelay(const WaveshareAction &action, bool on);
    void flashRelay(const WaveshareAction &action);
    void readRelay(const WaveshareAction &action, std::function<void(bool, bool)> callback);
    void sendRtuRequest(const WaveshareAction &action,
                        const QByteArray &pdu,
                        std::function<void(bool, const QByteArray &)> callback);
    void processNextRtuRequest();
    QString actionLabel(const WaveshareAction &action) const;

    Ui::WaveshareWidget *ui;
    QList<Row> rows;
    QList<WaveshareAction> actions;
    QQueue<PendingRtuRequest> rtuQueue;
    QTimer *refreshTimer;
    bool rtuRequestActive = false;
    bool shuttingDown = false;
};

#endif // QLOG_UI_WAVESHAREWIDGET_H
