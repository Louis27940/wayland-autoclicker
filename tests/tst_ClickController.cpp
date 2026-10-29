#include <QtTest>
#include <linux/input-event-codes.h>
#include "ClickController.hpp"
#include "FakeInputInjector.hpp"
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

class tst_ClickController : public QObject {
    Q_OBJECT

private slots:
    void testNormalClickCycle();
    void testRememberedButtonPressedRelease();
    void testImmediateStopDuringInterval();
    void testImmediateReleaseOnStopDuringPress();
    void testMicroMovementNetZero();
    void testMicroMovementCompensatedOnInterruption();
    void testConsecutiveFailuresThreshold();
};

void tst_ClickController::testNormalClickCycle() {
    FakeInputInjector injector;
    ClickController controller(&injector);

    controller.setInterval(200ms);
    controller.setJitter(0ms);
    controller.setMouseButton(BTN_LEFT);
    controller.setMode(ClickController::Mode::Click);
    controller.setStartCountdown(0s);

    QSignalSpy finishedSpy(&controller, &ClickController::runFinished);

    controller.start();
    // Allow 1.5 cycles (~350ms)
    QTest::qWait(350);
    controller.stop();

    auto events = injector.getButtonEvents();
    // Must have at least 1 press and 1 release
    QVERIFY(events.size() >= 2);
    QCOMPARE(events[0].button, static_cast<uint32_t>(BTN_LEFT));
    QCOMPARE(events[0].pressed, true);
    QCOMPARE(events[1].button, static_cast<uint32_t>(BTN_LEFT));
    QCOMPARE(events[1].pressed, false);

    // Verify press duration is within realistic bounds (20ms - 35ms, allow small timer jitter)
    auto pressDur = std::chrono::duration_cast<std::chrono::milliseconds>(events[1].time - events[0].time).count();
    QVERIFY(pressDur >= 15);
    QVERIFY(pressDur <= 50);
}

void tst_ClickController::testRememberedButtonPressedRelease() {
    FakeInputInjector injector;
    ClickController controller(&injector);

    controller.setInterval(300ms);
    controller.setMouseButton(BTN_LEFT);
    controller.setMode(ClickController::Mode::Click);
    controller.setStartCountdown(0s);

    controller.start();
    // Wait until press is recorded
    int attempts = 0;
    while (injector.getButtonEvents().empty() && attempts++ < 50) {
        QTest::qWait(10);
    }
    QVERIFY(!injector.getButtonEvents().empty());

    // Switch configured mouse button on the fly while button is pressed
    controller.setMouseButton(BTN_RIGHT);

    // Wait for release
    while (injector.getButtonEvents().size() < 2 && attempts++ < 50) {
        QTest::qWait(10);
    }
    controller.stop();

    auto events = injector.getButtonEvents();
    QVERIFY(events.size() >= 2);
    // Release must still be for BTN_LEFT, NOT BTN_RIGHT!
    QCOMPARE(events[0].button, static_cast<uint32_t>(BTN_LEFT));
    QCOMPARE(events[0].pressed, true);
    QCOMPARE(events[1].button, static_cast<uint32_t>(BTN_LEFT));
    QCOMPARE(events[1].pressed, false);
}

void tst_ClickController::testImmediateStopDuringInterval() {
    FakeInputInjector injector;
    ClickController controller(&injector);

    // 10 second interval
    controller.setInterval(10000ms);
    controller.setStartCountdown(0s);
    controller.start();

    // Wait for first click
    int attempts = 0;
    while (injector.getButtonEvents().size() < 2 && attempts++ < 50) {
        QTest::qWait(10);
    }
    QCOMPARE(injector.getButtonEvents().size(), 2UL);

    // Now worker is in a 10s wait. Stopping must be near-instantaneous (< 80ms)
    auto t0 = std::chrono::steady_clock::now();
    controller.stop();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();

    QVERIFY(elapsed < 100);
    QVERIFY(!controller.isRunning());
}

void tst_ClickController::testImmediateReleaseOnStopDuringPress() {
    FakeInputInjector injector;
    ClickController controller(&injector);

    controller.setInterval(1000ms);
    controller.setStartCountdown(0s);
    controller.start();

    // Stop right after press is detected
    int attempts = 0;
    while (injector.getButtonEvents().empty() && attempts++ < 50) {
        QTest::qWait(5);
    }
    controller.stop();

    auto events = injector.getButtonEvents();
    // Even if stopped during press duration, RAII guard must have released the button
    QVERIFY(events.size() >= 2);
    QCOMPARE(events[0].pressed, true);
    QCOMPARE(events.back().pressed, false);
    QCOMPARE(events.back().button, events[0].button);
}

void tst_ClickController::testMicroMovementNetZero() {
    FakeInputInjector injector;
    ClickController controller(&injector);

    controller.setInterval(200ms);
    controller.setMode(ClickController::Mode::MicroMovement);
    controller.setStartCountdown(0s);

    controller.start();
    QTest::qWait(450);
    controller.stop();

    auto motions = injector.getMotionEvents();
    QVERIFY(motions.size() >= 2);

    double netX = 0.0;
    double netY = 0.0;
    for (const auto& m : motions) {
        netX += m.dx;
        netY += m.dy;
    }
    QCOMPARE(netX, 0.0);
    QCOMPARE(netY, 0.0);
}

void tst_ClickController::testMicroMovementCompensatedOnInterruption() {
    FakeInputInjector injector;
    ClickController controller(&injector);

    controller.setInterval(500ms);
    controller.setMode(ClickController::Mode::MicroMovement);
    controller.setStartCountdown(0s);

    controller.start();
    // Wait until first motion (+1) occurs
    int attempts = 0;
    while (injector.getMotionEvents().size() < 1 && attempts++ < 50) {
        QTest::qWait(10);
    }
    // Stop immediately before next cycle
    controller.stop();

    auto motions = injector.getMotionEvents();
    double netX = 0.0;
    for (const auto& m : motions) {
        netX += m.dx;
    }
    // Net motion must remain 0 due to RAII compensation!
    QCOMPARE(netX, 0.0);
}

void tst_ClickController::testConsecutiveFailuresThreshold() {
    FakeInputInjector injector;
    ClickController controller(&injector);

    controller.setInterval(200ms);
    controller.setStartCountdown(0s);
    injector.setFailButton(true); // Every injection will fail

    QSignalSpy errorSpy(&controller, &ClickController::injectionErrorOccurred);
    QSignalSpy finishedSpy(&controller, &ClickController::runFinished);

    controller.start();
    QVERIFY(finishedSpy.wait(1000));

    // Must have stopped after threshold (3) failures
    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(!controller.isRunning());
}

QTEST_MAIN(tst_ClickController)
#include "tst_ClickController.moc"
