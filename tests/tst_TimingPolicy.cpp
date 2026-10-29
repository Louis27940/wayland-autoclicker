#include <QtTest>
#include "TimingPolicy.hpp"
#include <chrono>

using namespace std::chrono_literals;

class tst_TimingPolicy : public QObject {
    Q_OBJECT

private slots:
    void testIntervalClamp();
    void testJitterClamp();
    void testPressDurationBounds();
    void testDeadlineNoDrift();
    void testAutoStopSemantic();
    void testCatchUpPrevention();
};

void tst_TimingPolicy::testIntervalClamp() {
    QCOMPARE(TimingPolicy::clampInterval(50ms), 200ms);
    QCOMPARE(TimingPolicy::clampInterval(200ms), 200ms);
    QCOMPARE(TimingPolicy::clampInterval(1000ms), 1000ms);
    QCOMPARE(TimingPolicy::clampInterval(700000ms), 600000ms);
}

void tst_TimingPolicy::testJitterClamp() {
    // 50% of 1000ms is 500ms
    QCOMPARE(TimingPolicy::clampJitter(80ms, 1000ms), 80ms);
    QCOMPARE(TimingPolicy::clampJitter(600ms, 1000ms), 500ms);
    QCOMPARE(TimingPolicy::clampJitter(-10ms, 1000ms), 0ms);
    // 50% of 200ms is 100ms
    QCOMPARE(TimingPolicy::clampJitter(150ms, 200ms), 100ms);
}

void tst_TimingPolicy::testPressDurationBounds() {
    // Standard interval 1000ms: bounds should be [20ms, 35ms]
    auto [minBound, maxBound] = TimingPolicy::calculatePressDurationBounds(1000ms);
    QCOMPARE(minBound, 20ms);
    QCOMPARE(maxBound, 35ms);

    // Minimum interval 200ms: 35% of 200ms = 70ms > 35ms, so still [20ms, 35ms]
    auto [min200, max200] = TimingPolicy::calculatePressDurationBounds(200ms);
    QCOMPARE(min200, 20ms);
    QCOMPARE(max200, 35ms);

    // Small interval hypothetical (e.g. 50ms): 35% is 17.5ms -> max = 17ms, min = min(20, 17) = 17ms
    auto [min50, max50] = TimingPolicy::calculatePressDurationBounds(50ms);
    QVERIFY(min50 <= max50);
    QCOMPARE(max50, 17ms);
    QCOMPARE(min50, 17ms);

    // Generate multiple random press durations and verify they fall within bounds
    for (int i = 0; i < 100; ++i) {
        auto dur = TimingPolicy::randomPressDuration(1000ms);
        QVERIFY(dur >= 20ms);
        QVERIFY(dur <= 35ms);
    }
}

void tst_TimingPolicy::testDeadlineNoDrift() {
    auto start = std::chrono::steady_clock::now();
    TimingPolicy policy;
    policy.resetGrid(start, 1000ms);

    // Without jitter
    auto d1 = policy.nextNominalDeadline();
    QCOMPARE(std::chrono::duration_cast<std::chrono::milliseconds>(d1 - start).count(), 1000);

    auto d2 = policy.nextNominalDeadline();
    QCOMPARE(std::chrono::duration_cast<std::chrono::milliseconds>(d2 - start).count(), 2000);

    // Jitter applies non-cumulatively
    auto j1 = policy.applyJitter(d1, 50ms);
    QVERIFY(j1 >= d1 - 50ms && j1 <= d1 + 50ms);
}

void tst_TimingPolicy::testAutoStopSemantic() {
    auto start = std::chrono::steady_clock::now();
    
    // 0 = disabled (unlimited)
    QVERIFY(!TimingPolicy::isAutoStopExceeded(start, start + 10h, 0s));

    // Limit of 10s
    QVERIFY(!TimingPolicy::isAutoStopExceeded(start, start + 5s, 10s));
    QVERIFY(TimingPolicy::isAutoStopExceeded(start, start + 10s, 10s));
    QVERIFY(TimingPolicy::isAutoStopExceeded(start, start + 11s, 10s));

    // Changing limit on the fly: compared against total elapsed, no reset
    auto elapsed = 12s;
    QVERIFY(TimingPolicy::isAutoStopExceeded(start, start + elapsed, 10s));
    // If increased to 15s, not exceeded yet
    QVERIFY(!TimingPolicy::isAutoStopExceeded(start, start + elapsed, 15s));
    // If reduced to 5s, exceeded
    QVERIFY(TimingPolicy::isAutoStopExceeded(start, start + elapsed, 5s));
}

void tst_TimingPolicy::testCatchUpPrevention() {
    auto start = std::chrono::steady_clock::now();
    TimingPolicy policy;
    policy.resetGrid(start, 200ms);

    // Simulate lagging behind by 5000ms
    auto nowLagged = start + 5000ms;
    auto next = policy.computeNextExecution(nowLagged, 200ms, 0ms);
    
    // Next execution must be strictly in the future or now, not queued catch-up
    QVERIFY(next >= nowLagged);
}

QTEST_MAIN(tst_TimingPolicy)
#include "tst_TimingPolicy.moc"
