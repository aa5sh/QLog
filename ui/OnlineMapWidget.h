#ifndef QLOG_UI_ONLINEMAPWIDGET_H
#define QLOG_UI_ONLINEMAPWIDGET_H

#include <QWidget>
#include <QWebEngineView>
#include <QScopedPointer>
#include "ui/MapPageController.h"
#include "core/PropConditions.h"
#include "rig/Rig.h"
#include "ui/NewContactWidget.h"
#include "service/kstchat/KSTChat.h"
#include "data/WsjtxEntry.h"
#include "service/MapSpotProvider.h"

namespace Ui {
class OnlineMapWidget;
}

class OnlineMapWidget : public QWebEngineView
{
    Q_OBJECT

public:
    explicit OnlineMapWidget(QWidget* parent = nullptr);
    ~OnlineMapWidget();

    void assignPropConditions(PropConditions *);
    void registerContactWidget(const NewContactWidget*);

signals:
    void chatCallsignPressed(QString);
    void wsjtxCallsignPressed(QString);

public slots:
    void setTarget(double lat, double lon);
    void changeTheme(int, bool isDark);
    void auroraDataUpdate();
    void mufDataUpdate();
    void setIBPBand(VFOID, double, double, double);
    void antPositionChanged(double in_azimuth, double in_elevation);
    void rotConnected();
    void rotDisconnected();
    void flyToMyQTH();
    void drawChatUsers(const QList<KSTUsersInfo> &list);
    void drawWSJTXSpot(const WsjtxEntry &spot);
    void clearWSJTXSpots();
    void mapLayerChanged(const QString &key, bool visible);
    void stationProfileChanged();
    void rigConnectionChanged();
    void drawReceptionSpot(const MapReceptionSpot &spot);

protected slots:
    void finishLoading();
    void chatCallsignTrigger(const QString&);
    void wsjtxCallsignTrigger(const QString&);
    void IBPCallsignTrigger(const QString&, double);

private:

    QScopedPointer<MapPageController> mapController;
    PropConditions *prop_cond;
    const NewContactWidget *contact;
    double lastSeenAzimuth, lastSeenElevation;
    bool isRotConnected;
    QScopedPointer<MapSpotProvider> spotProvider;
    QString currentBand;
};

#endif // QLOG_UI_ONLINEMAPWIDGET_H
