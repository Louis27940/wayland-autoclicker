#pragma once

#include <QObject>
#include <linux/input-event-codes.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include "InputInjector.hpp"
#include "TimingPolicy.hpp"

class ClickController : public QObject {
    Q_OBJECT

public:
    enum class Mode {
        Click,
        MicroMovement
    };
    Q_ENUM(Mode)

    enum class StopReason {
        Manual,
        AutoStopTimer,
        InjectionFailure,
        SessionClosed,
        SignalTerminated
    };
    Q_ENUM(StopReason)

    explicit ClickController(InputInjector* injector, QObject* parent = nullptr);
    ~ClickController() override;

    // Configuration methods (thread-safe atomics)
    void setInterval(std::chrono::milliseconds interval);
    void setJitter(std::chrono::milliseconds jitter);
    void setMouseButton(uint32_t button);
    void setMode(Mode mode);
    void setAutoStopLimit(std::chrono::seconds limit);
    void setStartCountdown(std::chrono::seconds countdown);

    [[nodiscard]] std::chrono::milliseconds interval() const;
    [[nodiscard]] std::chrono::milliseconds jitter() const;
    [[nodiscard]] uint32_t mouseButton() const;
    [[nodiscard]] Mode mode() const;
    [[nodiscard]] std::chrono::seconds autoStopLimit() const;
    [[nodiscard]] std::chrono::seconds startCountdown() const;

    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] uint64_t actionCount() const;

public slots:
    void start();
    void stop(StopReason reason = StopReason::Manual);

signals:
    void runStarted();
    void runFinished(ClickController::StopReason reason);
    void countdownTick(int remainingSeconds);
    void autoStopTick(int remainingSeconds);
    void injectionErrorOccurred(const QString& errorMessage);

private:
    void workerLoop(std::stop_token stopToken);

    InputInjector* m_injector{nullptr};
    std::jthread m_workerThread;
    std::mutex m_cvMutex;
    std::condition_variable_any m_cv;

    std::atomic<int64_t> m_intervalMs{TimingPolicy::DefaultInterval.count()};
    std::atomic<int64_t> m_jitterMs{TimingPolicy::DefaultJitter.count()};
    std::atomic<uint32_t> m_mouseButton{static_cast<uint32_t>(BTN_LEFT)};
    std::atomic<Mode> m_mode{Mode::Click};
    std::atomic<int64_t> m_autoStopSec{TimingPolicy::DefaultAutoStop.count()};
    std::atomic<int64_t> m_countdownSec{TimingPolicy::DefaultCountdown.count()};

    std::atomic<bool> m_running{false};
    std::atomic<uint64_t> m_actionCount{0};
    std::atomic<StopReason> m_lastStopReason{StopReason::Manual};
    int m_consecutiveFailures{0};
    static constexpr int ConsecutiveFailureThreshold = 3;
};
