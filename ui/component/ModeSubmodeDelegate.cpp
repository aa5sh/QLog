#include "ModeSubmodeDelegate.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QHBoxLayout>
#include <QVariantMap>

#include "models/LogbookModel.h"
#include "ui/ModeSelectionController.h"

ModeSubmodeEditor::ModeSubmodeEditor(bool showMode, QWidget *parent) :
    QWidget(parent),
    modeCombo(new QComboBox(this)),
    submodeCombo(new QComboBox(this)),
    modeController(nullptr),
    finished(false)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(modeCombo);
    layout->addWidget(submodeCombo);

    if ( !showMode )
        modeCombo->hide();

    setFocusProxy(showMode ? modeCombo : submodeCombo);

    // Existing QSOs may contain modes disabled for new contacts. Both the
    // merged editor and the submode-only editor must therefore offer all modes.
    modeController = new ModeSelectionController(modeCombo, submodeCombo,
                                                 false, false, false, false, this);
    connect(modeCombo, &QComboBox::currentTextChanged,
            modeController, &ModeSelectionController::applyCurrentMode);
    modeCombo->installEventFilter(this);
    submodeCombo->installEventFilter(this);
    connect(qApp, &QApplication::focusChanged, this,
            [this](QWidget *old, QWidget *now)
            {
                if ( !ownsFocus(now)
                     && !(ownsFocus(old) && QApplication::activePopupWidget()) )
                    requestSave();
            });
}

void ModeSubmodeEditor::setModeSubmode(const QString &mode, const QString &submode)
{
    modeCombo->setCurrentText(mode);
    modeController->applyCurrentMode();
    submodeCombo->setCurrentText(submode);
}

QString ModeSubmodeEditor::mode() const
{
    return modeCombo->currentText();
}

QString ModeSubmodeEditor::submode() const
{
    return submodeCombo->currentText();
}

bool ModeSubmodeEditor::eventFilter(QObject *watched, QEvent *event)
{
    QComboBox *combo = qobject_cast<QComboBox *>(watched);

    if ( combo && event->type() == QEvent::KeyPress )
    {
        const QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        const bool popupVisible = combo->view() && combo->view()->isVisible();

        if ( keyEvent->key() == Qt::Key_Escape && !popupVisible )
        {
            requestCancel();
            return true;
        }

        if ( (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
             && !popupVisible )
        {
            requestSave();
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

bool ModeSubmodeEditor::ownsFocus(const QWidget *widget) const
{
    if ( !widget )
        return false;

    if ( widget == this || isAncestorOf(widget) )
        return true;

    const auto popupOwnsFocus = [widget](const QComboBox *combo)
    {
        const QAbstractItemView *view = combo->view();
        const QWidget *popup = view ? view->window() : nullptr;
        return (view && (widget == view || view->isAncestorOf(widget)))
               || (popup && (widget == popup || popup->isAncestorOf(widget)));
    };

    return popupOwnsFocus(modeCombo) || popupOwnsFocus(submodeCombo);
}

void ModeSubmodeEditor::requestSave()
{
    if ( finished )
        return;

    finished = true;
    emit saveRequested();
}

void ModeSubmodeEditor::requestCancel()
{
    if ( finished )
        return;

    finished = true;
    emit cancelRequested();
}

ModeSubmodeDelegate::ModeSubmodeDelegate(QObject *parent) :
    QStyledItemDelegate(parent)
{
}

QWidget *ModeSubmodeDelegate::createEditor(QWidget *parent,
                                           const QStyleOptionViewItem &,
                                           const QModelIndex &) const
{
    ModeSubmodeEditor *editor = new ModeSubmodeEditor(true, parent);
    ModeSubmodeDelegate *delegate = const_cast<ModeSubmodeDelegate *>(this);
    connect(editor, &ModeSubmodeEditor::saveRequested, delegate,
            [delegate, editor]()
            {
                emit delegate->commitData(editor);
                emit delegate->closeEditor(editor, QAbstractItemDelegate::NoHint);
            });
    connect(editor, &ModeSubmodeEditor::cancelRequested, delegate,
            [delegate, editor]()
            {
                emit delegate->closeEditor(editor, QAbstractItemDelegate::NoHint);
            });
    return editor;
}

void ModeSubmodeDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    ModeSubmodeEditor *modeSubmodeEditor = static_cast<ModeSubmodeEditor *>(editor);
    const QVariantMap value = index.model()->data(index, Qt::EditRole).toMap();

    modeSubmodeEditor->setModeSubmode(value.value("mode").toString(),
                                      value.value("submode").toString());
}

void ModeSubmodeDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                       const QModelIndex &index) const
{
    ModeSubmodeEditor *modeSubmodeEditor = static_cast<ModeSubmodeEditor *>(editor);

    QVariantMap value;
    value.insert("mode", modeSubmodeEditor->mode());
    value.insert("submode", modeSubmodeEditor->submode());

    model->setData(index, value, Qt::EditRole);
}

void ModeSubmodeDelegate::updateEditorGeometry(QWidget *editor,
                                               const QStyleOptionViewItem &option,
                                               const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}

SubmodeDelegate::SubmodeDelegate(QObject *parent) :
    QStyledItemDelegate(parent)
{
}

QWidget *SubmodeDelegate::createEditor(QWidget *parent,
                                       const QStyleOptionViewItem &,
                                       const QModelIndex &) const
{
    ModeSubmodeEditor *editor = new ModeSubmodeEditor(false, parent);
    SubmodeDelegate *delegate = const_cast<SubmodeDelegate *>(this);
    connect(editor, &ModeSubmodeEditor::saveRequested, delegate,
            [delegate, editor]()
            {
                emit delegate->commitData(editor);
                emit delegate->closeEditor(editor, QAbstractItemDelegate::NoHint);
            });
    connect(editor, &ModeSubmodeEditor::cancelRequested, delegate,
            [delegate, editor]()
            {
                emit delegate->closeEditor(editor, QAbstractItemDelegate::NoHint);
            });
    return editor;
}

void SubmodeDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    ModeSubmodeEditor *modeSubmodeEditor = static_cast<ModeSubmodeEditor *>(editor);

    const QAbstractItemModel *model = index.model();

    if ( !model )
        return;

    const QString mode = model->data(index.sibling(index.row(), LogbookModel::COLUMN_MODE),
                                             Qt::DisplayRole).toString();
    const QString submode = model->data(index, Qt::EditRole).toString();

    modeSubmodeEditor->setModeSubmode(mode, submode);
}

void SubmodeDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                   const QModelIndex &index) const
{
    ModeSubmodeEditor *modeSubmodeEditor = static_cast<ModeSubmodeEditor *>(editor);
    const QString submode = modeSubmodeEditor->submode();
    model->setData(index, submode.isEmpty() ? QVariant() : QVariant(submode), Qt::EditRole);
}

void SubmodeDelegate::updateEditorGeometry(QWidget *editor,
                                           const QStyleOptionViewItem &option,
                                           const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}
