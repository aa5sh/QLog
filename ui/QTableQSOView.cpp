#include <QAbstractItemModel>
#include <QKeyEvent>
#include "QTableQSOView.h"
#include "models/LogbookModel.h"

QTableQSOView::QTableQSOView(QWidget *parent) :
    QTableView(parent)
{ }

void QTableQSOView::commitData(QWidget *editor)
{
    QModelIndex editedIndex;
    if ( editor )
        editedIndex = indexAt(editor->mapTo(viewport(), editor->rect().center()));

    if ( !editedIndex.isValid() )
        editedIndex = this->currentIndex();

    int currRow = editedIndex.row();
    int currCol = editedIndex.column();
    const bool modeSubmodeColumn = currCol == LogbookModel::COLUMN_MODE_SUBMODE;
    QList<int> modeSubmodeSelectedRows;

    if ( modeSubmodeColumn )
    {
        const QModelIndexList &selectedRows = this->selectionModel()->selectedRows();
        for ( const QModelIndex &index : selectedRows )
            modeSubmodeSelectedRows << index.row();
    }

    QTableView::commitData(editor);

    QAbstractItemModel *model = this->model();
    QVariant value = model->data(model->index(currRow, currCol), Qt::EditRole);
    const QVariant modeValue = model->data(model->index(currRow, LogbookModel::COLUMN_MODE), Qt::EditRole);
    const QVariant submodeValue = model->data(model->index(currRow, LogbookModel::COLUMN_SUBMODE), Qt::EditRole);

    /* Group Editing Support */
    /* If rows are selected then update them*/
    if ( modeSubmodeColumn )
    {
        for ( int row : modeSubmodeSelectedRows )
        {
            if ( row != currRow ) // Do not update the same row again
            {
                model->setData(model->index(row, LogbookModel::COLUMN_MODE),
                               modeValue, Qt::EditRole);
                model->setData(model->index(row, LogbookModel::COLUMN_SUBMODE),
                               submodeValue, Qt::EditRole);
            }
        }
    }
    else
    {
        const QModelIndexList &selectedRows = this->selectionModel()->selectedRows();

        for ( const QModelIndex &index : selectedRows )
        {
            if ( index.row() != currRow // Do not update the same row again
                 /* Protect selected columns against group editing */
                 && currCol != LogbookModel::COLUMN_CALL
                 && currCol != LogbookModel::COLUMN_TIME_ON
                 && currCol != LogbookModel::COLUMN_TIME_OFF )
            {
                model->setData(model->index(index.row(),currCol), value, Qt::EditRole);
            }
        }
    }

    emit dataCommitted();
}

void QTableQSOView::keyPressEvent(QKeyEvent *event)
{
    if ( event->key() == Qt::Key_F2 )
    {
        return;
    }

    QTableView::keyPressEvent(event);
};
