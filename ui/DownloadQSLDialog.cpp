#include <QMessageBox>
#include <QProgressDialog>
#include "DownloadQSLDialog.h"
#include "qpushbutton.h"
#include "ui_DownloadQSLDialog.h"
#include "models/SqlListModel.h"
#include "core/debug.h"
#include "data/StationProfile.h"
#include "service/lotw/Lotw.h"
#include "service/eqsl/Eqsl.h"
#include "ui/QSLImportStatDialog.h"
#include "core/LogParam.h"

MODULE_IDENTIFICATION("qlog.ui.downloadqsldialog");

DownloadQSLDialog::DownloadQSLDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::DownloadQSLDialog)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    ui->lotwMyCallsignCombo->setModel(new SqlListModel(
                                          "SELECT DISTINCT "
                                          "UPPER(COALESCE(NULLIF(TRIM(station_callsign), ''), TRIM(operator))) "
                                          "FROM contacts "
                                          "WHERE NULLIF(TRIM(station_callsign), '') IS NOT NULL "
                                          "OR NULLIF(TRIM(operator), '') IS NOT NULL "
                                          "ORDER BY 1",
                                          tr("All Callsigns"),
                                          ui->lotwMyCallsignCombo));
    ui->lotwDateEdit->setDisplayFormat(locale.formatDateShortWithYYYY());
    ui->eqslDateEdit->setDisplayFormat(locale.formatDateShortWithYYYY());
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("&Download"));

    const StationProfile &profile = StationProfilesManager::instance()->getCurProfile1();
    const int profileIndex = ui->lotwMyCallsignCombo->findText(profile.callsign);

    if ( LogParam::hasDownloadQSLLoTWLastCall() )
    {
        const QString lastCall = LogParam::getDownloadQSLLoTWLastCall();
        const int lastCallIndex = ui->lotwMyCallsignCombo->findText(lastCall);
        if ( lastCall.isEmpty() )
            ui->lotwMyCallsignCombo->setCurrentIndex(0);
        else
            ui->lotwMyCallsignCombo->setCurrentIndex(lastCallIndex >= 0 ? lastCallIndex
                                                                        : qMax(profileIndex, 0));
    }
    else
        ui->lotwMyCallsignCombo->setCurrentIndex(qMax(profileIndex, 0));

    loadDialogState();

    connect(ui->lotwMyCallsignCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this] { loadLotwDate(); });
    connect(ui->lotwDateTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this] { loadLotwDate(); });

    // Enable options based on the configuration
    if ( LotwBase::getUsername().isEmpty() )
    {
        ui->lotwGroupBox->setChecked(false);
        ui->lotwGroupBox->setEnabled(false);
        ui->lotwGroupBox->setToolTip(tr("LoTW is not configured properly.<p> Please, use <b>Settings</b> dialog to configure it.</p>"));
    }

    if ( EQSLBase::getUsername().isEmpty() )
    {
        ui->eqslGroupBox->setChecked(false);
        ui->eqslGroupBox->setEnabled(false);
        ui->eqslGroupBox->setToolTip(tr("eQSL is not configured properly.<p> Please, use <b>Settings</b> dialog to configure it.</p>"));
    }
}

DownloadQSLDialog::~DownloadQSLDialog()
{
    FCT_IDENTIFICATION;
    delete ui;
}

void DownloadQSLDialog::startNextDownload()
{
    FCT_IDENTIFICATION;

    if (!downloadQueue.isEmpty())
    {
        auto next = downloadQueue.dequeue();
        next(); // call Lambda for service;
    }
    else
    {
        QSLImportStatDialog statDialog(downloadStat);
        downloadStat.clear();
        if ( statDialog.exec() == QDialog::Rejected )
            done(QDialog::Accepted);
    }
}

void DownloadQSLDialog::loadDialogState()
{
    FCT_IDENTIFICATION;

    /********/
    /* LoTW */
    /********/
    ui->lotwGroupBox->setChecked(LogParam::getDownloadQSLServiceState("lotw"));
    ui->lotwDateTypeCombo->setCurrentIndex((LogParam::getDownloadQSLServiceLastQSOQSL("lotw")) ? 0 : 1);
    loadLotwDate();

    /********/
    /* eQSL */
    /********/
    ui->eqslGroupBox->setChecked(LogParam::getDownloadQSLServiceState("eqsl"));
    ui->eqslDateEdit->setDate(LogParam::getDownloadQSLServiceLastDate("eqsl"));
    ui->eqslDateTypeCombo->setCurrentIndex((LogParam::getDownloadQSLServiceLastQSOQSL("eqsl")) ? 0 : 1);

    ui->eqslQTHProfileEdit->setText(LogParam::getDownloadQSLeQSLLastProfile());
}

void DownloadQSLDialog::loadLotwDate()
{
    FCT_IDENTIFICATION;

    const bool qslSince = ui->lotwDateTypeCombo->currentIndex() == 0;
    ui->lotwDateEdit->setDate(
                LogParam::getDownloadQSLLoTWLastDate(selectedLotwCallsign(), qslSince));
}

void DownloadQSLDialog::saveDialogState()
{
    FCT_IDENTIFICATION;

    /********/
    /* LoTW */
    /********/
    LogParam::setDownloadQSLServiceState("lotw", ui->lotwGroupBox->isChecked());
    LogParam::setDownloadQSLServiceLastQSOQSL("lotw", ui->lotwDateTypeCombo->currentIndex() == 0);
    LogParam::setDownloadQSLLoTWLastCall(selectedLotwCallsign());

    /********/
    /* eQSL */
    /********/
    LogParam::setDownloadQSLServiceState("eqsl", ui->eqslGroupBox->isChecked());
    LogParam::setDownloadQSLServiceLastQSOQSL("eqsl", ui->eqslDateTypeCombo->currentIndex() == 0);
    LogParam::setDownloadQSLeQSLLastProfile(ui->eqslQTHProfileEdit->text());
}

QString DownloadQSLDialog::selectedLotwCallsign() const
{
    FCT_IDENTIFICATION;

    return (ui->lotwMyCallsignCombo->currentIndex() == 0)
            ? QString()
            : ui->lotwMyCallsignCombo->currentText();
}

void DownloadQSLDialog::prepareDownload(GenericQSLDownloader *service,
                                        const QString &serviceName,
                                        const std::function<void()> &downloadSucceeded)
{
    FCT_IDENTIFICATION;

    QProgressDialog* progressDialog = new QProgressDialog("", tr("Cancel"), 0, 0, this);
    progressDialog->setWindowModality(Qt::WindowModal);
    progressDialog->setRange(0, 0);
    progressDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    progressDialog->show();
    progressDialog->setLabelText(tr("Downloading from %1").arg(serviceName));

    connect(service, &GenericQSLDownloader::receiveQSLProgress, progressDialog, &QProgressDialog::setValue);

    connect(service, &GenericQSLDownloader::receiveQSLStarted, this, [progressDialog, serviceName]
    {
        progressDialog->setLabelText(tr("Processing %1 QSLs").arg(serviceName));
        progressDialog->setRange(0, 100);
    });

    connect(service, &GenericQSLDownloader::receiveQSLComplete, this,
            [service, progressDialog, serviceName, downloadSucceeded, this](QSLMergeStat stats)
    {
        qCDebug(runtime) << "Service:" << serviceName;
        qCDebug(runtime) << "New QSLs: " << stats.newQSLs;
        qCDebug(runtime) << "Unmatched QSLs: " << stats.unmatchedQSLs;

        downloadSucceeded();

        progressDialog->done(QDialog::Accepted);
        downloadStat[serviceName] = stats;

        service->deleteLater();
        startNextDownload();
    });

    connect(service, &GenericQSLDownloader::receiveQSLFailed, this, [this, service, progressDialog, serviceName](const QString &error)
    {
        progressDialog->done(QDialog::Accepted);
        QMessageBox::critical(this, tr("QLog Error"), tr("%1 update failed: ").arg(serviceName) + error);
        service->deleteLater();
        startNextDownload();
    });

    connect(progressDialog, &QProgressDialog::canceled, this, [this, service]()
    {
        qCDebug(runtime)<< "Operation canceled";

        service->abortDownload();
        service->deleteLater();
        downloadQueue.clear();
    });
}

void DownloadQSLDialog::downloadQSLs()
{
    FCT_IDENTIFICATION;

    saveDialogState();

    downloadQueue.clear();

    if ( ui->eqslGroupBox->isChecked() )
        downloadQueue.enqueue([=]()
        {
            EQSLQSLDownloader* eqsl = new EQSLQSLDownloader(this);
            bool qslSinceActive = ui->eqslDateTypeCombo->currentIndex() == 0;
            const QDate downloadStartedDate = QDateTime::currentDateTimeUtc().date();
            prepareDownload(eqsl, "eQSL", [downloadStartedDate]
            {
                LogParam::setDownloadQSLServiceLastDate("eqsl", downloadStartedDate);
            });
            LogParam::setDownloadQSLeQSLLastProfile(ui->eqslQTHProfileEdit->text());
            LogParam::setDownloadQSLServiceLastQSOQSL("eqsl", qslSinceActive);
            eqsl->receiveQSL(ui->eqslDateEdit->date(), !qslSinceActive, ui->eqslQTHProfileEdit->text());
        });

    if ( ui->lotwGroupBox->isChecked() )
        downloadQueue.enqueue([=]()
        {
            LotwQSLDownloader* lotw = new LotwQSLDownloader(this);
            bool qslSinceActive = ui->lotwDateTypeCombo->currentIndex() == 0;
            const QString stationCallsign = selectedLotwCallsign();
            const QDate downloadStartedDate = QDateTime::currentDateTimeUtc().date();
            prepareDownload(lotw, "LoTW", [stationCallsign, qslSinceActive, downloadStartedDate]
            {
                LogParam::setDownloadQSLLoTWLastDate(
                            stationCallsign,
                            qslSinceActive,
                            downloadStartedDate);
            });
            LogParam::setDownloadQSLServiceLastQSOQSL("lotw", qslSinceActive);
            lotw->receiveQSL(ui->lotwDateEdit->date(), !qslSinceActive, stationCallsign);
        });

    if ( downloadQueue.isEmpty() )
    {
        QMessageBox::information(this, tr("QLog Information"), tr("No service selected"));
        return;
    }

    // Download Execution
    startNextDownload();
}
