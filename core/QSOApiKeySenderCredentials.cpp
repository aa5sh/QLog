#include "QSOApiKeySender.h"
#include "core/debug.h"
#include "core/LogParam.h"

MODULE_IDENTIFICATION("qlog.core.qsoapikeysenderbase");

const QString QSOApiKeySenderBase::SECURE_STORAGE_KEY = "QSOApiKey";
const QString QSOApiKeySenderBase::CONFIG_USERNAME_CONST = "qsoapikey";

REGISTRATION_SECURE_SERVICE(QSOApiKeySenderBase);

void QSOApiKeySenderBase::registerCredentials()
{
    CredentialRegistry::instance().add(SECURE_STORAGE_KEY, []()
    {
        return QList<CredentialDescriptor>
        {
            { SECURE_STORAGE_KEY, [](){ return getUsername(); } }
        };
    });
}

QString QSOApiKeySenderBase::getAPIKey()
{
    FCT_IDENTIFICATION;

    return getPassword(SECURE_STORAGE_KEY, getUsername());
}

void QSOApiKeySenderBase::saveAPIKey(const QString &newKey)
{
    FCT_IDENTIFICATION;

    deletePassword(SECURE_STORAGE_KEY, getUsername());

    if (newKey.isEmpty())
        return;

    savePassword(SECURE_STORAGE_KEY, getUsername(), newKey);
}

bool QSOApiKeySenderBase::getEnabled()
{
    FCT_IDENTIFICATION;

    return LogParam::getNetworkQSOApiEnabled();
}

void QSOApiKeySenderBase::setEnabled(bool enabled)
{
    FCT_IDENTIFICATION;

    LogParam::setNetworkQSOApiEnabled(enabled);
}

QString QSOApiKeySenderBase::getURL()
{
    FCT_IDENTIFICATION;

    return LogParam::getNetworkQSOApiURL();
}

void QSOApiKeySenderBase::setURL(const QString &url)
{
    FCT_IDENTIFICATION;

    LogParam::setNetworkQSOApiURL(url);
}
