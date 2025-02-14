#ifndef QLOG_CORE_PROPCONDITIONS_H
#define QLOG_CORE_PROPCONDITIONS_H

#include <QObject>
#include <QDateTime>
#include <QVector>
#include <QTimer>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

template<class T>
class GenericValueMap
{
public:
    explicit GenericValueMap() {}
    ~GenericValueMap(){};

    struct MapPoint
    {
        double longitude;
        double latitude;
        T value;
    };

    virtual void addPoint(double longitude,
                          double latitude,
                          const T &value,
                          const T *skipValue = nullptr)
    {
        if ( skipValue && value == *skipValue )
            return;

        GenericValueMap::MapPoint point;
        point.longitude = longitude;
        point.latitude = latitude;
        point.value = value;

        map.append(point);
    };

    QList<GenericValueMap::MapPoint> getMap() const
    {
        return map;
    };

    void clear()
    {
        map.clear();
    };

    int count() const
    {
        return map.size();
    };

private:
    QList<GenericValueMap::MapPoint> map;
};

class PropConditions : public QObject
{
    Q_OBJECT
public:
    explicit PropConditions(QObject *parent = nullptr);
    ~PropConditions();
    bool isFluxValid();
    bool isKIndexValid();
    bool isAIndexValid();
    bool isAuroraMapValid();
    bool isMufMapValid();
    int getFlux();
    int getAIndex();
    double getKIndex();
    QList<GenericValueMap<double>::MapPoint> getAuroraPoints() const;
    QList<GenericValueMap<double>::MapPoint> getMUFPoints() const;

    static QString solarSummaryFile();

signals:
    void conditionsUpdated();
    void fluxUpdated();
    void KIndexUpdated();
    void AIndexUpdated();
    void auroraMapUpdated();
    void mufMapUpdated();
    void dxTrendFinalized(QHash<QString, QHash<QString, QHash<QString, int>>>);

public slots:
    void update();
    void updateDxTrends();
    void processReply(QNetworkReply* reply);

private:
    QDateTime flux_last_update;
    QDateTime k_index_last_update;
    QDateTime a_index_last_update;
    QDateTime auroraMap_last_update;
    QDateTime mufMap_last_update;
    int flux;
    int a_index;
    double k_index;
    GenericValueMap<double> auroraMap;
    GenericValueMap<double> mufMap;
    QHash<QUrl, int> failedRequests;
    QList<QNetworkReply *> dxTrendPendingConnections;
    QTimer dxTrendTimeoutTimer;
    QHash<QString, QHash<QString, QHash<QString, int>>> dxTrendResult;
    QByteArray agentString;
    void repeateRequest(const QUrl &);
    QNetworkRequest prepareRequest(const QUrl &);

private slots:
    void dxTrendTimeout();

private:
    QNetworkAccessManager* nam;
};

#endif // QLOG_CORE_PROPCONDITIONS_H
