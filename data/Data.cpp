#include <QJsonDocument>
#include <QSqlQuery>
#include <QSqlError>
#include "Data.h"

Data::Data(QObject *parent) : QObject(parent) {
    loadContests();
    loadPropagationModes();
}

Data* Data::instance() {
    static Data instance;
    return &instance;
}

DxccStatus Data::dxccStatus(int dxcc, QString band, QString mode) {
    QSqlQuery query;
    query.prepare("SELECT (SELECT contacts.callsign FROM contacts WHERE dxcc = :dxcc ORDER BY start_time ASC LIMIT 1) as entity,"
                  "(SELECT contacts.callsign FROM contacts WHERE dxcc = :dxcc AND band = :band ORDER BY start_time ASC LIMIT 1) as band,"
                  "(SELECT contacts.callsign FROM contacts INNER JOIN modes ON (modes.name = contacts.mode)"
                  "        WHERE contacts.dxcc = :dxcc AND modes.dxcc = (SELECT modes.dxcc FROM modes WHERE modes.name = :mode LIMIT 1)"
                  "        ORDER BY start_time ASC LIMIT 1) as mode,"
                  "(SELECT contacts.callsign FROM contacts INNER JOIN modes ON (modes.name = contacts.mode)"
                  "        WHERE contacts.dxcc = :dxcc AND modes.dxcc = (SELECT modes.dxcc FROM modes WHERE modes.name = :mode LIMIT 1)"
                  "        AND band = :band ORDER BY start_time ASC LIMIT 1) as slot;");

    query.bindValue(":dxcc", dxcc);
    query.bindValue(":band", band);
    query.bindValue(":mode", mode);
    query.exec();

    if (query.next()) {
        if (query.value(0).isNull()) {
            return DxccStatus::NewEntity;
        }
        if (query.value(1).isNull()) {
            if (query.value(2).isNull()) {
                return DxccStatus::NewBandMode;
            }
            else {
                return DxccStatus::NewBand;
            }
        }
        if (query.value(2).isNull()) {
            return DxccStatus::NewMode;
        }
        if (query.value(3).isNull()) {
            return DxccStatus::NewSlot;
        }
        else {
            return DxccStatus::Worked;
        }
    }
    else {
        return DxccStatus::Unknown;
    }
}

QString Data::band(double freq) {
    QSqlQuery query;
    query.prepare("SELECT name FROM bands WHERE :freq BETWEEN start_freq AND end_freq");
    query.bindValue(0, freq);
    query.exec();

    if (query.next()) {
        return query.value(0).toString();
    }
    else {
        return QString();
    }
}

void Data::loadContests() {
    QFile file(":/res/data/contests.json");
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QByteArray data = file.readAll();

    for (QVariant object : QJsonDocument::fromJson(data).toVariant().toList()) {
        QVariantMap contestData = object.toMap();

        QString id = contestData.value("id").toString();
        QString name = contestData.value("name").toString();

        contests.insert(id, name);
    }
}

void Data::loadPropagationModes() {
    QFile file(":/res/data/propagation_modes.json");
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QByteArray data = file.readAll();

    for (QVariant object : QJsonDocument::fromJson(data).toVariant().toList()) {
        QVariantMap propagationModeData = object.toMap();

        QString id = propagationModeData.value("id").toString();
        QString name = propagationModeData.value("name").toString();

        propagationModes.insert(id, name);
    }
}

DxccEntity Data::lookupDxcc(QString callsign) {
    QSqlQuery query;
    query.prepare(
                "SELECT\n"
                "    dxcc_entities.id,\n"
                "    dxcc_entities.name,\n"
                "    dxcc_entities.prefix,\n"
                "    dxcc_entities.cont,\n"
                "    CASE\n"
                "        WHEN (dxcc_prefixes.cqz != 0)\n"
                "        THEN dxcc_prefixes.cqz\n"
                "        ELSE dxcc_entities.cqz\n"
                "    END AS cqz,\n"
                "    CASE\n"
                "        WHEN (dxcc_prefixes.ituz != 0)\n"
                "        THEN dxcc_prefixes.ituz\n"
                "        ELSE dxcc_entities.ituz\n"
                "    END AS ituz\n,"
                "    dxcc_entities.lat,\n"
                "    dxcc_entities.lon,\n"
                "    dxcc_entities.tz\n"
                "FROM dxcc_prefixes\n"
                "INNER JOIN dxcc_entities ON (dxcc_prefixes.dxcc = dxcc_entities.id)\n"
                "WHERE (dxcc_prefixes.prefix = :callsign and dxcc_prefixes.exact = true)\n"
                "    OR (dxcc_prefixes.exact = false and :callsign LIKE dxcc_prefixes.prefix || '%')\n"
                "ORDER BY dxcc_prefixes.prefix\n"
                "DESC LIMIT 1\n"
    );

    query.bindValue(":callsign", callsign);
    query.exec();

    DxccEntity dxcc;
    if (query.next()) {
        dxcc.dxcc = query.value(0).toInt();
        dxcc.country = query.value(1).toString();
        dxcc.prefix = query.value(2).toString();
        dxcc.cont = query.value(3).toString();
        dxcc.cqz = query.value(4).toInt();
        dxcc.ituz = query.value(5).toInt();
        dxcc.latlon[0] = query.value(6).toFloat();
        dxcc.latlon[1] = query.value(7).toFloat();
        dxcc.tz = query.value(8).toFloat();
    }
    return dxcc;
}