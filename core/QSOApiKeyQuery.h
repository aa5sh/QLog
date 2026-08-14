#ifndef QLOG_CORE_QSOAPIKEYQUERY_H
#define QLOG_CORE_QSOAPIKEYQUERY_H

#include <QList>
#include <QPair>
#include <QString>

class QSqlRecord;

/*!
 * Pure, network-free building block for QSOApiKeySender.
 *
 * Flattens a logged QSO into GET query parameters by reusing QLog's own
 * ADIF export (QSqlRecord -> ADIF text -> flat field map) rather than
 * inventing a second field-name mapping - the same round trip
 * CustomCallbook already relies on for the read direction
 * (AdiFormat::readContact()).
 *
 * Empty fields are omitted. The api key is deliberately never added here -
 * see QSOApiKeySender for why it travels separately.
 */
namespace QSOApiKeyQuery {

QList<QPair<QString, QString>> buildParams(const QSqlRecord &record);

} // namespace QSOApiKeyQuery

#endif // QLOG_CORE_QSOAPIKEYQUERY_H
