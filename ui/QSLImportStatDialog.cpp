#include "QSLImportStatDialog.h"
#include "ui_QSLImportStatDialog.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.QSLImportStatDialog");

QSLImportStatDialog::QSLImportStatDialog(QSLMergeStat stats, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QSLImportStatDialog)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    ui->updatedNumer->setText(QString::number(stats.updatedQSOs.size() + stats.newQSLs.size()));
    ui->downloadedNumber->setText(QString::number(stats.qsosDownloaded));
    ui->unmatchedNumber->setText(QString::number(stats.unmatchedQSLs.size()));
    ui->errorsNumber->setText(QString::number(stats.errorQSLs.size()));

    if ( !stats.newQSLs.isEmpty() )
    {
        ui->detailsText->moveCursor(QTextCursor::End);
        ui->detailsText->insertPlainText("*** " + tr("New QSLs: ") + "\n");
        ui->detailsText->moveCursor(QTextCursor::End);
        ui->detailsText->insertPlainText(stats.newQSLs.join("\n"));
        ui->detailsText->moveCursor(QTextCursor::End);
        ui->detailsText->insertPlainText("\n\n");
        ui->detailsText->moveCursor(QTextCursor::End);
    }

    if ( !stats.updatedQSOs.isEmpty() )
    {
        ui->detailsText->insertPlainText("*** " + tr("Updated QSOs: ") + "\n");
        ui->detailsText->moveCursor(QTextCursor::End);
        ui->detailsText->insertPlainText(stats.updatedQSOs.join("\n"));
        ui->detailsText->moveCursor(QTextCursor::End);
        ui->detailsText->insertPlainText("\n\n");
        ui->detailsText->moveCursor(QTextCursor::End);
    }

    if ( !stats.unmatchedQSLs.isEmpty() )
    {
        ui->detailsText->insertPlainText("*** " + tr("Unmatched QSLs: ") + "\n");
        ui->detailsText->moveCursor(QTextCursor::End);
        ui->detailsText->insertPlainText (stats.unmatchedQSLs.join("\n"));
        ui->detailsText->moveCursor(QTextCursor::End);
    }
}

QSLImportStatDialog::~QSLImportStatDialog()
{
    FCT_IDENTIFICATION;

    delete ui;
}
