#include "AwardDXMarathon.h"

#include <QComboBox>
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
#include "data/DXMarathon.h"
#include "data/StationProfile.h"
#include "logformat/AdiFormat.h"

MODULE_IDENTIFICATION("qlog.awards.dxmarathon");

QString AwardDXMarathon::displayName() const
{
    FCT_IDENTIFICATION;

    return QCoreApplication::translate("AwardsDialog", "DX Marathon");
}

QString AwardDXMarathon::rulesUrl() const
{
    FCT_IDENTIFICATION;

    // The site keeps one rules page per year: /rules/<year>/.
    const int year = yearSpin ? yearSpin->value() : DXMarathon::currentYear();
    return QStringLiteral("https://www.dxmarathon.com/rules/%1/").arg(year);
}

QWidget *AwardDXMarathon::createWidget(QWidget *parent)
{
    FCT_IDENTIFICATION;
    QWidget *widget = new QWidget(parent);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    QHBoxLayout *controls = new QHBoxLayout;
    profileCombo = new QComboBox(widget);
    for ( const QString &name : StationProfilesManager::instance()->profileNameList() )
        profileCombo->addItem(name, name);
    profileMatchCombo = new QComboBox(widget);
    profileMatchCombo->addItem(QCoreApplication::translate("AwardsDialog", "Station Profile"), QStringLiteral("profile"));
    profileMatchCombo->addItem(QCoreApplication::translate("AwardsDialog", "Callsign only"), QStringLiteral("callsign"));
    profileMatchCombo->setToolTip(QCoreApplication::translate("AwardsDialog", "Callsign-only matching can combine contacts from different operating locations. Review the exported log before submitting."));
    yearSpin = new QSpinBox(widget);
    yearSpin->setRange(2006, 2100);
    yearSpin->setValue(DXMarathon::currentYear());
    QPushButton *exportButton = new QPushButton(QCoreApplication::translate("AwardsDialog", "Export submission ADIF"), widget);
    QPushButton *submitButton = new QPushButton(QCoreApplication::translate("AwardsDialog", "Open submission tool"), widget);
    scoreLabel = new QLabel(widget);
    controls->addWidget(new QLabel(QCoreApplication::translate("AwardsDialog", "Station Profile"), widget));
    controls->addWidget(profileCombo);
    controls->addWidget(new QLabel(QCoreApplication::translate("AwardsDialog", "Match"), widget));
    controls->addWidget(profileMatchCombo);
    controls->addWidget(new QLabel(QCoreApplication::translate("AwardsDialog", "Year"), widget));
    controls->addWidget(yearSpin);
    controls->addWidget(exportButton);
    controls->addWidget(submitButton);
    controls->addStretch();
    controls->addWidget(scoreLabel);
    layout->addLayout(controls);
    resultTable = new QTableView(widget);
    resultModel = new QSqlQueryModel(resultTable);
    resultTable->setModel(resultModel);
    resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resultTable->setAlternatingRowColors(true);
    resultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    layout->addWidget(resultTable);
    QObject::connect(profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), widget, [this]{ refresh(); });
    QObject::connect(profileMatchCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), widget, [this]{ refresh(); });
    QObject::connect(yearSpin, QOverload<int>::of(&QSpinBox::valueChanged), widget, [this]{ refresh(); });
    QObject::connect(exportButton, &QPushButton::clicked, widget, [this]{ exportAdif(); });
    QObject::connect(submitButton, &QPushButton::clicked, widget, []{ QDesktopServices::openUrl(QUrl(QStringLiteral("https://entry.dxmarathon.com"))); });
    m_widget = widget;
    return widget;
}

QString AwardDXMarathon::contactPredicate(bool inlineValues) const
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << inlineValues;

    if ( !profileCombo || profileCombo->currentIndex() < 0 )
        return QStringLiteral("0");

    const StationProfile profile = StationProfilesManager::instance()->getProfile(profileCombo->currentData().toString());
    const bool byProfile = ( profileMatchCombo->currentData().toString() == QLatin1String("profile") );

    // Values are bound (see bindPredicate) except for getConditionSelected(),
    // whose filter the award framework requires as plain SQL text; only
    // there are they inlined, quoted in this one place.
    const auto value = [inlineValues](const QString &placeholder, const QString &text)
    {
        return inlineValues ? QLatin1Char('\'') + QString(text).replace(QLatin1Char('\''), QLatin1String("''")) + QLatin1Char('\'')
                            : placeholder;
    };

    // getContactInnerJoin() is upstream's station-profile matching SQL.
    const QString stationMatch = byProfile
        ? QStringLiteral("EXISTS (SELECT 1 FROM station_profiles WHERE profile_name = %1 AND %2)")
              .arg(value(QStringLiteral(":dxmProfile"), profile.profileName), profile.getContactInnerJoin())
        : QStringLiteral("UPPER(station_callsign) = UPPER(%1)")
              .arg(value(QStringLiteral(":dxmCallsign"), profile.callsign));

    return stationMatch
         + QStringLiteral(" AND start_time >= %1 AND start_time < %2 AND ")
               .arg(value(QStringLiteral(":dxmStart"), DXMarathon::yearStart(yearSpin->value())),
                    value(QStringLiteral(":dxmEnd"), DXMarathon::yearStart(yearSpin->value() + 1)))
         + DXMarathon::eligibleContactsCondition()
         + QLatin1Char(' ') + userFilter;
}

void AwardDXMarathon::bindPredicate(QSqlQuery &query) const
{
    FCT_IDENTIFICATION;

    if ( !profileCombo || profileCombo->currentIndex() < 0 )
        return;

    const StationProfile profile = StationProfilesManager::instance()->getProfile(profileCombo->currentData().toString());
    // Bind only the placeholder contactPredicate() emitted for this mode.
    if ( profileMatchCombo->currentData().toString() == QLatin1String("profile") )
        query.bindValue(QStringLiteral(":dxmProfile"), profile.profileName);
    else
        query.bindValue(QStringLiteral(":dxmCallsign"), profile.callsign);
    query.bindValue(QStringLiteral(":dxmStart"), DXMarathon::yearStart(yearSpin->value()));
    query.bindValue(QStringLiteral(":dxmEnd"), DXMarathon::yearStart(yearSpin->value() + 1));
}

QString AwardDXMarathon::entityKindLabel()
{
    FCT_IDENTIFICATION;

    return QCoreApplication::translate("AwardsDialog", "Entity");
}

QString AwardDXMarathon::zoneKindLabel()
{
    FCT_IDENTIFICATION;

    return QCoreApplication::translate("AwardsDialog", "Zone");
}

QString AwardDXMarathon::workedLabel()
{
    FCT_IDENTIFICATION;

    return QCoreApplication::translate("AwardsDialog", "Worked");
}

void AwardDXMarathon::updateData(const AwardFilterParams &params)
{
    FCT_IDENTIFICATION;

    userFilter = params.userFilterWhereClause;
    refresh();
}

void AwardDXMarathon::refresh()
{
    FCT_IDENTIFICATION;

    if ( !resultModel )
        return;

    const QString eligible = contactPredicate(false);
    // Displayed labels are bound so they can be translated.
    const QString sql = QStringLiteral(
        "WITH eligible AS (SELECT * FROM contacts WHERE %1), targets(kind,sort_key,item,name) AS ("
        " SELECT :entityKind, id, CAST(id AS TEXT), translate_to_locale(name) FROM dxcc_entities_clublog WHERE deleted=0"
        " UNION ALL SELECT :zoneKind, 1000+n, CAST(n AS TEXT), :zonePrefix || n FROM (WITH RECURSIVE z(n) AS (SELECT 1 UNION ALL SELECT n+1 FROM z WHERE n<40) SELECT n FROM z)),"
        " hits AS (SELECT :entityKind kind, CAST(dxcc AS TEXT) item, MIN(start_time) first_qso FROM eligible WHERE dxcc>0 GROUP BY dxcc"
        " UNION ALL SELECT :zoneKind, CAST(cqz AS TEXT), MIN(start_time) FROM eligible WHERE cqz BETWEEN 1 AND 40 GROUP BY cqz)"
        " SELECT t.kind, t.item, t.name, CASE WHEN h.item IS NULL THEN :notWorked ELSE :worked END status,"
        " h.first_qso FROM targets t LEFT JOIN hits h ON h.kind=t.kind AND h.item=t.item ORDER BY t.sort_key").arg(eligible);

    QSqlQuery query;
    if ( !query.prepare(sql) )
    {
        qCWarning(runtime) << "DX Marathon prepare error:" << query.lastError();
        return;
    }
    bindPredicate(query);
    query.bindValue(QStringLiteral(":entityKind"), entityKindLabel());
    query.bindValue(QStringLiteral(":zoneKind"), zoneKindLabel());
    query.bindValue(QStringLiteral(":zonePrefix"), QCoreApplication::translate("AwardsDialog", "CQ Zone") + QLatin1Char(' '));
    query.bindValue(QStringLiteral(":notWorked"), QCoreApplication::translate("AwardsDialog", "Not worked"));
    query.bindValue(QStringLiteral(":worked"), workedLabel());
    if ( !query.exec() )
        qCWarning(runtime) << "DX Marathon query error:" << query.lastError();

    resultModel->setQuery(std::move(query));
    resultModel->setHeaderData(0, Qt::Horizontal, QCoreApplication::translate("AwardsDialog", "Type"));
    resultModel->setHeaderData(1, Qt::Horizontal, QCoreApplication::translate("AwardsDialog", "Number"));
    resultModel->setHeaderData(2, Qt::Horizontal, QCoreApplication::translate("AwardsDialog", "Entity / Zone"));
    resultModel->setHeaderData(3, Qt::Horizontal, QCoreApplication::translate("AwardsDialog", "Status"));
    resultModel->setHeaderData(4, Qt::Horizontal, QCoreApplication::translate("AwardsDialog", "First QSO"));

    QSqlQuery count;
    if ( !count.prepare(QStringLiteral("SELECT COUNT(DISTINCT CASE WHEN dxcc>0 THEN dxcc END), "
                                       "COUNT(DISTINCT CASE WHEN cqz BETWEEN 1 AND 40 THEN cqz END) "
                                       "FROM contacts WHERE %1").arg(eligible)) )
    {
        qCWarning(runtime) << "DX Marathon score prepare error:" << count.lastError();
        return;
    }
    bindPredicate(count);
    if ( !count.exec() || !count.next() )
    {
        qCWarning(runtime) << "DX Marathon score query error:" << count.lastError();
        return;
    }
    const int entities = count.value(0).toInt();
    const int zones = count.value(1).toInt();
    scoreLabel->setText(QCoreApplication::translate("AwardsDialog", "Score: %1 (%2 entities + %3 zones)")
                         .arg(entities + zones).arg(entities).arg(zones));
}

void AwardDXMarathon::exportAdif()
{
    FCT_IDENTIFICATION;
    const QString fileName = QFileDialog::getSaveFileName(m_widget, QCoreApplication::translate("AwardsDialog", "Export DX Marathon ADIF"), QString("dxmarathon-%1.adi").arg(yearSpin->value()), QStringLiteral("ADIF (*.adi)"));
    if ( fileName.isEmpty() )
        return;
    QFile file(fileName);
    if ( !file.open(QIODevice::WriteOnly | QIODevice::Text) )
    {
        QMessageBox::critical(m_widget, QObject::tr("QLog Error"), file.errorString());
        return;
    }
    QTextStream stream(&file);
    AdiFormat format(stream);
    QSqlQuery query;
    if ( !query.prepare(QStringLiteral("SELECT * FROM contacts WHERE %1 ORDER BY start_time").arg(contactPredicate(false))) )
    {
        qCWarning(runtime) << "DX Marathon export prepare error:" << query.lastError();
        QMessageBox::critical(m_widget, QObject::tr("QLog Error"), query.lastError().text());
        return;
    }
    bindPredicate(query);
    query.exec();
    QList<QSqlRecord> records;
    while ( query.next() )
        records.append(query.record());
    if ( query.lastError().isValid() )
    {
        qCWarning(runtime) << "DX Marathon export query error:" << query.lastError();
        QMessageBox::critical(m_widget, QObject::tr("QLog Error"), query.lastError().text());
        return;
    }
    const long exported = format.runExport(records);
    qCDebug(runtime) << "DX Marathon ADIF exported contacts:" << exported << "file:" << fileName;
    QMessageBox::information(m_widget, QObject::tr("QLog Info"), QCoreApplication::translate("AwardsDialog", "%1 contacts exported. Upload this ADIF to the DX Marathon submission tool.").arg(exported));
}

AwardDefinition::ConditionResult AwardDXMarathon::getConditionSelected(const QModelIndex &index) const
{
    FCT_IDENTIFICATION;

    ConditionResult r;
    if ( !index.isValid() || resultModel->data(resultModel->index(index.row(), 3)).toString() != workedLabel() )
        return r;
    const QString kind = resultModel->data(resultModel->index(index.row(), 0)).toString();
    // The number column is an integer produced by the query, not user text.
    const int item = resultModel->data(resultModel->index(index.row(), 1)).toInt();
    r.filter = QStringLiteral("(%1 = %2 AND %3)").arg(kind == zoneKindLabel() ? QStringLiteral("cqz") : QStringLiteral("dxcc"))
                                                 .arg(item).arg(contactPredicate(true));
    r.valid = true;
    return r;
}
