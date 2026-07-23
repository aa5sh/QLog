#include <QAbstractItemModel>
#include <QKeyEvent>
#include "QTableQSOView.h"
#include "models/LogbookModel.h"

QTableQSOView::QTableQSOView(QWidget *parent) :
    QTableView(parent)
{ }

bool QTableQSOView::edit(const QModelIndex &index, EditTrigger trigger, QEvent *event)
{
    if ( !QTableView::edit(index, trigger, event) )
        return false;

    editedIndex = QPersistentModelIndex(index);
    editTargets.clear();

    const QModelIndexList selectedRows = selectionModel()->selectedRows();
    editTargets.reserve(selectedRows.size());

    for ( const QModelIndex &selectedRow : selectedRows )
        editTargets.append(QPersistentModelIndex(model()->index(selectedRow.row(),
                                                                index.column())));

    return true;
}

void QTableQSOView::commitData(QWidget *editor)
{
    const QPersistentModelIndex sourceIndex = editedIndex;
    const QList<QPersistentModelIndex> targetIndexes = editTargets;

    QTableView::commitData(editor);

    QAbstractItemModel *tableModel = model();

    if ( sourceIndex.isValid() && sourceIndex.model() == tableModel )
    {
        const int sourceColumn = sourceIndex.column();
        const QVariant value = tableModel->data(sourceIndex, Qt::EditRole);

        for ( const QPersistentModelIndex &targetIndex : targetIndexes )
        {
            if ( targetIndex.isValid()
                 && targetIndex.model() == tableModel
                 && targetIndex.row() != sourceIndex.row()
                 /* Protect selected columns against group editing */
                 && sourceColumn != LogbookModel::COLUMN_CALL
                 && sourceColumn != LogbookModel::COLUMN_TIME_ON
                 && sourceColumn != LogbookModel::COLUMN_TIME_OFF )
            {
                tableModel->setData(targetIndex, value, Qt::EditRole);
            }
        }
    }

    emit dataCommitted();
}

void QTableQSOView::closeEditor(QWidget *editor,
                                QAbstractItemDelegate::EndEditHint hint)
{
    clearEditContext();
    QTableView::closeEditor(editor, hint);
}

void QTableQSOView::clearEditContext()
{
    editedIndex = QPersistentModelIndex();
    editTargets.clear();
}

void QTableQSOView::keyPressEvent(QKeyEvent *event)
{
    if ( event->key() == Qt::Key_F2 )
    {
        return;
    }

    QTableView::keyPressEvent(event);
};
