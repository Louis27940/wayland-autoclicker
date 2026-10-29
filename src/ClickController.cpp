#include "ClickController.hpp"
#include <QMetaObject>
#include <algorithm>

using namespace std::chrono_literals;

ClickController::ClickController(InputInjector* injector, QObject* parent)
    : QObject(parent), m_injector(injector) {}

ClickController::~ClickController() {
    stop(StopReason::Manual);
}

void ClickController::setInterval(std::chrono::milliseconds interval) {
    m_intervalMs.store(TimingPolicy::clampInterval(interval).count(), std::memory_order_relaxed);
}

void ClickController::setJitter(std::chrono::milliseconds jitter) {
    auto currentInterval = std::chrono::milliseconds(m_intervalMs.load(std::memory_order_relaxed));
    m_jitterMs.store(TimingPolicy::clampJitter(jitter, currentInterval).count(), std::memory_order_relaxed);
}

void ClickController::setMouseButton(uint32_t button) {
    m_mouseButton.store(button, std::memory_order_relaxed);
}

void ClickController::setMode(Mode mode) {
    m_mode.store(mode, std::memory_order_relaxed);
}

void ClickController::setAutoStopLimit(std::chrono::seconds limit) {
    m_autoStopSec.store(std::max<int64_t>(0, limit.count()), std::memory_order_relaxed);
}

void ClickController::setStartCountdown(std::chrono::seconds countdown) {
    m_countdownSec.store(std::clamp<int64_t>(countdown.count(), 0, 30), std::memory_order_relaxed);
}

std::chrono::milliseconds ClickController::interval() const {
    return std::chrono::milliseconds{m_intervalMs.load(std::memory_order_relaxed)};
}

std::chrono::milliseconds ClickController::jitter() const {
    return std::chrono::milliseconds{m_jitterMs.load(std::memory_order_relaxed)};
}

uint32_t ClickController::mouseButton() const {
    return m_mouseButton.load(std::memory_order_relaxed);
}

ClickController::Mode ClickController::mode() const {
    return m_mode.load(std::memory_order_relaxed);
}

std::chrono::seconds ClickController::autoStopLimit() const {
    return std::chrono::seconds{m_autoStopSec.load(std::memory_order_relaxed)};
}

std::chrono::seconds ClickController::startCountdown() const {
    return std::chrono::seconds{m_countdownSec.load(std::memory_order_relaxed)};
}

bool ClickController::isRunning() const {
    return m_running.load(std::memory_order_relaxed);
}

void ClickController::start() {
    if (m_running.load(std::memory_order_relaxed)) {
        return;
    }

    if (m_workerThread.joinable()) {
        m_workerThread.request_stop();
        m_workerThread.join();
    }

    m_running.store(true, std::memory_order_relaxed);
    m_lastStopReason.store(StopReason::Manual, std::memory_order_relaxed);
    m_consecutiveFailures = 0;

    emit runStarted();

    m_workerThread = std::jthread([this](std::stop_token stopToken) {
        workerLoop(stopToken);
    });
}

void ClickController::stop(StopReason reason) {
    if (!m_running.exchange(false, std::memory_order_relaxed)) {
        return;
    }

    m_lastStopReason.store(reason, std::memory_order_relaxed);
    m_workerThread.request_stop();
    m_cv.notify_all();

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    emit runFinished(reason);
}

void ClickController::workerLoop(std::stop_token stopToken) {
    // 1. Initial Countdown
    const auto countdownTotal = m_countdownSec.load(std::memory_order_relaxed);
    for (int64_t remaining = countdownTotal; remaining > 0; --remaining) {
        if (stopToken.stop_requested()) {
            m_running.store(false, std::memory_order_relaxed);
            return;
        }
        QMetaObject::invokeMethod(
            this,
            [this, remaining] { emit countdownTick(static_cast<int>(remaining)); },
            Qt::QueuedConnection
        );

        std::unique_lock<std::mutex> lock(m_cvMutex);
        m_cv.wait_for(lock, stopToken, 1s, [&stopToken] {
            return stopToken.stop_requested();
        });
    }

    if (stopToken.stop_requested()) {
        m_running.store(false, std::memory_order_relaxed);
        return;
    }

    QMetaObject::invokeMethod(
        this,
        [this] { emit countdownTick(0); },
        Qt::QueuedConnection
    );

    const auto runStartTime = std::chrono::steady_clock::now();
    TimingPolicy timingPolicy;
    timingPolicy.resetGrid(runStartTime, std::chrono::milliseconds(m_intervalMs.load(std::memory_order_relaxed)));

    // RAII guards for failsafe button release and motion compensation
    std::optional<uint32_t> pressedButton;
    struct PressGuard {
        InputInjector* injector;
        std::optional<uint32_t>& button;
        ~PressGuard() {
            if (button.has_value() && injector) {
                injector->sendButton(*button, false);
                button.reset();
            }
        }
    } pressGuard{m_injector, pressedButton};

    double pendingMotionDx = 0.0;
    struct MotionGuard {
        InputInjector* injector;
        double& pendingDx;
        ~MotionGuard() {
            if (pendingDx != 0.0 && injector) {
                injector->sendMotion(-pendingDx, 0.0);
                pendingDx = 0.0;
            }
        }
    } motionGuard{m_injector, pendingMotionDx};

    StopReason stopReason = StopReason::Manual;

    while (!stopToken.stop_requested()) {
        const auto now = std::chrono::steady_clock::now();
        const auto currentInterval = TimingPolicy::clampInterval(
            std::chrono::milliseconds(m_intervalMs.load(std::memory_order_relaxed))
        );
        const auto currentJitter = TimingPolicy::clampJitter(
            std::chrono::milliseconds(m_jitterMs.load(std::memory_order_relaxed)),
            currentInterval
        );
        const auto currentMode = m_mode.load(std::memory_order_relaxed);
        const auto currentAutoStop = std::chrono::seconds(m_autoStopSec.load(std::memory_order_relaxed));
        const auto currentButton = m_mouseButton.load(std::memory_order_relaxed);

        // Check Auto-Stop Timer
        if (TimingPolicy::isAutoStopExceeded(runStartTime, now, currentAutoStop)) {
            stopReason = StopReason::AutoStopTimer;
            break;
        }

        if (currentAutoStop > 0s) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - runStartTime);
            const auto remainingSec = static_cast<int>(std::max<int64_t>(0, (currentAutoStop - elapsed).count()));
            QMetaObject::invokeMethod(
                this,
                [this, remainingSec] { emit autoStopTick(remainingSec); },
                Qt::QueuedConnection
            );
        }

        // Perform Action
        if (currentMode == Mode::Click) {
            if (!m_injector || !m_injector->sendButton(currentButton, true)) {
                ++m_consecutiveFailures;
                if (m_consecutiveFailures >= ConsecutiveFailureThreshold) {
                    stopReason = StopReason::InjectionFailure;
                    QMetaObject::invokeMethod(
                        this,
                        [this] { emit injectionErrorOccurred(QStringLiteral("Injection failed 3 consecutive times")); },
                        Qt::QueuedConnection
                    );
                    break;
                }
            } else {
                pressedButton = currentButton;
                m_consecutiveFailures = 0;
            }

            const auto pressDuration = TimingPolicy::randomPressDuration(currentInterval);
            {
                std::unique_lock<std::mutex> lock(m_cvMutex);
                m_cv.wait_for(lock, stopToken, pressDuration, [&stopToken] {
                    return stopToken.stop_requested();
                });
            }

            if (stopToken.stop_requested()) {
                break;
            }

            if (pressedButton.has_value()) {
                m_injector->sendButton(*pressedButton, false);
                pressedButton.reset();
            }
        } else {
            // Mode::MicroMovement: relative dx = +1, wait short duration, relative dx = -1
            if (!m_injector || !m_injector->sendMotion(1.0, 0.0)) {
                ++m_consecutiveFailures;
                if (m_consecutiveFailures >= ConsecutiveFailureThreshold) {
                    stopReason = StopReason::InjectionFailure;
                    QMetaObject::invokeMethod(
                        this,
                        [this] { emit injectionErrorOccurred(QStringLiteral("Injection failed 3 consecutive times")); },
                        Qt::QueuedConnection
                    );
                    break;
                }
            } else {
                pendingMotionDx = 1.0;
                m_consecutiveFailures = 0;
            }

            {
                std::unique_lock<std::mutex> lock(m_cvMutex);
                m_cv.wait_for(lock, stopToken, 25ms, [&stopToken] {
                    return stopToken.stop_requested();
                });
            }

            if (stopToken.stop_requested()) {
                break;
            }

            if (pendingMotionDx != 0.0) {
                m_injector->sendMotion(-pendingMotionDx, 0.0);
                pendingMotionDx = 0.0;
            }
        }

        // Wait for next grid deadline
        const auto nextDeadline = timingPolicy.computeNextExecution(
            std::chrono::steady_clock::now(),
            currentInterval,
            currentJitter
        );

        std::unique_lock<std::mutex> lock(m_cvMutex);
        m_cv.wait_until(lock, stopToken, nextDeadline, [&stopToken] {
            return stopToken.stop_requested();
        });
    }

    m_running.store(false, std::memory_order_relaxed);

    if (stopToken.stop_requested()) {
        stopReason = m_lastStopReason.load(std::memory_order_relaxed);
    }

    QMetaObject::invokeMethod(
        this,
        [this, stopReason] { emit runFinished(stopReason); },
        Qt::QueuedConnection
    );
}
