#include <QtTest>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QMessageBox>
#include <QTimer>

#include "core/CredentialStore.h"

namespace {
QtMessageHandler previousHandler = nullptr;

void credentialStoreTestMessageHandler(QtMsgType type, const QMessageLogContext &context,
                                       const QString &message)
{
    if (type == QtWarningMsg)
    {
        if (message.contains(QStringLiteral("This plugin does not support propagateSizeHints()")))
            return;
        if (message.contains(QStringLiteral("Cannot save a password. Error")))
            return;
    }

    if (previousHandler)
        previousHandler(type, context, message);
}

struct ScopedMessageHandler
{
    ScopedMessageHandler()
    {
        previousHandler = qInstallMessageHandler(credentialStoreTestMessageHandler);
    }

    ~ScopedMessageHandler()
    {
        qInstallMessageHandler(previousHandler);
        previousHandler = nullptr;
    }
};
}

class CredentialStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void instance_isStable();
    void savePassword_rejectsEmptyInput_data();
    void savePassword_rejectsEmptyInput();
    void getPassword_emptyKeyOrUser_returnsEmpty();
    void deletePassword_emptyKeyOrUser_noCrash();
    void saveGetDelete_worksAndCleansUp();
    void getPassword_benchmark_keychain();
};

void CredentialStoreTest::initTestCase()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
}

void CredentialStoreTest::instance_isStable()
{
    CredentialStore *first = CredentialStore::instance();
    CredentialStore *second = CredentialStore::instance();
    QVERIFY(first != nullptr);
    QCOMPARE(first, second);
}

void CredentialStoreTest::savePassword_rejectsEmptyInput_data()
{
    QTest::addColumn<QString>("storageKey");
    QTest::addColumn<QString>("user");
    QTest::addColumn<QString>("pass");

    QTest::newRow("empty_all") << QString() << QString() << QString();
    QTest::newRow("empty_key") << QString() << QStringLiteral("u") << QStringLiteral("p");
    QTest::newRow("empty_user") << QStringLiteral("k") << QString() << QStringLiteral("p");
    QTest::newRow("empty_pass") << QStringLiteral("k") << QStringLiteral("u") << QString();
}

void CredentialStoreTest::savePassword_rejectsEmptyInput()
{
    QFETCH(QString, storageKey);
    QFETCH(QString, user);
    QFETCH(QString, pass);

    CredentialStore *store = CredentialStore::instance();
    QVERIFY(store != nullptr);
    QCOMPARE(store->savePassword(storageKey, user, pass), 1);
}

void CredentialStoreTest::getPassword_emptyKeyOrUser_returnsEmpty()
{
    CredentialStore *store = CredentialStore::instance();
    QVERIFY(store != nullptr);

    QCOMPARE(store->getPassword(QString(), QStringLiteral("u")), QString());
    QCOMPARE(store->getPassword(QStringLiteral("k"), QString()), QString());
    QCOMPARE(store->getPassword(QString(), QString()), QString());
}

void CredentialStoreTest::deletePassword_emptyKeyOrUser_noCrash()
{
    CredentialStore *store = CredentialStore::instance();
    QVERIFY(store != nullptr);

    store->deletePassword(QString(), QStringLiteral("u"));
    store->deletePassword(QStringLiteral("k"), QString());
    store->deletePassword(QString(), QString());
    QVERIFY(true);
}

void CredentialStoreTest::saveGetDelete_worksAndCleansUp()
{
    CredentialStore *store = CredentialStore::instance();
    QVERIFY(store != nullptr);

    //const ScopedMessageHandler handlerScope;

    QTimer messageBoxCloser;
    messageBoxCloser.setInterval(25);
    messageBoxCloser.setSingleShot(false);
    QObject::connect(&messageBoxCloser, &QTimer::timeout, this, []() {
        const auto widgets = QApplication::topLevelWidgets();
        for (QWidget *widget : widgets)
        {
            if (auto *box = qobject_cast<QMessageBox *>(widget))
                box->reject();
        }
    });
    messageBoxCloser.start();

    const QString unique = QStringLiteral("%1-%2")
                               .arg(QCoreApplication::applicationPid())
                               .arg(QDateTime::currentMSecsSinceEpoch());
    const QString testKey = QStringLiteral("CredentialStoreTest-%1").arg(unique);
    const QString testUser = QStringLiteral("user-%1").arg(unique);
    const QString testPwd = QStringLiteral("pass-%1").arg(unique);

    struct Cleanup
    {
        CredentialStore *store;
        QString key;
        QString user;
        ~Cleanup()
        {
            if (store)
                store->deletePassword(key, user);
        }
    } cleanup{store, testKey, testUser};

    store->deletePassword(testKey, testUser);
    QCOMPARE(store->getPassword(testKey, testUser), QString());

    const int saveResult = store->savePassword(testKey, testUser, testPwd);
    if (saveResult != 0)
        QSKIP("Credential store backend not available (save failed).");

    QCOMPARE(store->getPassword(testKey, testUser), testPwd);

    store->deletePassword(testKey, testUser);
    QCOMPARE(store->getPassword(testKey, testUser), QString());
}

void CredentialStoreTest::getPassword_benchmark_keychain()
{
    CredentialStore *store = CredentialStore::instance();
    QVERIFY(store != nullptr);

    const ScopedMessageHandler handlerScope;

    QTimer messageBoxCloser;
    messageBoxCloser.setInterval(25);
    messageBoxCloser.setSingleShot(false);
    QObject::connect(&messageBoxCloser, &QTimer::timeout, []() {
        const auto widgets = QApplication::topLevelWidgets();
        for (QWidget *widget : widgets)
        {
            if (auto *box = qobject_cast<QMessageBox *>(widget))
                box->reject();
        }
    });
    messageBoxCloser.start();

    const QString unique = QStringLiteral("%1-%2-bench")
                               .arg(QCoreApplication::applicationPid())
                               .arg(QDateTime::currentMSecsSinceEpoch());
    const QString testKey = QStringLiteral("CredentialStoreBench-%1").arg(unique);
    const QString testUser = QStringLiteral("user-%1").arg(unique);
    const QString testPwd = QStringLiteral("pass-%1").arg(unique);

    struct Cleanup
    {
        CredentialStore *store;
        QString key;
        QString user;
        ~Cleanup()
        {
            if (store)
                store->deletePassword(key, user);
        }
    } cleanup{store, testKey, testUser};

    store->deletePassword(testKey, testUser);
    QCOMPARE(store->getPassword(testKey, testUser), QString());

    const int saveResult = store->savePassword(testKey, testUser, testPwd);
    if (saveResult != 0)
        QSKIP("Credential store backend not available (save failed).");

    QCOMPARE(store->getPassword(testKey, testUser), testPwd);

    QString out;
    QBENCHMARK { out = store->getPassword(testKey, testUser); }
    QCOMPARE(out, testPwd);
}

int main(int argc, char **argv)
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QApplication app(argc, argv);
    CredentialStoreTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_credentialstore.moc"
