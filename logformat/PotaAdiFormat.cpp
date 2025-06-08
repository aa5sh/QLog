#include "PotaAdiFormat.h"
#include <QDebug>
#include <QSqlField>
#include <QSqlRecord>
#include "core/debug.h"
#include <models/LogbookModel.h>

MODULE_IDENTIFICATION("qlog.logformat.potalogformat");

PotaAdiFormat::PotaAdiFormat(QTextStream &stream) :
    AdiFormat(stream),
    currentDate(QDateTime::currentDateTime())
{
    FCT_IDENTIFICATION;
}

void PotaAdiFormat::setExportDirectory(const QString &dir)
{
    FCT_IDENTIFICATION;

    exportDir = dir;
}

void PotaAdiFormat::exportContact(const QSqlRecord &sourceRecord, QMap<QString, QString> *applTags)
{
    FCT_IDENTIFICATION;

    if ( exportDir.isEmpty() || !isValidPotaRecord(sourceRecord) )
        return;

    QSqlRecord inputRecord(sourceRecord);

    preparePotaField(inputRecord, "my_pota_ref", "my_sig_info", "my_sig");
    preparePotaField(inputRecord, "pota_ref", "sig_info", "sig");

    QList<QSqlRecord> expandedRecords({inputRecord});

    // Expand records based on specific fields - one record for the specific POTA Ref
    expandParkRecord(expandedRecords, "my_sig_info");
    expandParkRecord(expandedRecords, "sig_info");

    for ( const QSqlRecord &record : static_cast<const QList<QSqlRecord>&>(expandedRecords) )
    {
        const QString &mySig = record.value("my_sig").toString().toLower();

        // export Activator Log
        if ( mySig == "pota" )
        {
            if ( AdiFormat *parkOut = this->getActivatorParkFormatter(record) )
            {
                parkOut->exportContact(record, applTags);
                continue;
            }
        }
        // export Hunter Log
        else if ( record.value("sig").toString().toLower() == "pota" && mySig.isEmpty() )
            AdiFormat::exportContact(record, applTags);
    }
}

AdiFormat *PotaAdiFormat::getActivatorParkFormatter(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    // https://docs.pota.app/docs/activator_reference/logging_made_easy.html#naming-your-files
    // station_callsign@park#-yyyymmdd
    const QString parkFileName = QString("%1@%2-%3.adif")
                                .arg(record.value("station_callsign").toString(),
                                     record.value("my_sig_info").toString(),
                                     currentDate.toString("yyyyMMdd-hhmm"));

    if ( parkFormatters.contains(parkFileName) )
    {
        qCDebug(runtime) << "Using park file " << parkFileName;
        return parkFormatters[parkFileName]->formatter;
    }

    ParkFormatter *parkFormatter = new ParkFormatter();

    parkFormatter->file = new QFile(exportDir + QDir::separator() + parkFileName);
    if ( !parkFormatter->file->open(QFile::WriteOnly | QFile::Text) )
    {
        qCWarning(runtime) << "Could not open POTA park file for writing "
                           << exportDir + QDir::separator() + parkFileName;
        delete parkFormatter;
        return nullptr;
    }

    parkFormatter->stream = new QTextStream(parkFormatter->file);
    if ( !parkFormatter->stream )
    {
        qCWarning(runtime) << "Cannot allocate QTextStream";
        delete parkFormatter;
        return nullptr;
    }

    parkFormatter->formatter = new AdiFormat(*parkFormatter->stream);
    if ( !parkFormatter->formatter )
    {
        qCWarning(runtime) << "Cannot allocate AdifFormatter";
        delete parkFormatter;
        return nullptr;
    }

    parkFormatters[parkFileName] = parkFormatter;
    parkFormatter->formatter->exportStart();
    return parkFormatter->formatter;
}

void PotaAdiFormat::expandParkRecord(QList<QSqlRecord> &inputList, const QString &columnName)
{
    FCT_IDENTIFICATION;

    QList<QSqlRecord> expandedNewRecords;

    // can contain multiple parks as a csv:
    // <MY_POTA_REF:40>K-0817,K-4566,K-4576,K-4573,K-4578@US-WY

    for ( auto it = inputList.cbegin(); it != inputList.cend(); it++ )
    {
        QStringList activatedParks = it->value(columnName).toString().split(",",
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
                                                                              Qt::SplitBehaviorFlags::SkipEmptyParts);
#else
                                                                              QString::SkipEmptyParts);
#endif
        for ( QString &str : activatedParks )
            str = str.trimmed();

        if ( activatedParks.size() <= 1 )
        {
            expandedNewRecords.append(*it);
            continue;
        }

        for ( const QString &parkID : static_cast<const QStringList&>(activatedParks) )
        {
            QSqlRecord newRecord(*it);
            newRecord.setValue(columnName, parkID);
            expandedNewRecords.append(newRecord);
        }
    }

    inputList.swap(expandedNewRecords);
}

void PotaAdiFormat::exportEnd()
{
    FCT_IDENTIFICATION;

    const QList<ParkFormatter*> &formatters = parkFormatters.values();

    for ( ParkFormatter *parkFormat :  formatters)
        parkFormat->formatter->exportEnd();
}

void PotaAdiFormat::moveFieldValue(QSqlRecord &record,
                                   const QString &fromFieldName,
                                   const QString &toFieldName)
{
    FCT_IDENTIFICATION;

    QSqlField dupped(record.field(fromFieldName));
    record.remove(record.indexOf(fromFieldName));
    record.remove(record.indexOf(toFieldName));
    dupped.setName(toFieldName);
    record.append(dupped);
}

bool PotaAdiFormat::isValidPotaRecord(const QSqlRecord &record) const
{
    FCT_IDENTIFICATION;

    auto isPotaWithEmptyInfo = [](const QString &sig, const QString &info)
    {
        return sig == "pota" && info.isEmpty();
    };

    const QString &sigField = record.value("sig").toString().toLower();
    const QString &mysigField = record.value("my_sig").toString().toLower();

    return !record.value("my_pota_ref").toString().isEmpty()
           || !record.value("pota_ref").toString().isEmpty()
           || (sigField == "pota" && !isPotaWithEmptyInfo(sigField, record.value("sig_info").toString()))
           || (mysigField == "pota" && !isPotaWithEmptyInfo(mysigField, record.value("my_sig_info").toString()));
}

void PotaAdiFormat::preparePotaField(QSqlRecord &record, const QString &fromField, const QString &toField, const QString &toFieldSig)
{
    FCT_IDENTIFICATION;

    if (record.value(fromField).toString().isEmpty())
        return;

    moveFieldValue(record, fromField, toField);
    record.setValue(toFieldSig, "POTA");

}

PotaAdiFormat::~PotaAdiFormat()
{
    FCT_IDENTIFICATION;

    qDeleteAll(parkFormatters);
    parkFormatters.clear();
}
