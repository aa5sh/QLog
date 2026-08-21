#include "MultilineTextDelegate.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QDialogButtonBox>
#include <QEvent>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

MultilineTextEditor::MultilineTextEditor(QWidget *parent) :
    QWidget(parent),
    textEdit(new QPlainTextEdit(this)),
    finished(false)
{
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Save
                                                     | QDialogButtonBox::Cancel,
                                                     this);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);
    layout->addWidget(textEdit);
    layout->addWidget(buttons);

    textEdit->setObjectName("multilineTextEdit");
    setFocusProxy(textEdit);
    textEdit->installEventFilter(this);

    buttons->button(QDialogButtonBox::Save)->setToolTip(tr("Save changes (Ctrl+Enter)"));
    buttons->button(QDialogButtonBox::Cancel)->setToolTip(tr("Discard changes (Esc)"));

    connect(buttons, &QDialogButtonBox::accepted,
            this, &MultilineTextEditor::requestSave);
    connect(buttons, &QDialogButtonBox::rejected,
            this, &MultilineTextEditor::requestCancel);
    connect(qApp, &QApplication::focusChanged, this,
            [this](QWidget *old, QWidget *now)
            {
                if ( !ownsFocus(now)
                     && !(ownsFocus(old) && QApplication::activePopupWidget()) )
                    requestSave();
            });
}

QString MultilineTextEditor::text() const
{
    return textEdit->toPlainText();
}

void MultilineTextEditor::setText(const QString &text)
{
    textEdit->setPlainText(text);
}

QSize MultilineTextEditor::sizeHint() const
{
    return QWidget::sizeHint().expandedTo(QSize(320, 180));
}

bool MultilineTextEditor::eventFilter(QObject *watched, QEvent *event)
{
    if ( watched == textEdit && event->type() == QEvent::KeyPress )
    {
        const QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

        if ( keyEvent->key() == Qt::Key_Escape )
        {
            requestCancel();
            return true;
        }

        if ( (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
             && keyEvent->modifiers().testFlag(Qt::ControlModifier) )
        {
            requestSave();
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

bool MultilineTextEditor::ownsFocus(const QWidget *widget) const
{
    return widget && (widget == this || isAncestorOf(widget));
}

void MultilineTextEditor::requestSave()
{
    if ( finished )
        return;

    finished = true;
    emit saveRequested();
}

void MultilineTextEditor::requestCancel()
{
    if ( finished )
        return;

    finished = true;
    emit cancelRequested();
}

MultilineTextDelegate::MultilineTextDelegate(QObject *parent) :
    QStyledItemDelegate(parent)
{
}

QWidget *MultilineTextDelegate::createEditor(QWidget *parent,
                                             const QStyleOptionViewItem &,
                                             const QModelIndex &) const
{
    MultilineTextEditor *editor = new MultilineTextEditor(parent);
    connect(editor, &MultilineTextEditor::saveRequested,
            this, &MultilineTextDelegate::saveEditor);
    connect(editor, &MultilineTextEditor::cancelRequested,
            this, &MultilineTextDelegate::cancelEditor);
    return editor;
}

void MultilineTextDelegate::setEditorData(QWidget *editor,
                                          const QModelIndex &index) const
{
    static_cast<MultilineTextEditor *>(editor)->setText(index.data(Qt::EditRole).toString());
}

void MultilineTextDelegate::setModelData(QWidget *editor,
                                         QAbstractItemModel *model,
                                         const QModelIndex &index) const
{
    model->setData(index, static_cast<MultilineTextEditor *>(editor)->text(), Qt::EditRole);
}

void MultilineTextDelegate::updateEditorGeometry(QWidget *editor,
                                                 const QStyleOptionViewItem &option,
                                                 const QModelIndex &) const
{
    QRect geometry(option.rect.topLeft(), editor->sizeHint());

    if ( const QWidget *viewport = editor->parentWidget() )
    {
        geometry.setSize(geometry.size().boundedTo(viewport->size()));

        if ( geometry.right() > viewport->rect().right() )
            geometry.moveRight(viewport->rect().right());
        if ( geometry.bottom() > viewport->rect().bottom() )
            geometry.moveBottom(viewport->rect().bottom());
    }

    editor->setGeometry(geometry);
}

QString MultilineTextDelegate::displayText(const QVariant &value,
                                           const QLocale &) const
{
    return value.toString().simplified();
}

void MultilineTextDelegate::saveEditor()
{
    MultilineTextEditor *editor = qobject_cast<MultilineTextEditor *>(sender());

    if ( editor )
    {
        emit commitData(editor);
        emit closeEditor(editor, QAbstractItemDelegate::NoHint);
    }
}

void MultilineTextDelegate::cancelEditor()
{
    MultilineTextEditor *editor = qobject_cast<MultilineTextEditor *>(sender());

    if ( editor )
        emit closeEditor(editor, QAbstractItemDelegate::NoHint);
}
