#include <QJsonDocument>
#include <QJsonArray>
#include <QPainter>
#include <QFont>
#include <QColor>
#include <QSqlQuery>
#include <QSqlError>
#include <QRegularExpression>
#include <QBuffer>
#include <QImageWriter>
#include <QHostInfo>
#include <QDateTime>
#include <QUuid>
#include <QCoreApplication>

#include "EmailQSLService.h"
#include "core/debug.h"
#include "core/LogParam.h"

MODULE_IDENTIFICATION("qlog.service.emailqsl");

// ---------------------------------------------------------------------------
// EmailQSLFieldOverlay
// ---------------------------------------------------------------------------

QJsonObject EmailQSLFieldOverlay::toJson() const
{
    FCT_IDENTIFICATION;

    QJsonObject o;
    o["type"]         = type;
    o["fieldName"]    = fieldName;
    o["x"]            = x;
    o["y"]            = y;
    o["fontFamily"]   = fontFamily;
    o["fontSize"]     = fontSize;
    o["color"]        = color;
    o["bold"]         = bold;
    o["italic"]       = italic;
    o["width"]        = width;
    o["height"]       = height;
    o["fillColor"]    = fillColor;
    o["opacity"]      = opacity;
    o["cornerRadius"] = cornerRadius;
    return o;
}

EmailQSLFieldOverlay EmailQSLFieldOverlay::fromJson(const QJsonObject &obj)
{
    FCT_IDENTIFICATION;

    EmailQSLFieldOverlay f;
    f.type         = obj.value("type").toString(QStringLiteral("TEXT"));
    f.fieldName    = obj.value("fieldName").toString();
    f.x            = obj.value("x").toInt();
    f.y            = obj.value("y").toInt();
    f.fontFamily   = obj.value("fontFamily").toString(QStringLiteral("Arial"));
    f.fontSize     = obj.value("fontSize").toInt(14);
    f.color        = obj.value("color").toString(QStringLiteral("#000000"));
    f.bold         = obj.value("bold").toBool();
    f.italic       = obj.value("italic").toBool();
    f.width        = obj.value("width").toInt(120);
    f.height       = obj.value("height").toInt(60);
    f.fillColor    = obj.value("fillColor").toString(QStringLiteral("#FFFF99"));
    f.opacity      = obj.value("opacity").toInt(80);
    f.cornerRadius = obj.value("cornerRadius").toInt(8);
    return f;
}

// ---------------------------------------------------------------------------
// EmailQSLBase — credential registration
// ---------------------------------------------------------------------------

const QString EmailQSLBase::SECURE_STORAGE_KEY = QStringLiteral("EmailQSL");
REGISTRATION_SECURE_SERVICE(EmailQSLBase);

void EmailQSLBase::registerCredentials()
{
    FCT_IDENTIFICATION;

    CredentialRegistry::instance().add(SECURE_STORAGE_KEY, []()
    {
        return QList<CredentialDescriptor>
        {
            { SECURE_STORAGE_KEY, []() { return getSmtpUsername(); } }
        };
    });
}

// ---------------------------------------------------------------------------
// EmailQSLBase — settings persisted via LogParam (log_param DB table)
// ---------------------------------------------------------------------------

QString EmailQSLBase::getSmtpHost()
{
    FCT_IDENTIFICATION;

    return LogParam::getEmailQSLSmtpHost();
}

void EmailQSLBase::setSmtpHost(const QString &host)
{
    FCT_IDENTIFICATION;

    LogParam::setEmailQSLSmtpHost(host);
}

int EmailQSLBase::getSmtpPort()
{
    FCT_IDENTIFICATION;

    return LogParam::getEmailQSLSmtpPort();
}

void EmailQSLBase::setSmtpPort(int port)
{
    FCT_IDENTIFICATION;

    LogParam::setEmailQSLSmtpPort(port);
}

bool EmailQSLBase::getSmtpAllowInvalidCert()
{
    FCT_IDENTIFICATION;

    return LogParam::getEmailQSLSmtpAllowInvalidCert();
}

void EmailQSLBase::setSmtpAllowInvalidCert(bool allow)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << allow;

    LogParam::setEmailQSLSmtpAllowInvalidCert(allow);
}

int EmailQSLBase::getSmtpEncryption()
{
    FCT_IDENTIFICATION;

    return LogParam::getEmailQSLSmtpEncryption();
}

void EmailQSLBase::setSmtpEncryption(int enc)
{
    FCT_IDENTIFICATION;

    LogParam::setEmailQSLSmtpEncryption(enc);
}

QString EmailQSLBase::getSmtpUsername()
{
    FCT_IDENTIFICATION;

    return LogParam::getEmailQSLSmtpUsername();
}

void EmailQSLBase::setSmtpUsername(const QString &username)
{
    FCT_IDENTIFICATION;

    LogParam::setEmailQSLSmtpUsername(username);
}

QString EmailQSLBase::getSmtpPassword()
{
    FCT_IDENTIFICATION;

    return getPassword(SECURE_STORAGE_KEY, getSmtpUsername());
}

void EmailQSLBase::saveSmtpCredentials(const QString &username, const QString &password)
{
    FCT_IDENTIFICATION;

    const QString oldUsername = getSmtpUsername();
    if ( oldUsername != username && !oldUsername.isEmpty() )
        deletePassword(SECURE_STORAGE_KEY, oldUsername);
    setSmtpUsername(username);
    savePassword(SECURE_STORAGE_KEY, username, password);
}

QString EmailQSLBase::getFromAddress()
{
    FCT_IDENTIFICATION;

    return LogParam::getEmailQSLFromAddress();
}

void EmailQSLBase::setFromAddress(const QString &addr)
{
    FCT_IDENTIFICATION;

    LogParam::setEmailQSLFromAddress(addr);
}

QString EmailQSLBase::getFromName()
{
    FCT_IDENTIFICATION;

    return LogParam::getEmailQSLFromName();
}

void EmailQSLBase::setFromName(const QString &name)
{
    FCT_IDENTIFICATION;

    LogParam::setEmailQSLFromName(name);
}

QString EmailQSLBase::getSubjectTemplate()
{
    FCT_IDENTIFICATION;

    return LogParam::getEmailQSLSubjectTemplate(
        QStringLiteral("QSL Card from {MY_CALLSIGN} for our QSO on {QSO_DATE}"));
}

void EmailQSLBase::setSubjectTemplate(const QString &tmpl)
{
    FCT_IDENTIFICATION;

    LogParam::setEmailQSLSubjectTemplate(tmpl);
}

QString EmailQSLBase::getBodyTemplate()
{
    FCT_IDENTIFICATION;

    return LogParam::getEmailQSLBodyTemplate(
        QStringLiteral("Dear {NAME},\n\nPlease find my QSL card attached confirming our contact.\n\n"
                       "Callsign: {MY_CALLSIGN}\n"
                       "Date: {QSO_DATE}\nTime: {TIME_ON} UTC\n"
                       "Band: {BAND}\nMode: {MODE}\n"
                       "RST Sent: {RST_SENT}\nRST Rcvd: {RST_RCVD}\n\n"
                       "73,\n{MY_CALLSIGN}"));
}

void EmailQSLBase::setBodyTemplate(const QString &tmpl)
{
    FCT_IDENTIFICATION;

    LogParam::setEmailQSLBodyTemplate(tmpl);
}

QString EmailQSLBase::getCardImagePath()
{
    FCT_IDENTIFICATION;

    return LogParam::getEmailQSLCardImagePath();
}

void EmailQSLBase::setCardImagePath(const QString &path)
{
    FCT_IDENTIFICATION;

    LogParam::setEmailQSLCardImagePath(path);
}

QList<EmailQSLFieldOverlay> EmailQSLBase::getCardFieldOverlays()
{
    FCT_IDENTIFICATION;

    QList<EmailQSLFieldOverlay> result;
    const QJsonArray arr = QJsonDocument::fromJson(LogParam::getEmailQSLCardOverlays()).array();
    for ( const QJsonValue &v : arr )
        result.append(EmailQSLFieldOverlay::fromJson(v.toObject()));
    return result;
}

void EmailQSLBase::setCardFieldOverlays(const QList<EmailQSLFieldOverlay> &overlays)
{
    FCT_IDENTIFICATION;

    QJsonArray arr;
    for ( const EmailQSLFieldOverlay &o : overlays )
        arr.append(o.toJson());
    LogParam::setEmailQSLCardOverlays(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

// ---------------------------------------------------------------------------
// EmailQSLBase — sent-tracking via contacts.fields JSON
// ---------------------------------------------------------------------------

static const QString EMAIL_QSL_SENT_KEY = QStringLiteral("email_qsl_sent_dt");

QDateTime EmailQSLBase::getEmailSentDateTime(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    const QByteArray raw = record.value(QStringLiteral("fields")).toByteArray();
    const QJsonObject fields = QJsonDocument::fromJson(raw).object();
    const QString dtStr = fields.value(EMAIL_QSL_SENT_KEY).toString();
    return dtStr.isEmpty() ? QDateTime() : QDateTime::fromString(dtStr, Qt::ISODate);
}

bool EmailQSLBase::hasEmailBeenSentToCallsign(const QString &callsign, int excludeId)
{
    FCT_IDENTIFICATION;

    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT fields FROM contacts WHERE callsign = :cs AND id != :ex"));
    q.bindValue(":cs", callsign.toUpper());
    q.bindValue(":ex", excludeId);
    if ( !q.exec() )
        return false;

    while ( q.next() )
    {
        const QByteArray raw = q.value(0).toByteArray();
        const QJsonObject fields = QJsonDocument::fromJson(raw).object();
        if ( fields.contains(EMAIL_QSL_SENT_KEY) )
            return true;
    }
    return false;
}

bool EmailQSLBase::recordEmailSent(int contactId)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << contactId;

    // Read the current value instead of the record captured when the send
    // started, so edits made meanwhile are not overwritten.
    QSqlQuery select;
    if ( !select.prepare(QStringLiteral("SELECT fields FROM contacts WHERE id = :id")) )
    {
        qCWarning(runtime) << "Cannot prepare Email QSL sent lookup" << select.lastError();
        return false;
    }
    select.bindValue(QStringLiteral(":id"), contactId);
    if ( !select.exec() || !select.next() )
    {
        qCWarning(runtime) << "Cannot read contact" << contactId << "to record Email QSL" << select.lastError();
        return false;
    }

    QJsonObject fields = QJsonDocument::fromJson(select.value(0).toByteArray()).object();
    fields[EMAIL_QSL_SENT_KEY] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    QSqlQuery update;
    if ( !update.prepare(QStringLiteral("UPDATE contacts SET fields = :f WHERE id = :id")) )
    {
        qCWarning(runtime) << "Cannot prepare Email QSL sent update" << update.lastError();
        return false;
    }
    update.bindValue(QStringLiteral(":f"),  QJsonDocument(fields).toJson(QJsonDocument::Compact));
    update.bindValue(QStringLiteral(":id"), contactId);
    if ( !update.exec() )
    {
        qCWarning(runtime) << "Cannot record Email QSL for contact" << contactId << update.lastError();
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// EmailQSLBase — merge fields
// ---------------------------------------------------------------------------

QString EmailQSLBase::fieldValue(const QString &key, const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    if ( key == QLatin1String("CALLSIGN") )
        return record.value(QStringLiteral("callsign")).toString().toUpper();

    if ( key == QLatin1String("QSO_DATE") )
    {
        const QDateTime dt = record.value(QStringLiteral("start_time")).toDateTime();
        return dt.isValid() ? dt.toUTC().toString(QStringLiteral("dd-MMM-yyyy")).toUpper() : QString();
    }
    if ( key == QLatin1String("QSO_DATE_ISO") )
    {
        const QDateTime dt = record.value(QStringLiteral("start_time")).toDateTime();
        return dt.isValid() ? dt.toUTC().toString(QStringLiteral("yyyyMMdd")) : QString();
    }
    if ( key == QLatin1String("TIME_ON") )
    {
        const QDateTime dt = record.value(QStringLiteral("start_time")).toDateTime();
        return dt.isValid() ? dt.toUTC().toString(QStringLiteral("HHmm")) : QString();
    }
    if ( key == QLatin1String("FREQ") )
    {
        bool ok;
        double mhz = record.value(QStringLiteral("freq")).toDouble(&ok);
        return ok ? QString::number(mhz, 'f', 3) : QString();
    }
    if ( key == QLatin1String("BAND") )
        return record.value(QStringLiteral("band")).toString().toUpper();
    if ( key == QLatin1String("MODE") )
        return record.value(QStringLiteral("mode")).toString().toUpper();
    if ( key == QLatin1String("SUBMODE") )
        return record.value(QStringLiteral("submode")).toString().toUpper();
    if ( key == QLatin1String("RST_SENT") )
        return record.value(QStringLiteral("rst_sent")).toString();
    if ( key == QLatin1String("RST_RCVD") )
        return record.value(QStringLiteral("rst_rcvd")).toString();
    if ( key == QLatin1String("NAME") )
    {
        const QString n = record.value(QStringLiteral("name_intl")).toString();
        return n.isEmpty() ? record.value(QStringLiteral("name")).toString() : n;
    }
    if ( key == QLatin1String("QTH") )
    {
        const QString q = record.value(QStringLiteral("qth_intl")).toString();
        return q.isEmpty() ? record.value(QStringLiteral("qth")).toString() : q;
    }
    if ( key == QLatin1String("COUNTRY") )
    {
        const QString c = record.value(QStringLiteral("country_intl")).toString();
        return c.isEmpty() ? record.value(QStringLiteral("country")).toString() : c;
    }
    if ( key == QLatin1String("GRIDSQUARE") )
        return record.value(QStringLiteral("gridsquare")).toString().toUpper();
    if ( key == QLatin1String("DXCC") )
        return record.value(QStringLiteral("dxcc")).toString();
    if ( key == QLatin1String("CQZ") )
        return record.value(QStringLiteral("cqz")).toString();
    if ( key == QLatin1String("ITUZ") )
        return record.value(QStringLiteral("ituz")).toString();
    if ( key == QLatin1String("TX_PWR") )
        return record.value(QStringLiteral("tx_pwr")).toString();
    if ( key == QLatin1String("EMAIL") )
        return record.value(QStringLiteral("email")).toString();
    if ( key == QLatin1String("MY_CALLSIGN") )
        return record.value(QStringLiteral("station_callsign")).toString().toUpper();
    if ( key == QLatin1String("MY_GRIDSQUARE") )
        return record.value(QStringLiteral("my_gridsquare")).toString().toUpper();
    if ( key == QLatin1String("OPERATOR") )
        return record.value(QStringLiteral("operator")).toString().toUpper();
    if ( key == QLatin1String("COMMENT") )
    {
        const QString c = record.value(QStringLiteral("comment_intl")).toString();
        return c.isEmpty() ? record.value(QStringLiteral("comment")).toString() : c;
    }
    if ( key == QLatin1String("SOTA_REF") )
        return record.value(QStringLiteral("sota_ref")).toString();
    if ( key == QLatin1String("POTA_REF") )
        return record.value(QStringLiteral("pota_ref")).toString();
    if ( key == QLatin1String("WWFF_REF") )
        return record.value(QStringLiteral("wwff_ref")).toString();
    if ( key == QLatin1String("IOTA") )
        return record.value(QStringLiteral("iota")).toString();
    if ( key == QLatin1String("SIG") )
        return record.value(QStringLiteral("sig_intl")).toString();
    if ( key == QLatin1String("CONTEST_ID") )
        return record.value(QStringLiteral("contest_id")).toString();

    // Fallback: try direct lowercase column name
    const QString col = key.toLower();
    if ( record.indexOf(col) >= 0 )
        return record.value(col).toString();

    return QString();
}

QString EmailQSLBase::applyMergeFields(const QString &tmpl, const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    QString result = tmpl;
    static const QRegularExpression rx(QStringLiteral("\\{([A-Z0-9_]+)\\}"));
    QRegularExpressionMatchIterator it = rx.globalMatch(tmpl);
    while ( it.hasNext() )
    {
        const QRegularExpressionMatch m = it.next();
        const QString key   = m.captured(1);
        const QString value = fieldValue(key, record);
        result.replace(QLatin1Char('{') + key + QLatin1Char('}'), value);
    }
    return result;
}

QList<EmailQSLBase::MergeField> EmailQSLBase::availableMergeFields()
{
    FCT_IDENTIFICATION;

    return {
        { "CALLSIGN",      QObject::tr("Contact callsign") },
        { "QSO_DATE",      QObject::tr("QSO date (dd-MMM-yyyy)") },
        { "QSO_DATE_ISO",  QObject::tr("QSO date (YYYYMMDD)") },
        { "TIME_ON",       QObject::tr("QSO start time UTC (HHmm)") },
        { "FREQ",          QObject::tr("Frequency (MHz)") },
        { "BAND",          QObject::tr("Band") },
        { "MODE",          QObject::tr("Mode") },
        { "SUBMODE",       QObject::tr("Sub-mode") },
        { "RST_SENT",      QObject::tr("RST sent") },
        { "RST_RCVD",      QObject::tr("RST received") },
        { "NAME",          QObject::tr("Contact name") },
        { "QTH",           QObject::tr("Contact QTH") },
        { "COUNTRY",       QObject::tr("Country") },
        { "GRIDSQUARE",    QObject::tr("Grid square") },
        { "DXCC",          QObject::tr("DXCC entity number") },
        { "CQZ",           QObject::tr("CQ zone") },
        { "ITUZ",          QObject::tr("ITU zone") },
        { "TX_PWR",        QObject::tr("TX power") },
        { "EMAIL",         QObject::tr("Contact email address") },
        { "MY_CALLSIGN",   QObject::tr("My callsign") },
        { "MY_GRIDSQUARE", QObject::tr("My grid square") },
        { "OPERATOR",      QObject::tr("Operator callsign") },
        { "COMMENT",       QObject::tr("Comment") },
        { "SOTA_REF",      QObject::tr("SOTA reference") },
        { "POTA_REF",      QObject::tr("POTA reference") },
        { "WWFF_REF",      QObject::tr("WWFF reference") },
        { "IOTA",          QObject::tr("IOTA reference") },
        { "SIG",           QObject::tr("Special interest group") },
        { "CONTEST_ID",    QObject::tr("Contest ID") },
    };
}

// ---------------------------------------------------------------------------
// EmailQSLBase — card rendering
// ---------------------------------------------------------------------------

QPixmap EmailQSLBase::renderCard(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;
    return renderCard(getCardImagePath(), record, getCardFieldOverlays());
}

QPixmap EmailQSLBase::renderCard(const QString &imagePath,
                                 const QSqlRecord &record,
                                 const QList<EmailQSLFieldOverlay> &overlays)
{
    FCT_IDENTIFICATION;

    QPixmap pixmap(imagePath);
    if ( pixmap.isNull() )
    {
        qCWarning(runtime) << "renderCard: could not load image:" << imagePath;
        return QPixmap();
    }

    // Overlay x, y, fontSize and BOX width/height are all stored as absolute
    // pixel values scaled to the source image dimensions.  No extra scaling is
    // applied here — the values are used as-is.  addDefaultOverlays() and
    // onCardImageChanged() are responsible for keeping them proportional whenever
    // the background image changes.
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    for ( const EmailQSLFieldOverlay &ov : overlays )
    {
        if ( ov.type == QLatin1String("BOX") )
        {
            QColor fill(ov.fillColor);
            fill.setAlphaF(ov.opacity / 100.0);
            painter.setPen(QPen(QColor(ov.color), 1.5));
            painter.setBrush(fill);
            painter.drawRoundedRect(ov.x, ov.y, ov.width, ov.height,
                                    ov.cornerRadius, ov.cornerRadius);

            if ( !ov.fieldName.isEmpty() )
            {
                QFont font(ov.fontFamily, ov.fontSize > 0 ? ov.fontSize : 11);
                font.setBold(ov.bold);
                painter.setFont(font);
                painter.setPen(QColor(ov.color));
                painter.setBrush(Qt::NoBrush);
                QFontMetrics fm(font);
                painter.drawText(ov.x, ov.y - fm.descent() - 2, ov.fieldName);
            }
        }
        else if ( ov.type == QLatin1String("LABEL") )
        {
            if ( ov.fieldName.isEmpty() )
                continue;

            QFont font(ov.fontFamily, ov.fontSize);
            font.setBold(ov.bold);
            font.setItalic(ov.italic);
            painter.setFont(font);
            painter.setPen(QColor(ov.color));
            painter.setBrush(Qt::NoBrush);
            painter.drawText(ov.x, ov.y, ov.fieldName);
        }
        else // TEXT — merge-field substitution
        {
            const QString value = fieldValue(ov.fieldName, record);
            if ( value.isEmpty() )
                continue;

            QFont font(ov.fontFamily, ov.fontSize);
            font.setBold(ov.bold);
            font.setItalic(ov.italic);
            painter.setFont(font);
            painter.setPen(QColor(ov.color));
            painter.setBrush(Qt::NoBrush);
            painter.drawText(ov.x, ov.y, value);
        }
    }
    painter.end();
    return pixmap;
}

// ---------------------------------------------------------------------------
// SmtpWorker
// ---------------------------------------------------------------------------

SmtpWorker::SmtpWorker(const QString &host, int port, int encryption, bool allowInvalidCert,
                       const QString &username, const QString &password,
                       const QString &fromAddress, const QString &fromName,
                       const QString &toAddress,
                       const QString &subject, const QString &body,
                       const QByteArray &imageData, const QString &imageName,
                       QObject *parent)
    : QObject(parent),
      smtpHost(host), smtpPort(port), smtpEncryption(encryption), smtpAllowInvalidCert(allowInvalidCert),
      testOnlyMode(false),
      smtpUsername(username), smtpPassword(password),
      mailFromAddress(fromAddress), mailFromName(fromName),
      mailToAddress(toAddress),
      mailSubject(subject), mailBody(body),
      attachmentData(imageData), attachmentName(imageName)
{
    FCT_IDENTIFICATION;
}

void SmtpWorker::setTestOnly(bool testOnly)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << testOnly;

    testOnlyMode = testOnly;
}

bool SmtpWorker::hasFinalReplyLine(const QByteArray &data)
{
    FCT_IDENTIFICATION;

    // A reply may arrive in several reads, and multi-line replies use
    // "NNN-" on every line but the last, which is "NNN " (or bare "NNN").
    // Only complete lines count.
    int lineStart = 0;
    while ( true )
    {
        const int lineEnd = data.indexOf('\n', lineStart);
        if ( lineEnd < 0 )
            return false;

        const QByteArray line = data.mid(lineStart, lineEnd - lineStart).trimmed();
        if ( line.size() >= 3
             && QChar(line.at(0)).isDigit() && QChar(line.at(1)).isDigit() && QChar(line.at(2)).isDigit()
             && ( line.size() == 3 || line.at(3) == ' ' ) )
            return true;

        lineStart = lineEnd + 1;
    }
}

bool SmtpWorker::waitForResponse(QSslSocket *socket, QByteArray &out, int timeoutMs)
{
    FCT_IDENTIFICATION;

    out = socket->readAll();
    while ( !hasFinalReplyLine(out) )
    {
        if ( !socket->waitForReadyRead(timeoutMs) )
            return false;
        out += socket->readAll();
    }
    return true;
}

int SmtpWorker::responseCode(const QByteArray &response)
{
    FCT_IDENTIFICATION;

    // Find the last "NNN " line
    const QList<QByteArray> lines = response.split('\n');
    for ( int i = lines.size() - 1; i >= 0; --i )
    {
        const QByteArray &line = lines.at(i).trimmed();
        if ( line.length() >= 3 )
        {
            bool ok;
            int code = line.left(3).toInt(&ok);
            if ( ok )
                return code;
        }
    }
    return -1;
}

bool SmtpWorker::sendCommand(QSslSocket *socket, const QByteArray &cmd,
                             int expectedCode, QByteArray &response)
{
    FCT_IDENTIFICATION;

    socket->write(cmd);
    if ( !socket->waitForBytesWritten(10000) )
        return false;
    if ( !waitForResponse(socket, response) )
        return false;
    return responseCode(response) == expectedCode;
}

QByteArray SmtpWorker::encodeHeaderText(const QString &text)
{
    FCT_IDENTIFICATION;

    // "=?UTF-8?B?" + base64 + "?=" must stay within 75 characters, so encode
    // at most 45 bytes per word, never splitting a UTF-8 sequence, and fold
    // the words onto continuation lines.
    static const int MAX_WORD_BYTES = 45;
    const QByteArray utf8 = text.toUtf8();
    QList<QByteArray> words;
    int start = 0;
    while ( start < utf8.size() )
    {
        int end = qMin(start + MAX_WORD_BYTES, utf8.size());
        while ( end < utf8.size() && end > start && ( static_cast<quint8>(utf8.at(end)) & 0xC0 ) == 0x80 )
            --end;
        words << ( "=?UTF-8?B?" + utf8.mid(start, end - start).toBase64() + "?=" );
        start = end;
    }
    return words.join("\r\n ");
}

bool SmtpWorker::isSafeAddress(const QString &address)
{
    FCT_IDENTIFICATION;

    // The address comes from contact data (callbook lookups, ADIF imports).
    // CR/LF or angle brackets would let it inject SMTP commands or headers.
    static const QRegularExpression re(QStringLiteral("^[^\\s<>@]+@[^\\s<>@]+$"));
    return re.match(address).hasMatch();
}

QByteArray SmtpWorker::buildMimeMessage()
{
    FCT_IDENTIFICATION;

    const QString boundary = QStringLiteral("----QLogEmailQSL_%1")
        .arg(QDateTime::currentMSecsSinceEpoch());

    QByteArray msg;

    // RFC 5322 requires Date; Message-ID is expected by most spam filters.
    const QString domain = mailFromAddress.mid(mailFromAddress.indexOf('@') + 1);
    msg += "MIME-Version: 1.0\r\n";
    msg += "Date: " + QDateTime::currentDateTime().toString(Qt::RFC2822Date).toLatin1() + "\r\n";
    msg += "Message-ID: <" + QUuid::createUuid().toString(QUuid::WithoutBraces).toLatin1()
           + "@" + domain.toUtf8() + ">\r\n";
    msg += "From: " + ( mailFromName.isEmpty() ? QByteArray() : encodeHeaderText(mailFromName) + " " )
           + "<" + mailFromAddress.toUtf8() + ">\r\n";
    msg += "To: <" + mailToAddress.toUtf8() + ">\r\n";
    msg += "Subject: " + encodeHeaderText(mailSubject) + "\r\n";
    msg += "Content-Type: multipart/mixed; boundary=\"" + boundary.toUtf8() + "\"\r\n";
    msg += "\r\n";

    // --- Text part ---
    msg += "--" + boundary.toUtf8() + "\r\n";
    msg += "Content-Type: text/plain; charset=UTF-8\r\n";
    msg += "Content-Transfer-Encoding: base64\r\n";
    msg += "\r\n";
    const QByteArray bodyB64 = mailBody.toUtf8().toBase64();
    for ( int i = 0; i < bodyB64.size(); i += 76 )
        msg += bodyB64.mid(i, 76) + "\r\n";

    // --- Image attachment (if present) ---
    if ( !attachmentData.isEmpty() )
    {
        msg += "\r\n--" + boundary.toUtf8() + "\r\n";
        const QString mimeType = attachmentName.endsWith(QLatin1String(".png"), Qt::CaseInsensitive)
                                 ? QStringLiteral("image/png")
                                 : QStringLiteral("image/jpeg");
        msg += "Content-Type: " + mimeType.toUtf8()
               + "; name=\"" + attachmentName.toUtf8() + "\"\r\n";
        msg += "Content-Transfer-Encoding: base64\r\n";
        msg += "Content-Disposition: attachment; filename=\""
               + attachmentName.toUtf8() + "\"\r\n";
        msg += "\r\n";
        const QByteArray imgB64 = attachmentData.toBase64();
        for ( int i = 0; i < imgB64.size(); i += 76 )
            msg += imgB64.mid(i, 76) + "\r\n";
    }

    msg += "\r\n--" + boundary.toUtf8() + "--\r\n";
    return msg;
}

void SmtpWorker::run()
{
    FCT_IDENTIFICATION;

    if ( !testOnlyMode && !isSafeAddress(mailFromAddress) )
    {
        emit finished(false, tr("The sender address is not valid: %1").arg(mailFromAddress));
        return;
    }
    if ( !testOnlyMode && !isSafeAddress(mailToAddress) )
    {
        emit finished(false, tr("The recipient address is not valid: %1").arg(mailToAddress));
        return;
    }

    QSslSocket socket;
    socket.setProtocol(QSsl::AnyProtocol);
    // Verify the certificate and host name unless the user explicitly
    // allowed an invalid one; otherwise the SMTP password could be sent to
    // anyone able to intercept the connection.
    socket.setPeerVerifyMode(smtpAllowInvalidCert ? QSslSocket::VerifyNone : QSslSocket::VerifyPeer);

    // ---- Connect ----
    if ( smtpEncryption == EmailQSLBase::ENCRYPTION_SSL_TLS )
    {
        socket.connectToHostEncrypted(smtpHost, static_cast<quint16>(smtpPort));
        if ( !socket.waitForEncrypted(15000) )
        {
            emit finished(false, tr("SSL/TLS connection failed: %1").arg(socket.errorString()));
            return;
        }
    }
    else
    {
        socket.connectToHost(smtpHost, static_cast<quint16>(smtpPort));
        if ( !socket.waitForConnected(15000) )
        {
            emit finished(false, tr("Could not connect to %1:%2 — %3")
                          .arg(smtpHost).arg(smtpPort).arg(socket.errorString()));
            return;
        }
    }

    // ---- Read greeting (220) ----
    QByteArray resp;
    if ( !waitForResponse(&socket, resp) || responseCode(resp) != 220 )
    {
        emit finished(false, tr("Server did not send a valid greeting (expected 220)."));
        return;
    }

    // ---- EHLO ----
    const QByteArray localHost = QHostInfo::localHostName().toUtf8();
    if ( !sendCommand(&socket, "EHLO " + localHost + "\r\n", 250, resp) )
    {
        // Try HELO as fallback
        if ( !sendCommand(&socket, "HELO " + localHost + "\r\n", 250, resp) )
        {
            emit finished(false, tr("EHLO/HELO rejected by server."));
            return;
        }
    }

    // ---- STARTTLS upgrade ----
    if ( smtpEncryption == EmailQSLBase::ENCRYPTION_STARTTLS )
    {
        if ( !sendCommand(&socket, "STARTTLS\r\n", 220, resp) )
        {
            emit finished(false, tr("STARTTLS not accepted by server."));
            return;
        }
        socket.startClientEncryption();
        if ( !socket.waitForEncrypted(15000) )
        {
            emit finished(false, tr("TLS handshake failed: %1").arg(socket.errorString()));
            return;
        }
        // Re-EHLO after TLS negotiation
        if ( !sendCommand(&socket, "EHLO " + localHost + "\r\n", 250, resp) )
        {
            emit finished(false, tr("EHLO after STARTTLS rejected."));
            return;
        }
    }

    // ---- AUTH LOGIN ----
    if ( !smtpUsername.isEmpty() )
    {
        if ( !sendCommand(&socket, "AUTH LOGIN\r\n", 334, resp) )
        {
            emit finished(false, tr("AUTH LOGIN not supported by server."));
            return;
        }
        if ( !sendCommand(&socket, smtpUsername.toUtf8().toBase64() + "\r\n", 334, resp) )
        {
            emit finished(false, tr("Username rejected by server."));
            return;
        }
        if ( !sendCommand(&socket, smtpPassword.toUtf8().toBase64() + "\r\n", 235, resp) )
        {
            emit finished(false, tr("Authentication failed — check your username and password."));
            return;
        }
    }

    if ( testOnlyMode )
    {
        socket.write("QUIT\r\n");
        socket.waitForBytesWritten(5000);
        socket.disconnectFromHost();
        emit finished(true, QString());
        return;
    }

    // ---- MAIL FROM ----
    if ( !sendCommand(&socket, "MAIL FROM:<" + mailFromAddress.toUtf8() + ">\r\n", 250, resp) )
    {
        emit finished(false, tr("MAIL FROM rejected: %1").arg(QString::fromUtf8(resp)));
        return;
    }

    // ---- RCPT TO ----
    if ( !sendCommand(&socket, "RCPT TO:<" + mailToAddress.toUtf8() + ">\r\n", 250, resp) )
    {
        emit finished(false, tr("Recipient address not accepted: %1").arg(QString::fromUtf8(resp)));
        return;
    }

    // ---- DATA ----
    if ( !sendCommand(&socket, "DATA\r\n", 354, resp) )
    {
        emit finished(false, tr("DATA command rejected."));
        return;
    }

    // ---- Send message body ----
    QByteArray mime = buildMimeMessage();
    mime += "\r\n.\r\n";
    socket.write(mime);
    socket.flush();

    if ( !waitForResponse(&socket, resp) || responseCode(resp) != 250 )
    {
        emit finished(false, tr("Message rejected by server: %1").arg(QString::fromUtf8(resp)));
        return;
    }

    // ---- QUIT ----
    socket.write("QUIT\r\n");
    socket.waitForBytesWritten(5000);
    socket.disconnectFromHost();
    socket.waitForDisconnected(5000);

    emit finished(true, QString());
}

// ---------------------------------------------------------------------------
// EmailQSLService
// ---------------------------------------------------------------------------

EmailQSLService::EmailQSLService(QObject *parent)
    : QObject(parent)
{
    FCT_IDENTIFICATION;
}

void EmailQSLService::sendEmailQSL(const QSqlRecord &record)
{
    FCT_IDENTIFICATION;

    // Render card at full source resolution (fonts scale with image size)
    const QPixmap cardPixmap = EmailQSLBase::renderCard(record);
    QByteArray imageData;
    QString imageName;
    if ( !cardPixmap.isNull() )
    {
        // Scale down to EMAIL_WIDTH for the attachment — keeps file size reasonable
        // while still looking sharp on any screen.  The full-res pixmap is only
        // used when the user explicitly saves via "Save Card…".
        static constexpr int EMAIL_WIDTH = 1280;
        const QPixmap emailPixmap = (cardPixmap.width() > EMAIL_WIDTH)
            ? cardPixmap.scaledToWidth(EMAIL_WIDTH, Qt::SmoothTransformation)
            : cardPixmap;

        QBuffer buf(&imageData);
        buf.open(QIODevice::WriteOnly);
        emailPixmap.save(&buf, "JPEG", 90);
        imageName = QStringLiteral("qsl_card.jpg");
    }

    // Merge email fields
    const QString subject = EmailQSLBase::applyMergeFields(
        EmailQSLBase::getSubjectTemplate(), record);
    const QString body    = EmailQSLBase::applyMergeFields(
        EmailQSLBase::getBodyTemplate(), record);

    const QString toAddress = record.value(QStringLiteral("email")).toString().trimmed();

    SmtpWorker *worker = new SmtpWorker(
        EmailQSLBase::getSmtpHost(),
        EmailQSLBase::getSmtpPort(),
        EmailQSLBase::getSmtpEncryption(),
        EmailQSLBase::getSmtpAllowInvalidCert(),
        EmailQSLBase::getSmtpUsername(),
        EmailQSLBase::getSmtpPassword(),
        EmailQSLBase::getFromAddress(),
        EmailQSLBase::getFromName(),
        toAddress,
        subject, body,
        imageData, imageName);

    // Record the send even if the dialog that started it is closed first:
    // the application object, not this service, is the context.
    const int contactId = record.value(QStringLiteral("id")).toInt();
    connect(worker, &SmtpWorker::finished, QCoreApplication::instance(),
            [contactId](bool ok, const QString &)
    {
        if ( ok && !EmailQSLBase::recordEmailSent(contactId) )
            qCWarning(runtime) << "Email QSL was sent but could not be recorded for contact" << contactId;
    });
    // Delivered only while this service (and its dialog) still exists.
    connect(worker, &SmtpWorker::finished, this, &EmailQSLService::sendFinished);
    startWorker(worker);
}

void EmailQSLService::testConnection(const QString &host, int port, int encryption, bool allowInvalidCert,
                                     const QString &username, const QString &password)
{
    FCT_IDENTIFICATION;
    qCDebug(function_parameters) << host << port << encryption << allowInvalidCert << username;

    // Connect, EHLO, (STARTTLS), AUTH, then QUIT. No message is sent.
    SmtpWorker *worker = new SmtpWorker(
        host, port, encryption, allowInvalidCert, username, password,
        QString(), QString(), QString(), QString(), QString(),
        QByteArray(), QString());
    worker->setTestOnly(true);

    connect(worker, &SmtpWorker::finished, this, &EmailQSLService::testFinished);
    startWorker(worker);
}

void EmailQSLService::startWorker(SmtpWorker *worker)
{
    FCT_IDENTIFICATION;

    // The thread must not be a child of the service: the service lives in a
    // dialog the user can close mid-send, and destroying a running QThread
    // is fatal. Thread and worker delete themselves when the worker is done.
    QThread *thread = new QThread;
    worker->moveToThread(thread);

    connect(thread, &QThread::started,   worker, &SmtpWorker::run);
    connect(worker, &SmtpWorker::finished, worker, &QObject::deleteLater);
    connect(worker, &SmtpWorker::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished,   thread, &QObject::deleteLater);

    thread->start();
}
