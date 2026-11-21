#include <QPainter>
#include <QPen>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QCursor>

#include "CardEditorWidget.h"
#include "core/debug.h"

MODULE_IDENTIFICATION("qlog.ui.component.cardeditorwidget");

QPointF CardEditorWidget::eventPosition(const QMouseEvent *event)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    return event->position();
#else
    return event->localPos();
#endif
}

CardEditorWidget::CardEditorWidget(QWidget *parent)
    : QWidget(parent)
{
    FCT_IDENTIFICATION;

    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(300, 200);
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

void CardEditorWidget::setImage(const QPixmap &pm)
{
    FCT_IDENTIFICATION;

    cardImage = pm;
    displayImage = QPixmap();
    update();
}

void CardEditorWidget::setOverlays(const QList<EmailQSLFieldOverlay> &overlays)
{
    FCT_IDENTIFICATION;

    overlayList = overlays;
    if ( selectedOverlayIndex >= overlayList.size() )
        selectedOverlayIndex = -1;
    update();
}

void CardEditorWidget::updateOverlay(int index, const EmailQSLFieldOverlay &ov)
{
    FCT_IDENTIFICATION;

    if ( index < 0 || index >= overlayList.size() )
        return;
    overlayList[index] = ov;
    update();
}

void CardEditorWidget::setSelectedIndex(int index)
{
    FCT_IDENTIFICATION;

    if ( selectedOverlayIndex == index )
        return;
    selectedOverlayIndex = index;
    update();
}

QSize CardEditorWidget::sizeHint() const
{
    FCT_IDENTIFICATION;

    if ( cardImage.isNull() )
        return QSize(500, 320);
    return cardImage.size().scaled(640, 420, Qt::KeepAspectRatio);
}

QSize CardEditorWidget::minimumSizeHint() const { return QSize(300, 200); }

// ---------------------------------------------------------------------------
// Coordinate helpers
// ---------------------------------------------------------------------------

QRectF CardEditorWidget::imageRect() const
{
    FCT_IDENTIFICATION;

    if ( cardImage.isNull() )
        return QRectF(rect());
    QSizeF sz = cardImage.size();
    sz.scale(size(), Qt::KeepAspectRatio);
    return QRectF(QPointF((width()  - sz.width())  / 2.0,
                          (height() - sz.height()) / 2.0), sz);
}

double CardEditorWidget::displayScale() const
{
    FCT_IDENTIFICATION;

    if ( cardImage.isNull() || cardImage.width() == 0 )
        return 1.0;
    return imageRect().width() / cardImage.width();
}

QPointF CardEditorWidget::imageToWidget(const QPointF &ip) const
{
    FCT_IDENTIFICATION;

    const QRectF r = imageRect();
    const double sx = r.width()  / (cardImage.isNull() ? 1 : cardImage.width());
    const double sy = r.height() / (cardImage.isNull() ? 1 : cardImage.height());
    return QPointF(r.left() + ip.x() * sx, r.top() + ip.y() * sy);
}

QPointF CardEditorWidget::widgetToImage(const QPointF &wp) const
{
    FCT_IDENTIFICATION;

    const QRectF r = imageRect();
    if ( r.width() == 0 || r.height() == 0 )
        return wp;
    const double sx = (cardImage.isNull() ? 1 : cardImage.width())  / r.width();
    const double sy = (cardImage.isNull() ? 1 : cardImage.height()) / r.height();
    return QPointF((wp.x() - r.left()) * sx, (wp.y() - r.top()) * sy);
}

QRectF CardEditorWidget::overlayWidgetRect(int idx) const
{
    FCT_IDENTIFICATION;

    if ( idx < 0 || idx >= overlayList.size() )
        return {};
    const EmailQSLFieldOverlay &ov = overlayList.at(idx);
    const QPointF tl = imageToWidget(QPointF(ov.x, ov.y));
    const double  sc = displayScale();

    if ( ov.type == QLatin1String("BOX") )
    {
        return QRectF(tl, QSizeF(ov.width * sc, ov.height * sc));
    }

    // TEXT / LABEL: bounding box of the displayed string
    QFont f(ov.fontFamily, qMax(8, qRound(ov.fontSize * sc)));
    f.setBold(ov.bold); f.setItalic(ov.italic);
    const QFontMetrics fm(f);
    const QString lbl = (ov.type == QLatin1String("LABEL"))
                        ? ov.fieldName
                        : (QLatin1Char('{') + ov.fieldName + QLatin1Char('}'));
    return QRectF(tl.x(), tl.y() - fm.ascent(), fm.horizontalAdvance(lbl), fm.height());
}

QRectF CardEditorWidget::resizeHandleRect(int idx) const
{
    FCT_IDENTIFICATION;

    const QRectF r = overlayWidgetRect(idx);
    if ( r.isNull() )
        return {};
    const double h = RESIZE_HANDLE_PX;
    return QRectF(r.right() - h, r.bottom() - h, h * 2, h * 2);
}

bool CardEditorWidget::isOnResizeHandle(int idx, const QPointF &wPt) const
{
    FCT_IDENTIFICATION;

    if ( idx < 0 || idx >= overlayList.size() )
        return false;
    if ( overlayList.at(idx).type != QLatin1String("BOX") )
        return false;
    return resizeHandleRect(idx).contains(wPt);
}

int CardEditorWidget::overlayAt(const QPointF &wPt) const
{
    FCT_IDENTIFICATION;

    // Iterate in reverse so top-drawn items (higher index) are hit first
    for ( int i = overlayList.size() - 1; i >= 0; --i )
    {
        const QRectF r = overlayWidgetRect(i).adjusted(-4, -4, 4, 4);
        if ( r.contains(wPt) )
            return i;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------

void CardEditorWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    // Background
    p.fillRect(rect(), palette().window());

    if ( cardImage.isNull() )
    {
        p.setPen(QColor(0x88, 0x88, 0x88));
        p.drawText(rect(), Qt::AlignCenter,
                   tr("No card image selected.\nClick \"Browse…\" to choose a QSL card image."));
        p.setPen(QPen(QColor(0x88, 0x88, 0x88), 1, Qt::DashLine));
        p.drawRect(rect().adjusted(0, 0, -1, -1));
        return;
    }

    // Card image
    const QRect target = imageRect().toRect();
    const qreal ratio = devicePixelRatioF();
    const QSize pixelSize = target.size() * ratio;
    if ( displayImage.size() != pixelSize )
    {
        displayImage = cardImage.scaled(pixelSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        displayImage.setDevicePixelRatio(ratio);
    }
    p.drawPixmap(target.topLeft(), displayImage);

    const double sc = displayScale();

    for ( int i = 0; i < overlayList.size(); ++i )
    {
        const EmailQSLFieldOverlay &ov = overlayList.at(i);
        const bool sel = (i == selectedOverlayIndex);
        const QRectF wr = overlayWidgetRect(i);

        if ( ov.type == QLatin1String("BOX") )
        {
            // --- Fill ---
            QColor fill(ov.fillColor);
            fill.setAlphaF(ov.opacity / 100.0);
            const double cr = ov.cornerRadius * sc;

            p.setPen(QPen(QColor(ov.color), sel ? 2.0 : 1.5));
            p.setBrush(fill);
            p.drawRoundedRect(wr, cr, cr);

            // --- Caption above box ---
            if ( !ov.fieldName.isEmpty() )
            {
                const int dispSz = qMax(8, qRound((ov.fontSize > 0 ? ov.fontSize : 11) * sc));
                QFont f(ov.fontFamily, dispSz);
                f.setBold(ov.bold);
                p.setFont(f);
                p.setPen(QColor(ov.color));
                p.setBrush(Qt::NoBrush);
                QFontMetrics fm(f);
                p.drawText(QPointF(wr.left(), wr.top() - fm.descent() - 2), ov.fieldName);
            }

            // --- Selection decoration ---
            if ( sel )
            {
                p.save();
                p.setPen(QPen(QColor(0, 120, 215), 1.5, Qt::DashLine));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(wr.adjusted(-3, -3, 3, 3), cr + 3, cr + 3);

                // Resize handle (bottom-right corner)
                const QRectF rh = resizeHandleRect(i);
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(0, 120, 215));
                p.drawEllipse(rh.center(), RESIZE_HANDLE_PX * 0.5, RESIZE_HANDLE_PX * 0.5);

                // Size info label
                p.setPen(QColor(0, 80, 180));
                p.setFont(QFont(QStringLiteral("Arial"), 8));
                p.drawText(QPointF(wr.left() + 3, wr.bottom() - 3),
                           QString("%1×%2").arg(ov.width).arg(ov.height));
                p.restore();
            }
        }
        else // TEXT or LABEL
        {
            const int dispSz = qMax(8, qRound(ov.fontSize * sc));
            QFont f(ov.fontFamily, dispSz);
            f.setBold(ov.bold); f.setItalic(ov.italic);
            p.setFont(f);

            // LABEL shows literal text; TEXT shows {FIELDNAME} placeholder
            const QString lbl = (ov.type == QLatin1String("LABEL"))
                                 ? ov.fieldName
                                 : (QLatin1Char('{') + ov.fieldName + QLatin1Char('}'));
            const QFontMetrics fm(f);
            const QPointF anchor(wr.left(), wr.top() + fm.ascent());

            // Selection box
            if ( sel )
            {
                p.save();
                p.setPen(QPen(QColor(0, 120, 215), 1.5, Qt::DashLine));
                p.setBrush(QColor(0, 120, 215, 25));
                p.drawRect(wr.adjusted(-3, -3, 3, 3));
                // Drag-handle dot
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(0, 120, 215));
                p.drawEllipse(QPointF(wr.left() - 7, wr.center().y()), 4, 4);
                p.restore();
            }
            else
            {
                p.save();
                p.setPen(QPen(QColor(0, 0, 0, 35), 1, Qt::DotLine));
                p.setBrush(Qt::NoBrush);
                p.drawRect(wr.adjusted(-2, -2, 2, 2));
                p.restore();
            }

            // Shadow
            {
                QColor shadow = QColor(ov.color).lightness() > 128
                                ? QColor(0, 0, 0, 90) : QColor(255, 255, 255, 90);
                p.save();
                p.setPen(shadow);
                p.drawText(anchor + QPointF(1, 1), lbl);
                p.restore();
            }

            p.setPen(QColor(ov.color));
            p.drawText(anchor, lbl);
        }
    }

    // Border
    p.setPen(QPen(QColor(0x88, 0x88, 0x88), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

// ---------------------------------------------------------------------------
// Mouse events
// ---------------------------------------------------------------------------

void CardEditorWidget::mousePressEvent(QMouseEvent *event)
{
    FCT_IDENTIFICATION;

    if ( event->button() != Qt::LeftButton )
    {
        QWidget::mousePressEvent(event);
        return;
    }

    const QPointF wPos = eventPosition(event);
    const int idx = overlayAt(wPos);

    selectedOverlayIndex = idx;
    dragIndex     = -1;
    resizing      = false;

    if ( idx >= 0 )
    {
        dragIndex     = idx;
        dragStartImage  = widgetToImage(wPos);
        dragAnchorImage = QPointF(overlayList.at(idx).x, overlayList.at(idx).y);

        if ( isOnResizeHandle(idx, wPos) )
        {
            resizing = true;
            dragSizeAtPress = QSizeF(overlayList.at(idx).width, overlayList.at(idx).height);
            setCursor(Qt::SizeFDiagCursor);
        }
        else
        {
            setCursor(Qt::ClosedHandCursor);
        }
    }

    emit overlaySelected(selectedOverlayIndex);
    update();
}

void CardEditorWidget::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF wPos = eventPosition(event);

    if ( dragIndex >= 0 && (event->buttons() & Qt::LeftButton) )
    {
        const QPointF curImg = widgetToImage(wPos);
        const QPointF delta  = curImg - dragStartImage;

        EmailQSLFieldOverlay &ov = overlayList[dragIndex];
        const int imgW = cardImage.isNull() ? 99999 : cardImage.width();
        const int imgH = cardImage.isNull() ? 99999 : cardImage.height();

        if ( resizing )
        {
            const int newW = qMax(20, qRound(dragSizeAtPress.width()  + delta.x()));
            const int newH = qMax(10, qRound(dragSizeAtPress.height() + delta.y()));
            ov.width  = qMin(newW, imgW - ov.x);
            ov.height = qMin(newH, imgH - ov.y);
        }
        else
        {
            ov.x = qBound(0, qRound(dragAnchorImage.x() + delta.x()), imgW - 1);
            ov.y = qBound(0, qRound(dragAnchorImage.y() + delta.y()), imgH - 1);
        }
        update();
    }
    else
    {
        // Cursor feedback
        const int hov = overlayAt(wPos);
        if ( hov >= 0 )
        {
            if ( isOnResizeHandle(hov, wPos) )
                setCursor(Qt::SizeFDiagCursor);
            else
                setCursor(Qt::OpenHandCursor);
        }
        else
        {
            setCursor(Qt::ArrowCursor);
        }
    }
}

void CardEditorWidget::mouseReleaseEvent(QMouseEvent *event)
{
    FCT_IDENTIFICATION;

    if ( event->button() == Qt::LeftButton && dragIndex >= 0 )
    {
        const EmailQSLFieldOverlay &ov = overlayList.at(dragIndex);
        if ( resizing )
            emit overlaySizeChanged(dragIndex, ov.width, ov.height);
        else
            emit overlayPositionChanged(dragIndex, ov.x, ov.y);

        dragIndex = -1;
        resizing  = false;
        setCursor(Qt::ArrowCursor);
        update();
    }
}
