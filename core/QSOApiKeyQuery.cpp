#include <QSqlRecord>
#include <QTextStream>
#include <QVariantMap>

#include "QSOApiKeyQuery.h"
#include "logformat/AdiFormat.h"

namespace QSOApiKeyQuery {

QList<QPair<QString, QString>> buildParams(const QSqlRecord &record)
{
    QList<QPair<QString, QString>> params;

    QString adifText;
    QTextStream writeStream(&adifText, QIODevice::ReadWrite);
    AdiFormat writer(writeStream);
    writer.exportContact(record);
    writeStream.flush();

    QTextStream readStream(&adifText, QIODevice::ReadOnly);
    AdiFormat reader(readStream);

    QVariantMap fields;
    if (!reader.readContact(fields))
        return params;

    for (auto it = fields.constBegin(); it != fields.constEnd(); ++it)
    {
        const QString value = it.value().toString();
        if (!value.isEmpty())
            params.append({it.key(), value});
    }

    return params;
}

} // namespace QSOApiKeyQuery
