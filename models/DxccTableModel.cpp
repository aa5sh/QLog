#include <QColor>
#include <QSize>
#include <QFont>
#include <QDebug>
#include <QBrush>
#include "DxccTableModel.h"
#include "data/Data.h"

DxccTableModel::DxccTableModel(QObject* parent) : QSqlQueryModel(parent) {}

QVariant DxccTableModel::data(const QModelIndex &index, int role) const
{

    if ( index.column() == 0 )
        return QSqlQueryModel::data(index, role);

    switch ( role )
    {
    case  Qt::TextAlignmentRole:
       return int(Qt::AlignCenter | Qt::AlignVCenter);
    case  Qt::BackgroundRole:
    {
        const QString &currData = data(index, Qt::DisplayRole).toString();

        if ( currData.contains("L") || currData.contains("P"))
            return Data::statusToColor(DxccStatus::NewMode, false, Qt::green);

        if ( currData == "e" || currData == "W" )
            return Data::statusToColor(DxccStatus::Worked, false, Qt::transparent);
    }
        break;

    case Qt::DisplayRole:
    {
        const QString &currData = QSqlQueryModel::data(index, Qt::DisplayRole).toString();

        if ( currData.isEmpty() || currData.size() < 3 )
            return QString();

        if ( currData == "111" ) return QString("W");

        QString ret;

        if ( currData[0] == '2' ) ret.append("e");
        if ( currData[1] == '2' ) ret.append("L");
        if ( currData[2] == '2' ) ret.append("P");

        return ret;
    }

    case Qt::ToolTipRole:
    {
        const QString &currData = data(index, Qt::DisplayRole).toString();
        QStringList ret;

        if ( currData.contains("W") ) ret.append(tr("Worked"));
        if ( currData.contains("e") ) ret.append(tr("eQSL"));
        if ( currData.contains("L") ) ret.append(tr("LoTW"));
        if ( currData.contains("P") ) ret.append(tr("Paper"));

        return ret.join(", ");
    }
    }

    return QSqlQueryModel::data(index, role);
}
