#include "rotator/Rotator.h"
#include "core/debug.h"
#include "rotator/drivers/HamlibRotDrv.h"
#include "rotator/drivers/PSTRotDrv.h"
#include <QSemaphore>
#include <QSharedPointer>
#include <QThread>

MODULE_IDENTIFICATION("qlog.rotator.rotator");

#define MUTEXLOCKER     qCDebug(runtime) << "Waiting for Rot mutex"; \
                        QMutexLocker locker(&rotLock); \
                        qCDebug(runtime) << "Using Rot"


#define TIME_PERIOD 1000

Rotator::Rotator(QObject *parent) :
    QObject{parent},
    rotDriver(nullptr),
    connected(false),
    cacheAzimuth(0.0),
    cacheElevation(0.0),
    openRequestSequence(0)
{
    FCT_IDENTIFICATION;

    drvMapping[HAMLIB_DRIVER] = DrvParams(HAMLIB_DRIVER,
                                          "Hamlib",
                                          &HamlibRotDrv::getModelList,
                                          &HamlibRotDrv::getCaps);
    drvMapping[PSTROTATOR_DRIVER] = DrvParams(PSTROTATOR_DRIVER,
                                          "PSTRotator",
                                          &PSTRotDrv::getModelList,
                                          &PSTRotDrv::getCaps);
}

Rotator::~Rotator()
{
    FCT_IDENTIFICATION;

    if ( !rotDriver )
        return;

    if ( QThread::currentThread() != thread() )
    {
        qCWarning(runtime) << "Skipping Rotator shutdown from non-owner thread";
        return;
    }

    __closeRot();
    stopTimerImplt();
}

bool Rotator::getPosition(double &azimuth, double &elevation) const
{
    FCT_IDENTIFICATION;

    QMutexLocker locker(&stateLock);
    azimuth = cacheAzimuth;
    elevation = cacheElevation;
    return connected;
}

bool Rotator::isRotConnected() const
{
    FCT_IDENTIFICATION;

    QMutexLocker locker(&stateLock);
    return connected;
}

const QList<QPair<int, QString> > Rotator::getModelList(const DriverID &id) const
{
    FCT_IDENTIFICATION;

    QList<QPair<int, QString>> ret;

    if ( drvMapping.contains(id)
         && drvMapping.value(id).getModeslListFunction != nullptr )
    {
        ret = (drvMapping.value(id).getModeslListFunction)();
    }
    return ret;
}

const QList<QPair<int, QString> > Rotator::getDriverList() const
{
    FCT_IDENTIFICATION;

    QList<QPair<int, QString>> ret;

    const QList<int> &keys = drvMapping.keys();

    for ( const int &key : keys )
    {
        ret << QPair<int, QString>(key, drvMapping[key].driverName);
    }

    return ret;
}

const RotCaps Rotator::getRotCaps(const DriverID &id, int model) const
{
    FCT_IDENTIFICATION;

    if ( drvMapping.contains(id)
         && drvMapping.value(id).getCapsFunction != nullptr)
    {
        return (drvMapping.value(id).getCapsFunction)(model);
    }
    return RotCaps();
}

void Rotator::stopTimer()
{
    FCT_IDENTIFICATION;

    MUTEXLOCKER;
    bool check = QMetaObject::invokeMethod(Rotator::instance(),
                                           &Rotator::stopTimerImplt,
                                           Qt::QueuedConnection);
    Q_ASSERT( check );
}

void Rotator::shutdown()
{
    FCT_IDENTIFICATION;

    ++openRequestSequence;

    if ( QThread::currentThread() == thread() )
    {
        shutdownImpl();
        return;
    }

    if ( !thread() || !thread()->isRunning() )
    {
        qCWarning(runtime) << "Cannot shut down Rotator because owner thread is not running";
        return;
    }

    QSharedPointer<QSemaphore> shutdownDone = QSharedPointer<QSemaphore>::create();
    bool check = QMetaObject::invokeMethod(this, [this, shutdownDone]()
    {
        shutdownImpl();
        shutdownDone->release();
    }, Qt::QueuedConnection);

    if ( !check )
    {
        qCWarning(runtime) << "Cannot queue Rotator shutdown";
        return;
    }

    if ( !shutdownDone->tryAcquire(1, SHUTDOWN_TIMEOUT_MS) )
    {
        qCWarning(runtime) << "Rotator shutdown did not finish within"
                           << SHUTDOWN_TIMEOUT_MS << "ms";
    }
}

void Rotator::stopTimerImplt()
{
    FCT_IDENTIFICATION;

    if ( rotDriver )
        rotDriver->stopTimers();
}

void Rotator::start()
{
    FCT_IDENTIFICATION;
}

void Rotator::open()
{
    FCT_IDENTIFICATION;

    const RotProfile profile = RotProfilesManager::instance()->getCurProfile1();
    const quint64 requestSequence = ++openRequestSequence;
    QMetaObject::invokeMethod(this, [this, profile, requestSequence]()
    {
        openImpl(profile, requestSequence);
    }, Qt::QueuedConnection);
}

void Rotator::openImpl(const RotProfile &profile, quint64 requestSequence)
{
    FCT_IDENTIFICATION;

    MUTEXLOCKER;

    if ( requestSequence != openRequestSequence.load() )
    {
        qCDebug(runtime) << "Skipping obsolete Rotator open request";
        return;
    }

    __openRot(profile);
}

void Rotator::sendState()
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, "sendStateImpl", Qt::QueuedConnection);
}

void Rotator::sendStateImpl()
{
    FCT_IDENTIFICATION;

    MUTEXLOCKER;

    if ( ! rotDriver )
        return;

    rotDriver->sendState();
}

void Rotator::__openRot(const RotProfile &newRotProfile)
{
    FCT_IDENTIFICATION;

    if ( newRotProfile == RotProfile() )
    {
        emit rotErrorPresent(tr("No Rotator Profile selected"),
                             QString());
        return;
    }

    if ( rotDriver )
    {
        const RotProfile &currentProfile = rotDriver->getCurrRotProfile();
        if ( currentProfile == newRotProfile )
        {
            qCDebug(runtime) << "Rotator profile is already open:"
                             << newRotProfile.profileName;
            return;
        }
    }

    // if rot is active then close it
    __closeRot();

    qCDebug(runtime) << "Opening profile name: " << newRotProfile.profileName;

    rotDriver = getDriver(newRotProfile);

    if ( !rotDriver )
    {
        // initialization failed
        emit rotErrorPresent(tr("Initialization Error"),
                             tr("Internal Error"));
        return;
    }

    connect(rotDriver, &GenericRotDrv::positioningChanged, this, [this](double a, double b)
    {
        {
            QMutexLocker locker(&stateLock);
            cacheAzimuth = a;
            cacheElevation = b;
        }
        emit positionChanged(a, b);
    });

    connect(rotDriver, &GenericRotDrv::errorOccurred, this, [this](const QString &a,
                                                                const QString &b)
    {
        close();
        emit rotErrorPresent(a, b);
    });

    connect(rotDriver, &GenericRotDrv::rotIsReady, this, [this, newRotProfile]()
    {
        {
            QMutexLocker locker(&stateLock);
            connected = true;
        }

        emit rotConnected();

        sendState();
    });

    if ( !rotDriver->open() )
    {
        emit rotErrorPresent(tr("Cannot open Rotator"),
                             rotDriver->lastError());
        qWarning() << rotDriver->lastError();
        __closeRot();
        return;
    }
}

GenericRotDrv *Rotator::getDriver(const RotProfile &profile)
{
    FCT_IDENTIFICATION;

    qCDebug(runtime) << profile.driver;

    switch ( profile.driver )
    {
    case Rotator::HAMLIB_DRIVER:
        return new HamlibRotDrv(profile, this);
        break;
    case Rotator::PSTROTATOR_DRIVER:
        return new PSTRotDrv(profile, this);
        break;
    default:
        qWarning() << "Unsupported Rotator Driver " << profile.driver;
    }

    return nullptr;
}

void Rotator::close()
{
    FCT_IDENTIFICATION;

    ++openRequestSequence;
    QMetaObject::invokeMethod(this, &Rotator::closeImpl, Qt::QueuedConnection);
}


void Rotator::closeImpl()
{
    FCT_IDENTIFICATION;

    MUTEXLOCKER;
    __closeRot();
}

void Rotator::shutdownImpl()
{
    FCT_IDENTIFICATION;

    closeImpl();
    stopTimerImplt();
}

void Rotator::__closeRot()
{
    FCT_IDENTIFICATION;

    if ( !rotDriver )
    {
        qCDebug(runtime) << "Driver is not active";
        return;
    }

    delete rotDriver;
    rotDriver = nullptr;
    {
        QMutexLocker locker(&stateLock);
        connected = false;
    }
    emit rotDisconnected();
}


void Rotator::setPosition(double azimuth, double elevation)
{
    FCT_IDENTIFICATION;

    QMetaObject::invokeMethod(this, "setPositionImpl", Qt::QueuedConnection,
                              Q_ARG(double,azimuth), Q_ARG(double,elevation));
}


void Rotator::setPositionImpl(double azimuth, double elevation)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters)<<azimuth<< " " << elevation;

    MUTEXLOCKER;

    if ( ! rotDriver )
        return;

    rotDriver->setPosition(azimuth, elevation);
}

#undef MUTEXLOCKER
