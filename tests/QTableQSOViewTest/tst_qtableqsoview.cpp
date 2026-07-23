#include <QAbstractTableModel>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QStyledItemDelegate>
#include <QTest>
#include <QVariantMap>

#include "models/LogbookModel.h"
#include "ui/QTableQSOView.h"

class TestTableModel : public QAbstractTableModel
{
public:
    explicit TestTableModel(int rows, QObject *parent = nullptr) :
        QAbstractTableModel(parent),
        values(rows, QVector<QVariant>(LogbookModel::COLUMN_MODE_SUBMODE + 1))
    {
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : values.size();
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : LogbookModel::COLUMN_MODE_SUBMODE + 1;
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if ( !index.isValid() || (role != Qt::DisplayRole && role != Qt::EditRole) )
            return QVariant();

        if ( index.column() == LogbookModel::COLUMN_MODE_SUBMODE )
        {
            QVariantMap modeSubmode;
            modeSubmode.insert("mode", values.at(index.row()).at(LogbookModel::COLUMN_MODE));
            modeSubmode.insert("submode", values.at(index.row()).at(LogbookModel::COLUMN_SUBMODE));
            return role == Qt::EditRole
                   ? QVariant(modeSubmode)
                   : QVariant(modeSubmode.value("submode").toString().isEmpty()
                              ? modeSubmode.value("mode")
                              : modeSubmode.value("submode"));
        }

        return values.at(index.row()).at(index.column());
    }

    bool setData(const QModelIndex &index, const QVariant &value, int role) override
    {
        if ( !index.isValid() || role != Qt::EditRole )
            return false;

        if ( index.column() == LogbookModel::COLUMN_MODE_SUBMODE )
        {
            const QVariantMap modeSubmode = value.toMap();
            values[index.row()][LogbookModel::COLUMN_MODE] = modeSubmode.value("mode");
            values[index.row()][LogbookModel::COLUMN_SUBMODE] = modeSubmode.value("submode");
            modeSubmodeWrites.append(index.row());
        }
        else
        {
            values[index.row()][index.column()] = value;
            regularWrites.append(qMakePair(index.row(), index.column()));
        }

        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
        return true;
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        return QAbstractTableModel::flags(index) | Qt::ItemIsEditable;
    }

    void setValue(int row, int column, const QVariant &value)
    {
        values[row][column] = value;
    }

    QList<int> modeSubmodeWrites;
    QList<QPair<int, int>> regularWrites;

private:
    QVector<QVector<QVariant>> values;
};

class OversizedTextDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &) const override
    {
        return new QPlainTextEdit(parent);
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        static_cast<QPlainTextEdit *>(editor)->setPlainText(index.data(Qt::EditRole).toString());
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        model->setData(index, static_cast<QPlainTextEdit *>(editor)->toPlainText(),
                       Qt::EditRole);
    }

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &) const override
    {
        editor->setGeometry(option.rect.x(), option.rect.y(), 240, 120);
    }
};

class ModeSubmodeTestDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &) const override
    {
        return new QLineEdit(parent);
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        const QVariantMap value = index.data(Qt::EditRole).toMap();
        static_cast<QLineEdit *>(editor)->setText(value.value("mode").toString()
                                                  + "/"
                                                  + value.value("submode").toString());
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        const QStringList parts = static_cast<QLineEdit *>(editor)->text().split('/');
        QVariantMap value;
        value.insert("mode", parts.value(0));
        value.insert("submode", parts.value(1));
        model->setData(index, value, Qt::EditRole);
    }
};

class QTableQSOViewTest : public QObject
{
    Q_OBJECT

private slots:
    void oversizedEditorCommitsSourceRow();
    void groupEditUsesSelectionAtEditStart();
    void modeSubmodeBatchUsesVirtualColumn();
};

void QTableQSOViewTest::oversizedEditorCommitsSourceRow()
{
    TestTableModel model(4);
    const int column = LogbookModel::COLUMN_NOTES;
    model.setValue(0, column, "row 0");
    model.setValue(1, column, "row 1");
    model.setValue(2, column, "row 2");

    QTableQSOView view;
    view.setModel(&model);
    view.setItemDelegateForColumn(column, new OversizedTextDelegate(&view));
    view.resize(500, 300);
    view.show();

    const QModelIndex sourceIndex = model.index(0, column);
    view.setCurrentIndex(sourceIndex);
    view.selectionModel()->select(sourceIndex,
                                  QItemSelectionModel::ClearAndSelect
                                  | QItemSelectionModel::Rows);
    view.edit(sourceIndex);

    QPlainTextEdit *editor = view.findChild<QPlainTextEdit *>();
    QVERIFY(editor);
    editor->setPlainText("changed");
    view.commitData(editor);

    QCOMPARE(model.data(sourceIndex, Qt::EditRole).toString(), QString("changed"));
    QCOMPARE(model.data(model.index(1, column), Qt::EditRole).toString(), QString("row 1"));
    QCOMPARE(model.data(model.index(2, column), Qt::EditRole).toString(), QString("row 2"));
}

void QTableQSOViewTest::groupEditUsesSelectionAtEditStart()
{
    TestTableModel model(3);
    const int column = LogbookModel::COLUMN_NOTES;
    model.setValue(0, column, "row 0");
    model.setValue(1, column, "row 1");
    model.setValue(2, column, "row 2");

    QTableQSOView view;
    view.setModel(&model);
    view.resize(500, 300);
    view.show();

    const QModelIndex sourceIndex = model.index(0, column);
    view.setCurrentIndex(sourceIndex);
    view.selectionModel()->select(sourceIndex,
                                  QItemSelectionModel::ClearAndSelect
                                  | QItemSelectionModel::Rows);
    view.selectionModel()->select(model.index(1, column),
                                  QItemSelectionModel::Select
                                  | QItemSelectionModel::Rows);
    view.edit(sourceIndex);

    QLineEdit *editor = view.findChild<QLineEdit *>();
    QVERIFY(editor);
    editor->setText("batch value");

    const QModelIndex laterSelection = model.index(2, column);
    view.setCurrentIndex(laterSelection);
    view.selectionModel()->select(laterSelection,
                                  QItemSelectionModel::ClearAndSelect
                                  | QItemSelectionModel::Rows);
    view.commitData(editor);

    QCOMPARE(model.data(sourceIndex, Qt::EditRole).toString(), QString("batch value"));
    QCOMPARE(model.data(model.index(1, column), Qt::EditRole).toString(), QString("batch value"));
    QCOMPARE(model.data(laterSelection, Qt::EditRole).toString(), QString("row 2"));
}

void QTableQSOViewTest::modeSubmodeBatchUsesVirtualColumn()
{
    TestTableModel model(2);
    model.setValue(0, LogbookModel::COLUMN_MODE, "MFSK");
    model.setValue(0, LogbookModel::COLUMN_SUBMODE, "FT4");
    model.setValue(1, LogbookModel::COLUMN_MODE, "SSB");
    model.setValue(1, LogbookModel::COLUMN_SUBMODE, QVariant());

    QTableQSOView view;
    view.setModel(&model);
    view.setItemDelegateForColumn(LogbookModel::COLUMN_MODE_SUBMODE,
                                  new ModeSubmodeTestDelegate(&view));
    view.resize(500, 300);
    view.show();

    const QModelIndex sourceIndex = model.index(0, LogbookModel::COLUMN_MODE_SUBMODE);
    view.setCurrentIndex(sourceIndex);
    view.selectionModel()->select(sourceIndex,
                                  QItemSelectionModel::ClearAndSelect
                                  | QItemSelectionModel::Rows);
    view.selectionModel()->select(model.index(1, LogbookModel::COLUMN_MODE_SUBMODE),
                                  QItemSelectionModel::Select
                                  | QItemSelectionModel::Rows);
    view.edit(sourceIndex);

    QLineEdit *editor = view.findChild<QLineEdit *>();
    QVERIFY(editor);
    editor->setText("MFSK/FT8");
    view.commitData(editor);

    QCOMPARE(model.modeSubmodeWrites, QList<int>({0, 1}));
    QVERIFY(!model.regularWrites.contains(qMakePair(1, LogbookModel::COLUMN_MODE)));
    QVERIFY(!model.regularWrites.contains(qMakePair(1, LogbookModel::COLUMN_SUBMODE)));
    QCOMPARE(model.data(model.index(1, LogbookModel::COLUMN_MODE), Qt::EditRole).toString(),
             QString("MFSK"));
    QCOMPARE(model.data(model.index(1, LogbookModel::COLUMN_SUBMODE), Qt::EditRole).toString(),
             QString("FT8"));
}

QTEST_MAIN(QTableQSOViewTest)

#include "tst_qtableqsoview.moc"
