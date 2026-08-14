// Stubs for QSOApiKeySenderTest.
//
// sendQSO() (the thing under test) never touches LogParam/CredentialStore -
// that's the whole point of the split (see core/QSOApiKeySender.h). But
// QSOInserted() lives in the same translation unit and does call the
// QSOApiKeySenderBase statics, so the linker still needs *some* definition
// for them. Stubbing those here keeps this test target free of
// CredentialStore.cpp/PasswordCipher.cpp/QtKeychain, exactly like
// tests/AdiFormatTest/test_stubs.cpp keeps AdiFormat's tests free of the
// real Data.cpp/LogFormat.cpp.

#include "core/QSOApiKeySender.h"
#include "data/Data.h"
#include "logformat/LogFormat.h"

const QString QSOApiKeySenderBase::SECURE_STORAGE_KEY = QStringLiteral("QSOApiKeySenderTestStub");
const QString QSOApiKeySenderBase::CONFIG_USERNAME_CONST = QStringLiteral("test");

void QSOApiKeySenderBase::registerCredentials()
{
}

int QSOApiKeySenderBase::QSOApiKeySenderBaseRegistrationDummy =
    QSOApiKeySenderBase::QSOApiKeySenderBaseForceRegistration();

bool QSOApiKeySenderBase::getEnabled() { return true; }
void QSOApiKeySenderBase::setEnabled(bool) {}
QString QSOApiKeySenderBase::getURL() { return QString(); }
void QSOApiKeySenderBase::setURL(const QString &) {}
QString QSOApiKeySenderBase::getAPIKey() { return QString(); }
void QSOApiKeySenderBase::saveAPIKey(const QString &) {}

LogFormat::LogFormat(QTextStream &stream) :
    QObject(nullptr),
    stream(stream),
    exportedFields(QStringLiteral("*")),
    duplicateQSOFunc(nullptr)
{
    defaults = nullptr;
}

LogFormat::~LogFormat() = default;

void LogFormat::setDefaults(QMap<QString, QString> &defaults)
{
    this->defaults = &defaults;
}

Data::Data(QObject *parent) :
    QObject(parent)
{
}

Data::~Data() = default;

QPair<QString, QString> Data::legacyMode(const QString &)
{
    return {};
}

void Data::invalidateDXCCStatusCache(const QSqlRecord &)
{
}

void Data::invalidateSetOfDXCCStatusCache(const QSet<uint> &)
{
}

void Data::clearDXCCStatusCache()
{
}
