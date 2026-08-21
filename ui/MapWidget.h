#ifndef QLOG_UI_MAPWIDGET_H
#define QLOG_UI_MAPWIDGET_H

#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>

namespace Ui {
class MapWidget;
}

class MapWidget : public QGraphicsView
{
    Q_OBJECT

public:
    explicit MapWidget(QWidget* parent = nullptr);
    ~MapWidget();

public slots:
    void setTarget(double lat, double lon);
    void clear();
    void redraw();
    void stationProfileChanged();

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void redrawNightOverlay();
    void drawPoint(const QPoint &point);
    void drawLine(const QPoint &pointA, const QPoint &pointB);
    void fitQSOPath(const QPoint &qth, const QPoint &target);

    void pointToRad(const QPoint &point, double& lat, double& lon);
    void pointToCoord(const QPoint &point, double& lat, double& lon);
    QPoint radToPoint(const double lat, const double lon);
    QPoint coordToPoint(const double lat, const double lon);
    QPointF stationMapCenter();
    QPointF currentMapCenter() const;
    void setMapZoom(double zoom);
    void updateMapTransform();

    int sunSize = 20;

    QGraphicsPixmapItem* nightOverlay;
    QList<QGraphicsItem*> items;
    QGraphicsEllipseItem* sunItem;
    QGraphicsPathItem* terminatorItem;
    QGraphicsScene* scene;
    double mapZoom;
    QPointF mapCenter;
    double targetLatitude;
    double targetLongitude;

    const double MAP_ZOOM_MIN = 1.0;
    const double MAP_ZOOM_MAX = 5.0;
    const double MAP_ZOOM_STEP = 0.25;
};

#endif // QLOG_UI_MAPWIDGET_H
