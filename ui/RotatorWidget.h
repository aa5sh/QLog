#ifndef QLOG_UI_ROTATORWIDGET_H
#define QLOG_UI_ROTATORWIDGET_H

#include <QWidget>
#include <QGraphicsPixmapItem>
#include <QPushButton>
#include "ui/NewContactWidget.h"

namespace Ui {
class RotatorWidget;
}

class QGraphicsScene;
class QGraphicsLineItem;
class QAction;

class RotatorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RotatorWidget(QWidget *parent = nullptr);
    ~RotatorWidget();
    void registerContactWidget(const NewContactWidget*);
    void setConnectAction(QAction *action);

signals:
    void rotProfileChanged();
    void rotUserButtonChanged();
    void bearingRequested(double azimuth);

public slots:
    void setBearing(double);
    void positionChanged(double, double);
    void redrawMap();
    void stationProfileChanged();
    void updateMapViewport();
    void rotProfileComboChanged(QString);
    void rotUserButtonProfileComboChanged(QString);
    void reloadSettings();
    void rotConnected();
    void rotDisconnected();
    void refreshRotProfileCombo();
    void setQSOBearing(double, double);
    void shortcutProfileIncrease();
    void shortcutProfileDecrease();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void userButton1();
    void userButton2();
    void userButton3();
    void userButton4();
    void gotoPosition();
    void qsoBearingLP();
    void qsoBearingSP();
    void setRequestedAz(double);

private:

    void refreshRotUserButtonProfileCombo();
    void refreshRotUserButtons();
    void setUserButtonDesc(QPushButton *button, const QString&, const double);
    double getQSOBearing();
    void shortcutComboMove(int);
    void setMapZoom(double zoom);
    void setBearingFromMapPosition(const QPoint &position);
    QPoint mousePosition(const QMouseEvent *event) const;
    QPointF currentMapCenter() const;
    void saveMapCenter();
    void updateMapTransform();

    QGraphicsLineItem* antennaNeedle;
    QGraphicsLineItem* requestedAzimuthNeedle;
    QGraphicsLineItem* QSOAzimuthNeedle;
    QGraphicsScene* compassScene;
    QString mapLocator;
    Ui::RotatorWidget *ui;
    double antennaAzimuth;
    double requestedAzimuth;
    double qsoAzimuth;
    double mapZoom;
    QPointF mapCenter;
    QPoint mapPressPosition;
    bool mapDragged;
    const NewContactWidget *contact;

    const int MAP_RESOLUTION = 1000;
    const float GLOBE_RADIUS = 100.0;
    const double AZIMUTH_DEAD_BAND = 2.0;
    const double MAP_ZOOM_MIN = 1.0;
    const double MAP_ZOOM_MAX = 5.0;
    const double MAP_ZOOM_STEP = 0.25;
};

#endif // QLOG_UI_ROTATORWIDGET_H
