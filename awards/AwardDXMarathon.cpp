#include "AwardDXMarathon.h"

#include <QComboBox>
#include <QDate>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QTableView>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>

#include "core/debug.h"
#include "data/StationProfile.h"
#include "logformat/AdiFormat.h"

MODULE_IDENTIFICATION("qlog.awards.dxmarathon");

QString AwardDXMarathon::displayName() const
{
    return QCoreApplication::translate("AwardsDialog", "DX Marathon");
}

QString AwardDXMarathon::rulesUrl() const
{
    return QStringLiteral("https://www.dxmarathon.com/rules/2026/");
}

QWidget *AwardDXMarathon::createWidget(QWidget *parent)
{
    FCT_IDENTIFICATION;
    QWidget *widget = new QWidget(parent);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    QHBoxLayout *controls = new QHBoxLayout;
    m_profile = new QComboBox(widget);
    for (const QString &name : StationProfilesManager::instance()->profileNameList())
        m_profile->addItem(name, name);
    m_profileMatch = new QComboBox(widget);
    m_profileMatch->addItem(QCoreApplication::translate("AwardsDialog", "Station Profile"), QStringLiteral("profile"));
    m_profileMatch->addItem(QCoreApplication::translate("AwardsDialog", "Callsign only"), QStringLiteral("callsign"));
    m_profileMatch->setToolTip(QCoreApplication::translate("AwardsDialog", "Callsign-only matching can combine contacts from different operating locations. Review the exported log before submitting."));
    m_year = new QSpinBox(widget);
    m_year->setRange(2006, 2100);
    m_year->setValue(QDate::currentDate().year());
    QPushButton *exportButton = new QPushButton(QCoreApplication::translate("AwardsDialog", "Export submission ADIF"), widget);
    QPushButton *submitButton = new QPushButton(QCoreApplication::translate("AwardsDialog", "Open submission tool"), widget);
    m_score = new QLabel(widget);
    controls->addWidget(new QLabel(QCoreApplication::translate("AwardsDialog", "Station Profile"), widget));
    controls->addWidget(m_profile);
    controls->addWidget(new QLabel(QCoreApplication::translate("AwardsDialog", "Match"), widget));
    controls->addWidget(m_profileMatch);
    controls->addWidget(new QLabel(QCoreApplication::translate("AwardsDialog", "Year"), widget));
    controls->addWidget(m_year);
    controls->addWidget(exportButton);
    controls->addWidget(submitButton);
    controls->addStretch();
    controls->addWidget(m_score);
    layout->addLayout(controls);
    m_table = new QTableView(widget);
    m_model = new QSqlQueryModel(m_table);
    m_table->setModel(m_model);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    layout->addWidget(m_table);
    QObject::connect(m_profile, QOverload<int>::of(&QComboBox::currentIndexChanged), widget, [this]{ refresh(); });
    QObject::connect(m_profileMatch, QOverload<int>::of(&QComboBox::currentIndexChanged), widget, [this]{ refresh(); });
    QObject::connect(m_year, QOverload<int>::of(&QSpinBox::valueChanged), widget, [this]{ refresh(); });
    QObject::connect(exportButton, &QPushButton::clicked, widget, [this]{ exportAdif(); });
    QObject::connect(submitButton, &QPushButton::clicked, widget, []{ QDesktopServices::openUrl(QUrl(QStringLiteral("https://entry.dxmarathon.com"))); });
    m_widget = widget;
    return widget;
}

QString AwardDXMarathon::contactPredicate() const
{
    if (!m_profile || m_profile->currentIndex() < 0)
        return QStringLiteral("0");
    const StationProfile p = StationProfilesManager::instance()->getProfile(m_profile->currentData().toString());
    const QString callsign = QString(p.callsign).replace("'", "''");
    QString stationMatch = QString("UPPER(station_callsign)=UPPER('%1')").arg(callsign);
    if (m_profileMatch->currentData().toString() == QLatin1String("profile"))
        stationMatch = QString("EXISTS (SELECT 1 FROM station_profiles WHERE profile_name='%1' AND %2)")
                           .arg(QString(m_profile->currentData().toString()).replace("'", "''"), p.getContactInnerJoin());
    return QString("%1 "
                   "AND start_time >= '%2-01-01T00:00:00' AND start_time < '%3-01-01T00:00:00' "
                   "AND band IN ('160m','80m','60m','40m','30m','20m','17m','15m','12m','10m','6m') "
                   "AND callsign NOT LIKE '%/AM' AND callsign NOT LIKE '%/MM' %4")
        .arg(stationMatch).arg(m_year->value()).arg(m_year->value() + 1).arg(m_userFilter);
}

void AwardDXMarathon::updateData(const AwardFilterParams &params)
{
    m_userFilter = params.userFilterWhereClause;
    refresh();
}

void AwardDXMarathon::refresh()
{
    FCT_IDENTIFICATION;
    if (!m_model) return;
    const QString eligible = contactPredicate();
    const QString sql = QString(
        "WITH eligible AS (SELECT * FROM contacts WHERE %1), targets(kind,sort_key,item,name) AS ("
        " SELECT 'Entity', id, CAST(id AS TEXT), translate_to_locale(name) FROM dxcc_entities_clublog WHERE deleted=0"
        " UNION ALL SELECT 'Zone', 1000+n, CAST(n AS TEXT), 'CQ Zone '||n FROM (WITH RECURSIVE z(n) AS (SELECT 1 UNION ALL SELECT n+1 FROM z WHERE n<40) SELECT n FROM z)),"
        " hits AS (SELECT 'Entity' kind, CAST(dxcc AS TEXT) item, MIN(start_time) first_qso FROM eligible WHERE dxcc>0 GROUP BY dxcc"
        " UNION ALL SELECT 'Zone', CAST(cqz AS TEXT), MIN(start_time) FROM eligible WHERE cqz BETWEEN 1 AND 40 GROUP BY cqz)"
        " SELECT t.kind, t.item, t.name, CASE WHEN h.item IS NULL THEN 'Not worked' ELSE 'Worked' END status,"
        " h.first_qso FROM targets t LEFT JOIN hits h ON h.kind=t.kind AND h.item=t.item ORDER BY t.sort_key").arg(eligible);
    qCDebug(runtime) << "DX Marathon SQL:" << sql;
    m_model->setQuery(sql);
    if (m_model->lastError().isValid()) qCWarning(runtime) << "DX Marathon query error:" << m_model->lastError();
    m_model->setHeaderData(0, Qt::Horizontal, QCoreApplication::translate("AwardsDialog", "Type"));
    m_model->setHeaderData(1, Qt::Horizontal, QCoreApplication::translate("AwardsDialog", "Number"));
    m_model->setHeaderData(2, Qt::Horizontal, QCoreApplication::translate("AwardsDialog", "Entity / Zone"));
    m_model->setHeaderData(3, Qt::Horizontal, QCoreApplication::translate("AwardsDialog", "Status"));
    m_model->setHeaderData(4, Qt::Horizontal, QCoreApplication::translate("AwardsDialog", "First QSO"));
    QSqlQuery count(QString("SELECT COUNT(DISTINCT CASE WHEN dxcc>0 THEN dxcc END), "
                            "COUNT(DISTINCT CASE WHEN cqz BETWEEN 1 AND 40 THEN cqz END) "
                            "FROM contacts WHERE %1").arg(eligible));
    if (count.next()) m_score->setText(QCoreApplication::translate("AwardsDialog", "Score: %1 (%2 entities + %3 zones)").arg(count.value(0).toInt()+count.value(1).toInt()).arg(count.value(0).toInt()).arg(count.value(1).toInt()));
}

void AwardDXMarathon::exportAdif()
{
    FCT_IDENTIFICATION;
    const QString fileName = QFileDialog::getSaveFileName(m_widget, QCoreApplication::translate("AwardsDialog", "Export DX Marathon ADIF"), QString("dxmarathon-%1.adi").arg(m_year->value()), QStringLiteral("ADIF (*.adi)"));
    if (fileName.isEmpty()) return;
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) { QMessageBox::critical(m_widget, QObject::tr("QLog Error"), file.errorString()); return; }
    QTextStream stream(&file);
    AdiFormat format(stream);
    QSqlQuery query(QString("SELECT * FROM contacts WHERE %1 ORDER BY start_time").arg(contactPredicate()));
    QList<QSqlRecord> records;
    while (query.next()) records.append(query.record());
    if (query.lastError().isValid()) { qCWarning(runtime) << "DX Marathon export query error:" << query.lastError(); QMessageBox::critical(m_widget, QObject::tr("QLog Error"), query.lastError().text()); return; }
    const long exported = format.runExport(records);
    qCDebug(runtime) << "DX Marathon ADIF exported contacts:" << exported << "file:" << fileName;
    QMessageBox::information(m_widget, QObject::tr("QLog Info"), QCoreApplication::translate("AwardsDialog", "%1 contacts exported. Upload this ADIF to the DX Marathon submission tool.").arg(exported));
}

AwardDefinition::ConditionResult AwardDXMarathon::getConditionSelected(const QModelIndex &index) const
{
    ConditionResult r;
    if (!index.isValid() || m_model->data(m_model->index(index.row(), 3)).toString() != QLatin1String("Worked")) return r;
    const QString kind = m_model->data(m_model->index(index.row(), 0)).toString();
    const QString item = m_model->data(m_model->index(index.row(), 1)).toString();
    r.filter = QString("(%1 = '%2' AND %3)").arg(kind == QLatin1String("Zone") ? "cqz" : "dxcc", item, contactPredicate());
    r.valid = true;
    return r;
}
