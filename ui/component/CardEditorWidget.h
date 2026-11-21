#ifndef QLOG_UI_COMPONENT_CARDEDITORWIDGET_H
#define QLOG_UI_COMPONENT_CARDEDITORWIDGET_H

#include <QWidget>
#include <QPixmap>

#include "service/emailqsl/EmailQSLService.h"

// Interactive QSL card editor widget.
// Supports two overlay types:
//   TEXT — displays a {FIELD} placeholder; drag to reposition.
//   BOX  — filled rounded rectangle; drag to move, drag bottom-right handle to resize.
class CardEditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CardEditorWidget(QWidget *parent = nullptr);

    void setImage(const QPixmap &pm);
    const QPixmap &image() const { return cardImage; }

    void setOverlays(const QList<EmailQSLFieldOverlay> &overlays);
    const QList<EmailQSLFieldOverlay> &overlays() const { return overlayList; }

    void updateOverlay(int index, const EmailQSLFieldOverlay &ov);
    void setSelectedIndex(int index);
    int  selectedIndex() const { return selectedOverlayIndex; }

    QSize sizeHint()        const override;
    QSize minimumSizeHint() const override;

signals:
    void overlayPositionChanged(int index, int newX, int newY);
    void overlaySizeChanged(int index, int newW, int newH);   // BOX resize
    void overlaySelected(int index);

protected:
    void paintEvent(QPaintEvent *)    override;
    void mousePressEvent(QMouseEvent *)   override;
    void mouseMoveEvent(QMouseEvent *)    override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    static constexpr int RESIZE_HANDLE_PX = 10; // half-size of resize grip in widget px

    QRectF  imageRect()                           const;
    double  displayScale()                        const;
    QPointF imageToWidget(const QPointF &imgPt)   const;
    QPointF widgetToImage(const QPointF &wPt)     const;
    QRectF  overlayWidgetRect(int index)          const; // widget-coords bounding rect
    QRectF  resizeHandleRect(int index)           const; // widget-coords resize grip
    int     overlayAt(const QPointF &wPt)         const; // -1 = none
    bool    isOnResizeHandle(int index, const QPointF &wPt) const;

    static QPointF eventPosition(const QMouseEvent *event);

    QPixmap                     cardImage;
    // cardImage scaled to the on-screen rect. Rescaling a full-resolution card
    // on every repaint made dragging overlays lag; rebuilt when the image
    // or the display size changes.
    QPixmap                     displayImage;
    QList<EmailQSLFieldOverlay> overlayList;
    int     selectedOverlayIndex     = -1;

    // Drag state
    int     dragIndex         = -1;
    bool    resizing          = false;
    QPointF dragStartImage;         // image-coord of press point
    QPointF dragAnchorImage;        // image-coord anchor (overlay x,y at press)
    QSizeF  dragSizeAtPress;      // overlay w,h at press (resize only)
};

#endif // QLOG_UI_COMPONENT_CARDEDITORWIDGET_H
