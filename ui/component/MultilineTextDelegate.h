#ifndef QLOG_UI_MULTILINETEXTDELEGATE_H
#define QLOG_UI_MULTILINETEXTDELEGATE_H

#include <QStyledItemDelegate>
#include <QWidget>

class QPlainTextEdit;

class MultilineTextEditor : public QWidget
{
    Q_OBJECT

public:
    explicit MultilineTextEditor(QWidget *parent = nullptr);

    QString text() const;
    void setText(const QString &text);
    QSize sizeHint() const override;

signals:
    void saveRequested();
    void cancelRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool ownsFocus(const QWidget *widget) const;
    void requestSave();
    void requestCancel();

    QPlainTextEdit *textEdit;
    bool finished;
};

class MultilineTextDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit MultilineTextDelegate(QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor,
                              const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override;
    QString displayText(const QVariant &value, const QLocale &locale) const override;

private slots:
    void saveEditor();
    void cancelEditor();
};

#endif // QLOG_UI_MULTILINETEXTDELEGATE_H
