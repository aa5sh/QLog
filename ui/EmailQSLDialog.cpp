#include <QMessageBox>
#include <QPushButton>
#include <QDateTime>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QDialogButtonBox>
#include <QPainter>
#include <QFileDialog>
#include <QStandardPaths>

#include "EmailQSLDialog.h"
#include "ui_EmailQSLDialog.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.emailqsldialog");

EmailQSLDialog::EmailQSLDialog(const QSqlRecord &record, QWidget *parent)
    : QDialog(parent),
      ui(new Ui::EmailQSLDialog),
      qsoRecord(record),
      emailService(new EmailQSLService(this))
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    // Add action buttons next to Cancel
    QPushButton *previewBtn = ui->buttonBox->addButton(tr("Preview Card…"), QDialogButtonBox::ActionRole);
    connect(previewBtn, &QPushButton::clicked, this, &EmailQSLDialog::previewAndPrintCard);

    QPushButton *sendBtn = ui->buttonBox->addButton(tr("Send"), QDialogButtonBox::AcceptRole);
    connect(sendBtn, &QPushButton::clicked, this, &EmailQSLDialog::sendClicked);
    connect(emailService, &EmailQSLService::sendFinished,
            this, &EmailQSLDialog::onSendFinished);

    populateInfo();
    buildWarnings();
}

EmailQSLDialog::~EmailQSLDialog()
{
    FCT_IDENTIFICATION;

    delete ui;
}

void EmailQSLDialog::populateInfo()
{
    FCT_IDENTIFICATION;

    const QString callsign = qsoRecord.value(QStringLiteral("callsign")).toString().toUpper();
    const QString email    = qsoRecord.value(QStringLiteral("email")).toString();
    const QDateTime dt     = qsoRecord.value(QStringLiteral("start_time")).toDateTime().toUTC();
    const QString band     = qsoRecord.value(QStringLiteral("band")).toString().toUpper();
    const QString mode     = qsoRecord.value(QStringLiteral("mode")).toString().toUpper();

    ui->callValueLabel->setText(callsign.isEmpty() ? tr("(unknown)") : callsign);
    ui->emailValueLabel->setText(email.isEmpty()
                                 ? tr("<span style='color:red'>No email address on record</span>")
                                 : email);
    ui->dateValueLabel->setText(dt.isValid() ? dt.toString(Qt::RFC2822Date) : tr("(unknown)"));
    ui->bandModeValueLabel->setText(
        QString("%1  /  %2").arg(band.isEmpty() ? QStringLiteral("?") : band,
                                 mode.isEmpty() ? QStringLiteral("?") : mode));

    // Subject / body previews
    ui->subjectPreviewLabel->setText(
        EmailQSLBase::applyMergeFields(EmailQSLBase::getSubjectTemplate(), qsoRecord));
    ui->bodyPreviewEdit->setPlainText(
        EmailQSLBase::applyMergeFields(EmailQSLBase::getBodyTemplate(), qsoRecord));

    // Card thumbnail
    const QPixmap card = EmailQSLBase::renderCard(qsoRecord);
    if ( !card.isNull() )
        ui->cardPreviewLabel->setPixmap(card.scaled(
            ui->cardPreviewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    else
        ui->cardPreviewLabel->setText(tr("No card image"));
}

void EmailQSLDialog::buildWarnings()
{
    FCT_IDENTIFICATION;

    QStringList warnings;

    // 1) No email address
    const QString emailAddr = qsoRecord.value(QStringLiteral("email")).toString().trimmed();
    if ( emailAddr.isEmpty() )
    {
        warnings << tr("This contact has no email address — the message cannot be sent.");
        // Disable the Send button
        for ( QAbstractButton *btn : ui->buttonBox->buttons() )
            if ( ui->buttonBox->buttonRole(btn) == QDialogButtonBox::AcceptRole )
                btn->setEnabled(false);
    }

    // 2) Already sent for this specific QSO
    const QDateTime prevSent = EmailQSLBase::getEmailSentDateTime(qsoRecord);
    if ( prevSent.isValid() )
    {
        warnings << tr("An Email QSL was already sent for this QSO on %1 UTC.")
                    .arg(prevSent.toString(Qt::RFC2822Date));
    }

    // 3) Previously sent to the same callsign (different QSO)
    const QString callsign = qsoRecord.value(QStringLiteral("callsign")).toString().toUpper();
    const int     id       = qsoRecord.value(QStringLiteral("id")).toInt();
    if ( !callsign.isEmpty() && EmailQSLBase::hasEmailBeenSentToCallsign(callsign, id) )
    {
        warnings << tr("Note: you have previously sent an Email QSL to %1 for a different QSO.")
                    .arg(callsign);
    }

    // 4) No SMTP host configured
    if ( EmailQSLBase::getSmtpHost().isEmpty() )
        warnings << tr("SMTP server is not configured. Go to Settings → Email QSL.");

    if ( !warnings.isEmpty() )
        ui->warningLabel->setText(warnings.join(QStringLiteral("\n")));
}

void EmailQSLDialog::sendClicked()
{
    FCT_IDENTIFICATION;

    // Disable buttons while sending
    for ( QAbstractButton *btn : ui->buttonBox->buttons() )
        btn->setEnabled(false);

    ui->statusLabel->setText(tr("Sending…"));

    emailService->sendEmailQSL(qsoRecord);
}

void EmailQSLDialog::onSendFinished(bool success, const QString &message)
{
    FCT_IDENTIFICATION;

    if ( success )
    {
        // EmailQSLService has already recorded the send in contacts.fields.
        // Auto-close — the log table refresh is handled by the finished() signal
        accept();
    }
    else
    {
        ui->statusLabel->setStyleSheet(QStringLiteral("color: red; font-weight: bold;"));
        ui->statusLabel->setText(tr("Send failed: %1").arg(message));

        // Rename Cancel → Close and disable Send so the user can only dismiss
        for ( QAbstractButton *btn : ui->buttonBox->buttons() )
        {
            const QDialogButtonBox::ButtonRole role = ui->buttonBox->buttonRole(btn);
            if ( role == QDialogButtonBox::RejectRole )
            {
                btn->setText(tr("Close"));
                btn->setEnabled(true);
            }
            else
            {
                btn->setEnabled(false); // keep Send / Preview disabled
            }
        }
    }
}

void EmailQSLDialog::previewAndPrintCard()
{
    FCT_IDENTIFICATION;

    const QPixmap card = EmailQSLBase::renderCard(qsoRecord);
    if ( card.isNull() )
    {
        QMessageBox::warning(this, tr("Preview"),
                             tr("Could not render the card image.\n"
                                "Please check Settings → Email QSL and make sure a card image is selected."));
        return;
    }

    QDialog *dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(tr("Card Preview — %1")
                        .arg(qsoRecord.value(QStringLiteral("callsign")).toString().toUpper()));
    QVBoxLayout *lay = new QVBoxLayout(dlg);

    QScrollArea *scroll = new QScrollArea(dlg);
    QLabel *lbl = new QLabel(scroll);
    const QPixmap scaled = card.scaled(QSize(800, 600), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    lbl->setPixmap(scaled);
    lbl->adjustSize();
    scroll->setWidget(lbl);
    scroll->setMinimumSize(qMin(scaled.width() + 20, 820),
                           qMin(scaled.height() + 20, 620));
    lay->addWidget(scroll);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
    QPushButton *saveBtn = bb->addButton(tr("Save Card…"), QDialogButtonBox::ActionRole);

    connect(saveBtn, &QPushButton::clicked, dlg, [card, this]()
    {
        const QString callsign = qsoRecord.value(QStringLiteral("callsign")).toString().toUpper();
        const QString rawDate  = qsoRecord.value(QStringLiteral("start_time")).toString();
        const QDateTime dt     = QDateTime::fromString(rawDate, Qt::ISODate);
        const QString date     = dt.isValid() ? dt.toUTC().toString(QStringLiteral("yyyyMMdd")) : QStringLiteral("date");
        const QString time     = dt.isValid() ? dt.toUTC().toString(QStringLiteral("HHmm"))     : QStringLiteral("time");
        const QString band     = qsoRecord.value(QStringLiteral("band")).toString().toUpper().replace(QLatin1Char(' '), QLatin1Char('_'));
        const QString mode     = qsoRecord.value(QStringLiteral("mode")).toString().toUpper();
        const QString defaultName = QStringLiteral("QSL_%1_%2_%3_%4_%5.png")
                                    .arg(callsign, date, time, band, mode);
        const QString defaultDir  = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        const QString path = QFileDialog::getSaveFileName(
            this,
            tr("Save QSL Card"),
            defaultDir + QStringLiteral("/") + defaultName,
            tr("PNG Image (*.png);;JPEG Image (*.jpg *.jpeg)"));

        if ( path.isEmpty() )
            return;

        if ( !card.save(path) )
        {
            QMessageBox::warning(this, tr("Save Failed"),
                                 tr("Could not save the card image to:\n%1").arg(path));
        }
    });

    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::close);
    lay->addWidget(bb);
    dlg->show(); // non-modal
}
