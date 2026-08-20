#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QWheelEvent>
#include <cmath>
#include "rotator/Rotator.h"
#include "RotatorWidget.h"
#include "ui_RotatorWidget.h"
#include "core/debug.h"
#include "core/LogParam.h"
#include "data/Gridsquare.h"
#include "data/StationProfile.h"
#include "data/RotUsrButtonsProfile.h"

MODULE_IDENTIFICATION("qlog.ui.rotatorwidget");

RotatorWidget::RotatorWidget(QWidget *parent) :
    QWidget(parent),
    antennaNeedle(nullptr),
    requestedAzimuthNeedle(nullptr),
    QSOAzimuthNeedle(nullptr),
    compassScene(nullptr),
    ui(new Ui::RotatorWidget),
    antennaAzimuth(0.0),
    requestedAzimuth(qQNaN()),
    qsoAzimuth(qQNaN()),
    mapZoom(1.0),
    mapCenter(0.0, 0.0),
    mapPressPosition(-1, -1),
    mapDragged(false),
    contact(nullptr)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    mapZoom = LogParam::getRotatorMapZoom();
    if ( !qIsFinite(mapZoom) )
        mapZoom = MAP_ZOOM_MIN;
    else
        mapZoom = qBound(MAP_ZOOM_MIN, mapZoom, MAP_ZOOM_MAX);

    mapCenter = LogParam::getRotatorMapCenter();
    if ( !qIsFinite(mapCenter.x()) || !qIsFinite(mapCenter.y()) )
        mapCenter = QPointF();

    ui->compassView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->compassView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->compassView->setDragMode(QGraphicsView::ScrollHandDrag);
    ui->compassView->viewport()->installEventFilter(this);

    redrawMap();

    QStringListModel* rotModel = new QStringListModel(this);
    ui->rotProfileCombo->setModel(rotModel);
    refreshRotProfileCombo();

    QStringListModel* userButtonModel = new QStringListModel(this);
    ui->userButtonsProfileCombo->setModel(userButtonModel);
    refreshRotUserButtonProfileCombo();

    QMenu *qsoBearingMenu = new QMenu(this);
    qsoBearingMenu->addAction(ui->actionQSO_SP);
    qsoBearingMenu->addAction(ui->actionQSO_LP);

    ui->qsoBearingButton->setMenu(qsoBearingMenu);
    ui->qsoBearingButton->setDefaultAction(ui->actionQSO_SP);

    rotDisconnected();
}

void RotatorWidget::gotoPosition()
{
    FCT_IDENTIFICATION;

    setBearing(ui->gotoDoubleSpinBox->value());
}

double RotatorWidget::getQSOBearing()
{
    FCT_IDENTIFICATION;

    double qsoBearing = (contact) ? contact->getQSOBearing()
                                  : qQNaN();

    qCDebug(runtime) << "QSO Bearing:" << qsoBearing;

    return qsoBearing;
}

void RotatorWidget::shortcutComboMove(int step)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << step;

    int count = ui->userButtonsProfileCombo->count();
    int currIndex = ui->userButtonsProfileCombo->currentIndex();

    if ( count > 0
         && currIndex != -1
         && step < count )
    {
        int nextIndex = currIndex + step;
        nextIndex += (1 - nextIndex / count) * count;
        ui->userButtonsProfileCombo->setCurrentIndex(nextIndex % count);
    }
}

void RotatorWidget::qsoBearingLP()
{
    FCT_IDENTIFICATION;

    ui->qsoBearingButton->setDefaultAction(ui->actionQSO_LP);

    double qsoBearing = getQSOBearing();

    if ( qIsNaN(qsoBearing) )
        return;

    qsoBearing -= 180;

    if ( qsoBearing < 0 )
        qsoBearing += 360;

    setBearing(qsoBearing);
}

void RotatorWidget::qsoBearingSP()
{
    FCT_IDENTIFICATION;

    ui->qsoBearingButton->setDefaultAction(ui->actionQSO_SP);

    setBearing(getQSOBearing());
}

void RotatorWidget::setRequestedAz(double requestedAz)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << requestedAz;

    if ( qIsNaN(requestedAz) )
        return;

    requestedAzimuth = requestedAz;
    if ( requestedAzimuthNeedle )
    {
        requestedAzimuthNeedle->show();
        requestedAzimuthNeedle->setRotation(requestedAz);
    }
}

void RotatorWidget::setBearing(double in_azimuth)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << in_azimuth;

    if ( qIsNaN(in_azimuth) || !ui->gotoDoubleSpinBox->isEnabled() )
        return;

    setRequestedAz(in_azimuth);
    Rotator::instance()->setPosition(in_azimuth, 0);
}

void RotatorWidget::positionChanged(double in_azimuth, double in_elevation)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << in_azimuth <<in_elevation;

    if ( !qIsFinite(in_azimuth) )
    {
        qWarning() << "Invalid value in RotatorWidget::positionChanged:" << in_azimuth;
        return;
    }

    antennaAzimuth = (in_azimuth < 0.0 ) ? 360.0 + in_azimuth : in_azimuth;
    if ( antennaNeedle) antennaNeedle->setRotation(antennaAzimuth);
    ui->gotoDoubleSpinBox->blockSignals(true);
    ui->gotoDoubleSpinBox->setValue(antennaAzimuth);
    ui->gotoDoubleSpinBox->blockSignals(false);

    if ( qIsNaN(requestedAzimuth) )
        return;

    if ( !qIsFinite(requestedAzimuth) )
    {
        qWarning() << "Invalid value in RotatorWidget::positionChanged:" << requestedAzimuth;
        return;
    }

    const double azimuthDifference = qAbs(std::remainder(requestedAzimuth - in_azimuth, 360.0));

    if ( azimuthDifference <= AZIMUTH_DEAD_BAND )
    {
        requestedAzimuth = qQNaN();
        if ( requestedAzimuthNeedle )
            requestedAzimuthNeedle->hide();
    }
}

void RotatorWidget::setQSOBearing(double , double )
{
    FCT_IDENTIFICATION;

    if ( !QSOAzimuthNeedle )
        return;

    qsoAzimuth = getQSOBearing();

    qCDebug(runtime) << qsoAzimuth;

    if ( !qIsNaN(qsoAzimuth) )
    {
        QSOAzimuthNeedle->show();
        QSOAzimuthNeedle->setRotation(qsoAzimuth);
    }
    else
        QSOAzimuthNeedle->hide();
}

void RotatorWidget::shortcutProfileIncrease()
{
    FCT_IDENTIFICATION;

    shortcutComboMove(1);
}

void RotatorWidget::shortcutProfileDecrease()
{
    FCT_IDENTIFICATION;

     shortcutComboMove(-1);
}

bool RotatorWidget::eventFilter(QObject *watched, QEvent *event)
{
    if ( watched != ui->compassView->viewport() )
        return QWidget::eventFilter(watched, event);

    if ( event->type() == QEvent::Wheel )
    {
        QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
        const int delta = wheelEvent->angleDelta().y() != 0
                              ? wheelEvent->angleDelta().y()
                              : wheelEvent->pixelDelta().y();

        if ( delta != 0 )
        {
            setMapZoom(mapZoom + (delta > 0 ? MAP_ZOOM_STEP : -MAP_ZOOM_STEP));
            wheelEvent->accept();
            return true;
        }
    }
    else if ( event->type() == QEvent::MouseButtonPress )
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);

        if ( mouseEvent->button() == Qt::LeftButton )
        {
            mapPressPosition = mousePosition(mouseEvent);
            mapDragged = false;
        }
    }
    else if ( event->type() == QEvent::MouseMove )
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);

        if ( mouseEvent->buttons().testFlag(Qt::LeftButton)
             && mapPressPosition.x() >= 0
             && (mousePosition(mouseEvent) - mapPressPosition).manhattanLength()
                >= QApplication::startDragDistance() )
            mapDragged = true;
    }
    else if ( event->type() == QEvent::MouseButtonRelease )
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);

        if ( mouseEvent->button() == Qt::LeftButton )
        {
            if ( mapPressPosition.x() >= 0 )
            {
                if ( mapDragged )
                    saveMapCenter();
                else
                    setBearingFromMapPosition(mousePosition(mouseEvent));
            }

            mapPressPosition = QPoint(-1, -1);
            mapDragged = false;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void RotatorWidget::setMapZoom(double zoom)
{
    const double boundedZoom = qBound(MAP_ZOOM_MIN, zoom, MAP_ZOOM_MAX);

    if ( qFuzzyCompare(mapZoom, boundedZoom) )
        return;

    mapCenter = currentMapCenter();
    mapZoom = boundedZoom;
    updateMapTransform();
    LogParam::setRotatorMapZoom(mapZoom);
    LogParam::setRotatorMapCenter(mapCenter);
}

void RotatorWidget::setBearingFromMapPosition(const QPoint &position)
{
    const QPointF clickPos = ui->compassView->mapToScene(position);
    const qreal dx = clickPos.x();
    const qreal dy = -clickPos.y();

    if ( qSqrt(qPow(dx, 2) + qPow(dy, 2)) > GLOBE_RADIUS )
        return;

    double angle = qRadiansToDegrees(qAtan2(dx, dy));

    if ( angle < 0 )
        angle += 360;

    setBearing(std::round(angle));
}

QPoint RotatorWidget::mousePosition(const QMouseEvent *event) const
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    return event->position().toPoint();
#else
    return event->pos();
#endif
}

QPointF RotatorWidget::currentMapCenter() const
{
    return ui->compassView->mapToScene(ui->compassView->viewport()->rect().center());
}

void RotatorWidget::saveMapCenter()
{
    mapCenter = currentMapCenter();
    LogParam::setRotatorMapCenter(mapCenter);
}

void RotatorWidget::updateMapTransform()
{
    if ( !compassScene || compassScene->sceneRect().isEmpty() )
        return;

    const QRectF sceneRect = compassScene->sceneRect();
    mapCenter.setX(qBound(sceneRect.left(), mapCenter.x(), sceneRect.right()));
    mapCenter.setY(qBound(sceneRect.top(), mapCenter.y(), sceneRect.bottom()));

    ui->compassView->resetTransform();
    ui->compassView->fitInView(sceneRect, Qt::KeepAspectRatio);
    ui->compassView->scale(mapZoom, mapZoom);
    ui->compassView->centerOn(mapCenter);
    mapCenter = currentMapCenter();
}

void RotatorWidget::showEvent(QShowEvent* event)
{
    FCT_IDENTIFICATION;

    updateMapTransform();
    QWidget::showEvent(event);
}

void RotatorWidget::resizeEvent(QResizeEvent* event)
{
    FCT_IDENTIFICATION;

    updateMapTransform();
    QWidget::resizeEvent(event);
}

void RotatorWidget::userButton1()
{
    FCT_IDENTIFICATION;

    double bearing = RotUsrButtonsProfilesManager::instance()->getCurProfile1().bearings[0];
    if ( bearing >= 0 ) setBearing(bearing);
}

void RotatorWidget::userButton2()
{
    FCT_IDENTIFICATION;

    double bearing = RotUsrButtonsProfilesManager::instance()->getCurProfile1().bearings[1];
    if ( bearing >= 0 ) setBearing(bearing);
}

void RotatorWidget::userButton3()
{
    FCT_IDENTIFICATION;

    double bearing = RotUsrButtonsProfilesManager::instance()->getCurProfile1().bearings[2];
    if ( bearing >= 0 ) setBearing(bearing);
}

void RotatorWidget::userButton4()
{
    FCT_IDENTIFICATION;

    double bearing = RotUsrButtonsProfilesManager::instance()->getCurProfile1().bearings[3];
    if ( bearing >= 0 ) setBearing(bearing);
}

void RotatorWidget::refreshRotProfileCombo()
{
    FCT_IDENTIFICATION;

    ui->rotProfileCombo->blockSignals(true);

    QStringList currProfiles = RotProfilesManager::instance()->profileNameList();
    QStringListModel* model = dynamic_cast<QStringListModel*>(ui->rotProfileCombo->model());

    model->setStringList(currProfiles);

    if ( RotProfilesManager::instance()->getCurProfile1().profileName.isEmpty()
         && currProfiles.count() > 0 )
    {
        /* changing profile from empty to something */
        ui->rotProfileCombo->setCurrentText(currProfiles.first());
        rotProfileComboChanged(currProfiles.first());
    }
    else
    {
        /* no profile change, just refresh the combo and preserve current profile */
        ui->rotProfileCombo->setCurrentText(RotProfilesManager::instance()->getCurProfile1().profileName);
    }

    ui->rotProfileCombo->blockSignals(false);
}

void RotatorWidget::refreshRotUserButtonProfileCombo()
{
    FCT_IDENTIFICATION;

    ui->userButtonsProfileCombo->blockSignals(true);

    RotUsrButtonsProfilesManager *buttonManager =  RotUsrButtonsProfilesManager::instance();

    QStringList currProfiles = buttonManager->profileNameList();
    QStringListModel* model = dynamic_cast<QStringListModel*>(ui->userButtonsProfileCombo->model());

    model->setStringList(currProfiles);

    if ( buttonManager->getCurProfile1().profileName.isEmpty()
         && currProfiles.count() > 0 )
    {
        /* changing profile from empty to something */
        ui->userButtonsProfileCombo->setCurrentText(currProfiles.first());
    }
    else
    {
        /* no profile change, just refresh the combo and preserve current profile */
        ui->userButtonsProfileCombo->setCurrentText(buttonManager->getCurProfile1().profileName);
    }

    rotUserButtonProfileComboChanged(ui->userButtonsProfileCombo->currentText());

    ui->userButtonsProfileCombo->blockSignals(false);
}

void RotatorWidget::refreshRotUserButtons()
{
    FCT_IDENTIFICATION;

    RotUsrButtonsProfile profile = RotUsrButtonsProfilesManager::instance()->getCurProfile1();

    setUserButtonDesc(ui->userButton_1,
                      profile.shortDescs[0],
                      profile.bearings[0]);
    setUserButtonDesc(ui->userButton_2,
                      profile.shortDescs[1],
                      profile.bearings[1]);
    setUserButtonDesc(ui->userButton_3,
                      profile.shortDescs[2],
                      profile.bearings[2]);
    setUserButtonDesc(ui->userButton_4,
                      profile.shortDescs[3],
                      profile.bearings[3]);
}

void RotatorWidget::setUserButtonDesc(QPushButton *button,
                                      const QString &desc,
                                      const double value)
{
    FCT_IDENTIFICATION;

    if ( value >= 0 )
    {
        button->setText(desc);
        button->setEnabled(true);
    }
    else
    {
        button->setText("");
        button->setEnabled(false);
    }
}

void RotatorWidget::redrawMap()
{
    FCT_IDENTIFICATION;

    if ( !ui )
        return;

    const QString locator = StationProfilesManager::instance()->getCurProfile1().locator;

    if ( compassScene && mapLocator == locator )
    {
        updateMapTransform();
        return;
    }

    mapLocator = locator;

    if ( compassScene )
        compassScene->deleteLater();

    antennaNeedle = nullptr;
    requestedAzimuthNeedle = nullptr;
    QSOAzimuthNeedle = nullptr;

    compassScene = new QGraphicsScene(this);
    ui->compassView->setScene(compassScene);
    ui->compassView->setStyleSheet("background-color: transparent;");

    const Gridsquare myGrid(locator);

    double lat = myGrid.getLatitude();
    double lon = myGrid.getLongitude();

    if ( qIsNaN(lat) || qIsNaN(lon) )
        return;

    QImage source(":/res/map/nasabluemarble.jpg");
    QImage map(MAP_RESOLUTION, MAP_RESOLUTION, QImage::Format_ARGB32);

    // transform image to azimuthal map
    const int mapWidth = map.width();
    const int mapHeight = map.height();
    const int srcWidth = source.width();
    const int srcHeight = source.height();

    const double lambda0 = (lon / 180.0) * M_PI;
    const double phi1 = - (lat / 90.0) * (0.5 * M_PI);
    const double sinPhi1 = sin(phi1);
    const double cosPhi1 = cos(phi1);
    const double twoPI = 2.0 * M_PI;

    for (int x = 0; x < mapWidth; x++)
    {
        double x2 = twoPI * (static_cast<double>(x) / static_cast<double>(mapWidth) - 0.5);

        for (int y = 0; y < mapHeight; y++)
        {
            double y2 = twoPI * (static_cast<double>(y) / static_cast<double>(mapHeight) - 0.5);
            double c = sqrt(x2 * x2 + y2 * y2);

            if ( c < M_PI )
            {
                double phi = phi1;
                double lambda = lambda0;

                if ( c != 0.0 )
                {
                    double sinC = sin(c);
                    double cosC = cos(c);
                    phi = asin(cosC * sinPhi1 + (y2 * sinC * cosPhi1) / c);
                    lambda = lambda0 + atan2(x2 * sinC, c * cosPhi1 * cosC - y2 * sinPhi1 * sinC);
                }

                double s = (lambda / twoPI) + 0.5;
                double t = (phi / M_PI) + 0.5;

                int x3 = static_cast<int>(s * srcWidth);
                int y3 = static_cast<int>(t * srcHeight);

                x3 = (x3 % srcWidth + srcWidth) % srcWidth;
                y3 = (y3 % srcHeight + srcHeight) % srcHeight;

                map.setPixelColor(x, y, source.pixelColor(x3, y3));
            }
            else
                map.setPixelColor(x, y, QColor(0, 0, 0, 0));
        }
    }

    // draw azimuthal map
    QGraphicsPixmapItem *pixMapItem = compassScene->addPixmap(QPixmap::fromImage(map));
    pixMapItem->moveBy(-MAP_RESOLUTION/2, -MAP_RESOLUTION/2);
    pixMapItem->setTransformOriginPoint(MAP_RESOLUTION/2, MAP_RESOLUTION/2);
    pixMapItem->setScale(GLOBE_RADIUS * 2/MAP_RESOLUTION);

    // circle around the globe - globe "antialiasing"
    compassScene->addEllipse(-100, -100, GLOBE_RADIUS * 2, GLOBE_RADIUS * 2,
                             QPen(QColor(100, 100, 100)),
                             QBrush(QColor(0, 0, 0),
                             Qt::NoBrush));

    // point in the middle of globe
    compassScene->addEllipse(-1, -1, 2, 2,
                             QPen(Qt::NoPen),
                             QBrush(QColor(0, 0, 0),
                             Qt::SolidPattern));

    // draw needles
    QPen needlePen(QColor(255, 255, 255), 2.0, Qt::SolidLine, Qt::RoundCap);
    needlePen.setCosmetic(true);
    requestedAzimuthNeedle = compassScene->addLine(0, 0, 0, -90, needlePen);
    if ( !qIsNaN(requestedAzimuth) )
        requestedAzimuthNeedle->setRotation(requestedAzimuth);
    else
        requestedAzimuthNeedle->hide();

    needlePen.setColor(QColor(255, 191, 0));
    needlePen.setWidthF(4.0);
    antennaNeedle = compassScene->addLine(0, 0, 0, -90, needlePen);
    antennaNeedle->setRotation(antennaAzimuth);
    if ( !ui->gotoButton->isEnabled() )
        antennaNeedle->hide();

    needlePen.setColor(QColor(255, 0, 255));
    needlePen.setWidthF(2.0);
    QSOAzimuthNeedle = compassScene->addLine(0, 0, 0, -90, needlePen);

    setQSOBearing(qQNaN(), qQNaN()); // only call the function; input parameters are ignored
    updateMapTransform();
}

void RotatorWidget::stationProfileChanged()
{
    mapCenter = QPointF();
    LogParam::setRotatorMapCenter(mapCenter);
    redrawMap();
}

void RotatorWidget::updateMapViewport()
{
    FCT_IDENTIFICATION;

    ui->compassView->viewport()->update();
}

void RotatorWidget::rotProfileComboChanged(QString profileName)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << profileName;

    RotProfilesManager::instance()->setCurProfile1(profileName);

    emit rotProfileChanged();
}

void RotatorWidget::rotUserButtonProfileComboChanged(QString profileName)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << profileName;

    RotUsrButtonsProfilesManager::instance()->setCurProfile1(profileName);

    refreshRotUserButtons();

    emit rotUserButtonChanged();
}

void RotatorWidget::reloadSettings()
{
    FCT_IDENTIFICATION;

    refreshRotProfileCombo();
    refreshRotUserButtonProfileCombo();
}

void RotatorWidget::rotConnected()
{
    FCT_IDENTIFICATION;

    ui->connectButton->setStyleSheet("QToolButton {background-color: green}");
    ui->gotoDoubleSpinBox->setEnabled(true);
    ui->gotoButton->setEnabled(true);
    ui->qsoBearingButton->setEnabled(true);
    ui->userButtonsProfileCombo->setEnabled(true);
    ui->leftButton->setVisible(ui->userButtonsProfileCombo->count()>1);
    ui->rightButton->setVisible(ui->userButtonsProfileCombo->count()>1);
    refreshRotUserButtons();
    if ( antennaNeedle ) antennaNeedle->show();
}

void RotatorWidget::rotDisconnected()
{
    FCT_IDENTIFICATION;

    ui->connectButton->setStyleSheet(QString());
    ui->gotoDoubleSpinBox->setEnabled(false);
    ui->gotoButton->setEnabled(false);
    ui->qsoBearingButton->setEnabled(false);
    ui->userButtonsProfileCombo->setEnabled(false);
    ui->userButton_1->setEnabled(false);
    ui->userButton_2->setEnabled(false);
    ui->userButton_3->setEnabled(false);
    ui->userButton_4->setEnabled(false);
    ui->leftButton->setVisible(false);
    ui->rightButton->setVisible(false);
    if ( antennaNeedle ) antennaNeedle->hide();
    if ( requestedAzimuthNeedle) requestedAzimuthNeedle->hide();
    requestedAzimuth = qQNaN();
}

RotatorWidget::~RotatorWidget()
{
    delete ui;
}

void RotatorWidget::setConnectAction(QAction *action)
{
    ui->connectButton->setDefaultAction(action);
}

void RotatorWidget::registerContactWidget(const NewContactWidget *contactWidget)
{
    FCT_IDENTIFICATION;

    contact = contactWidget;
}
