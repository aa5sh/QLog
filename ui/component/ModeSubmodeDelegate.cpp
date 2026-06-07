#include "ModeSubmodeDelegate.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
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
    modeController(nullptr)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(modeCombo);
    layout->addWidget(submodeCombo);

    if ( !showMode )
        modeCombo->hide();

    // Existing QSOs may contain modes disabled for new contacts. Both the
    // merged editor and the submode-only editor must therefore offer all modes.
    modeController = new ModeSelectionController(modeCombo, submodeCombo,
                                                 false, false, false, false, this);
    connect(modeCombo, &QComboBox::currentTextChanged,
            modeController, &ModeSelectionController::applyCurrentMode);
}

void ModeSubmodeEditor::setModeSubmode(const QString &mode, const QString &submode)
{
    modeCombo->setCurrentText(mode);
    modeController->applyCurrentMode();
    submodeCombo->setCurrentText(submode);
}

void ModeSubmodeEditor::installComboEventFilter(QObject *filter)
{
    modeCombo->installEventFilter(filter);
    submodeCombo->installEventFilter(filter);
}

QString ModeSubmodeEditor::mode() const
{
    return modeCombo->currentText();
}

QString ModeSubmodeEditor::submode() const
{
    return submodeCombo->currentText();
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
    editor->installComboEventFilter(delegate);
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

bool ModeSubmodeDelegate::eventFilter(QObject *object, QEvent *event)
{
    QComboBox *combo = qobject_cast<QComboBox *>(object);
    if ( combo )
    {
        if ( event->type() == QEvent::KeyPress )
        {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if ( keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter )
            {
                if ( combo->view() && combo->view()->isVisible() )
                    return false;

                QWidget *editor = combo->parentWidget();
                if ( editor )
                {
                    emit commitData(editor);
                    emit closeEditor(editor, QAbstractItemDelegate::NoHint);
                    return true;
                }
            }
        }

        return false;
    }

    return QStyledItemDelegate::eventFilter(object, event);
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
    editor->installComboEventFilter(delegate);
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

bool SubmodeDelegate::eventFilter(QObject *object, QEvent *event)
{
    QComboBox *combo = qobject_cast<QComboBox *>(object);
    if ( combo )
    {
        if ( event->type() == QEvent::KeyPress )
        {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if ( keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter )
            {
                if ( combo->view() && combo->view()->isVisible() )
                    return false;

                QWidget *editor = combo->parentWidget();
                if ( editor )
                {
                    emit commitData(editor);
                    emit closeEditor(editor, QAbstractItemDelegate::NoHint);
                    return true;
                }
            }
        }

        return false;
    }

    return QStyledItemDelegate::eventFilter(object, event);
}
