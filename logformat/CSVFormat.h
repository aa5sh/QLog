#ifndef QLOG_LOGFORMAT_CSVFORMAT_H
#define QLOG_LOGFORMAT_CSVFORMAT_H

#include "LogFormat.h"
#include "logformat/AdxFormat.h"

// derived from ADX because ADX format contains non-intl and intl fields
class CSVFormat : public AdxFormat
{
public:
    explicit CSVFormat(QTextStream& stream);

    virtual void exportStart() override;
    virtual void exportContact(const QSqlRecord& record, QMap<QString, QString> *) override;
    virtual void exportEnd() override;

    virtual void importStart() override {};
    virtual void importEnd() override {};
    void setDelimiter(const QChar&);

protected:
    virtual void writeField(const QString &name,
                            bool presenceCondition,
                            const QString &value,
                            const QString &type="") override;
    const QString toDate(const QVariant &) override;
    const QString toTime(const QVariant &) override;

private:
    QMap<QString, int> header;
    QList<QHash<QString, QString>> exportedRecords;
    QHash<QString, QString> currectRecord;

    QString csvStringValue(const QString&);
    QChar delimiter;
};

#endif // QLOG_LOGFORMAT_CSVFORMAT_H
