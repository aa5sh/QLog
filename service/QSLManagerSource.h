#ifndef QLOG_SERVICE_QSLMANAGERSOURCE_H
#define QLOG_SERVICE_QSLMANAGERSOURCE_H

#include <QObject>
#include <QString>

struct QSLQueryResult
{
    enum class Status
    {
        Found,
        NotFound,
        Ambiguous,
        Error,
        Timeout
    };

    Status status = Status::NotFound;
    QString callsign;
    QString qslVia;
    QString error;
};

class QSLManagerSource : public QObject
{
    Q_OBJECT

public:
    explicit QSLManagerSource(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~QSLManagerSource() override = default;

    virtual QString sourceId() const = 0;

signals:
    void queryFinished(QSLQueryResult result);

public slots:
    virtual void queryCallsign(const QString &callsign) = 0;
    virtual void abortQuery() = 0;
};

#endif // QLOG_SERVICE_QSLMANAGERSOURCE_H
