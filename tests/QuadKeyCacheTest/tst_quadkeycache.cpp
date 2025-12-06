#include <QtTest>

#include "core/QuadKeyCache.h"

namespace {
struct DummyValue
{
    DummyValue(int v = 0) : payload(v) {}
    int payload = 0;
};
}

class QuadKeyCacheTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void insertAndRetrieve();
    void containsCheck();
    void invalidateByPair();
    void evictionFollowsCapacity();
};

void QuadKeyCacheTest::initTestCase()
{
     QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
}

void QuadKeyCacheTest::insertAndRetrieve()
{
    QuadKeyCache<DummyValue> cache;

    auto value1 = new DummyValue{42};
    auto value2 = new DummyValue{73};

    cache.insert(1, 10, QStringLiteral("A"), QStringLiteral("alpha"), value1);
    cache.insert(2, 20, QStringLiteral("B"), QStringLiteral("beta"), value2);

    DummyValue *retrieved1 = cache.value(1, 10, QStringLiteral("A"), QStringLiteral("alpha"));
    DummyValue *retrieved2 = cache.value(2, 20, QStringLiteral("B"), QStringLiteral("beta"));

    QVERIFY(retrieved1 != nullptr);
    QVERIFY(retrieved2 != nullptr);
    QCOMPARE(retrieved1->payload, 42);
    QCOMPARE(retrieved2->payload, 73);
    QCOMPARE(cache.size(), 2);
}

void QuadKeyCacheTest::containsCheck()
{
    QuadKeyCache<DummyValue> cache;
    auto value = new DummyValue{5};
    cache.insert(3, 30, QStringLiteral("C"), QStringLiteral("gamma"), value);

    QVERIFY(cache.contains(3, 30, QStringLiteral("C"), QStringLiteral("gamma")));
    QVERIFY(!cache.contains(4, 40, QStringLiteral("D"), QStringLiteral("delta")));
}

void QuadKeyCacheTest::invalidateByPair()
{
    QuadKeyCache<DummyValue> cache;
    cache.insert(1, 1, QStringLiteral("X"), QStringLiteral("x"), new DummyValue{1});
    cache.insert(1, 1, QStringLiteral("Y"), QStringLiteral("y"), new DummyValue{2});
    cache.insert(2, 2, QStringLiteral("X"), QStringLiteral("x"), new DummyValue{3});

    QCOMPARE(cache.size(), 3);

    cache.invalidate(1, 1);

    QCOMPARE(cache.size(), 1);
    QVERIFY(cache.contains(2, 2, QStringLiteral("X"), QStringLiteral("x")));
    QVERIFY(!cache.contains(1, 1, QStringLiteral("X"), QStringLiteral("x")));
    QVERIFY(!cache.contains(1, 1, QStringLiteral("Y"), QStringLiteral("y")));
}

void QuadKeyCacheTest::evictionFollowsCapacity()
{
    QuadKeyCache<DummyValue> cache;
    cache.setMaxCost(2);

    cache.insert(1, 1, QStringLiteral("A"), QStringLiteral("a"), new DummyValue{1});
    cache.insert(2, 2, QStringLiteral("B"), QStringLiteral("b"), new DummyValue{2});
    cache.insert(3, 3, QStringLiteral("C"), QStringLiteral("c"), new DummyValue{3});

    QCOMPARE(cache.size(), 2);
    QVERIFY(cache.value(1, 1, QStringLiteral("A"), QStringLiteral("a")) == nullptr);
    QVERIFY(cache.value(2, 2, QStringLiteral("B"), QStringLiteral("b")) != nullptr);
    QVERIFY(cache.value(3, 3, QStringLiteral("C"), QStringLiteral("c")) != nullptr);
}

QTEST_APPLESS_MAIN(QuadKeyCacheTest)

#include "tst_quadkeycache.moc"
