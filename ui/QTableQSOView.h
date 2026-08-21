#ifndef QLOG_UI_QTABLEQSOVIEW_H
#define QLOG_UI_QTABLEQSOVIEW_H

#include <QPersistentModelIndex>
#include <QTableView>
#include <QObject>

class QTableQSOView : public QTableView
{
    Q_OBJECT

signals:
    void dataCommitted();

public:
    explicit QTableQSOView(QWidget *parent = nullptr);
    using QTableView::edit;
    void commitData(QWidget *editor) override;
    void keyPressEvent(QKeyEvent *event) override;

protected:
    bool edit(const QModelIndex &index, EditTrigger trigger, QEvent *event) override;
    void closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint) override;

private:
    void clearEditContext();

    QPersistentModelIndex editedIndex;
    QList<QPersistentModelIndex> editTargets;
};

#endif // QLOG_UI_QTABLEQSOVIEW_H
