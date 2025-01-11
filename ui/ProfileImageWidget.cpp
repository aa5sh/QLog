#include <QVBoxLayout>
#include "ProfileImageWidget.h"
#include "ui_ProfileImageWidget.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.profileimagewidget");

ProfileImageWidget::ProfileImageWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ProfileImageWidget),
    imageLabel(new AspectRatioLabel(this)),
    nam(new QNetworkAccessManager(this)),
    currentReply(nullptr)
{
    FCT_IDENTIFICATION;

    ui->setupUi(this);

    QVBoxLayout *layout = new QVBoxLayout(this);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setMinimumSize(sizeHint());
    layout->addWidget(imageLabel);

    connect(nam, &QNetworkAccessManager::finished,
            this, &ProfileImageWidget::processReply);
}

ProfileImageWidget::~ProfileImageWidget()
{
    FCT_IDENTIFICATION;

    if ( currentReply )
        currentReply->abort();

    nam->deleteLater();
    imageLabel->deleteLater();

    delete ui;
}

void ProfileImageWidget::loadImageFromUrl(const QString &urlAddress)
{
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << urlAddress;

    if ( urlAddress.isEmpty() )
    {
        imageLabel->clear();
        imageLabel->setToolTip("");
        return;
    }

    if ( currentReply )
    {
        qCDebug(runtime) << "Previous request is still running";
        currentReply->abort();
    }

    QUrl url(urlAddress);

    currentReply = nam->get(QNetworkRequest(url));
}

void ProfileImageWidget::processReply(QNetworkReply *reply)
{
    FCT_IDENTIFICATION;

    /* always process one requests per class */
    currentReply = nullptr;

    int replyStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if ( reply->error() != QNetworkReply::NoError
         || replyStatusCode < 200
         || replyStatusCode >= 300)
    {
        qCDebug(runtime) << "Download Image error: URL " << reply->request().url().toString();
        qCDebug(runtime) << "Download Image error:" << reply->errorString();
        qCDebug(runtime) << "HTTP Status Code" << replyStatusCode;

        if ( reply->error() != QNetworkReply::OperationCanceledError )
        {
            reply->deleteLater();
        }
        return;
    }

    QByteArray imageData = reply->readAll();
    QPixmap pixmap;

    pixmap.loadFromData(imageData);
    imageLabel->setPixmap(pixmap);
    imageLabel->setScaledContents(true);
    mimeType = mimeDatabase.mimeTypeForData(imageData);

    if ( mimeType.name().contains("image", Qt::CaseInsensitive))
    {
        // QByteArray->QString in arg is due to QT5.12
        imageLabel->setToolTip(QString("<img src='data:%0;base64, %1'>").arg(mimeType.name(), QString(imageData.toBase64())));
    }

    reply->deleteLater();
}

AspectRatioLabel::AspectRatioLabel(QWidget* parent, Qt::WindowFlags f) :
    QLabel(parent, f)
{
}

AspectRatioLabel::~AspectRatioLabel()
{
}

void AspectRatioLabel::setPixmap(const QPixmap& pm)
{
    pixmapWidth = pm.width();
    pixmapHeight = pm.height();

    updateMargins();
    QLabel::setPixmap(pm);
}

void AspectRatioLabel::resizeEvent(QResizeEvent* event)
{
    updateMargins();
    QLabel::resizeEvent(event);
}

void AspectRatioLabel::updateMargins()
{
    if ( pixmapWidth <= 0 || pixmapHeight <= 0 )
        return;

    int w = width();
    int h = height();

    if ( w <= 0 || h <= 0 )
        return;

    if ( w * pixmapHeight > h * pixmapWidth )
    {
        int m = (w - (pixmapWidth * h / pixmapHeight)) / 2;
        setContentsMargins(m, 0, m, 0);
    }
    else
    {
        int m = (h - (pixmapHeight * w / pixmapWidth)) / 2;
        setContentsMargins(0, m, 0, m);
    }
}
