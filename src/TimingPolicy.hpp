#pragma once

#include <chrono>
#include <random>
#include <utility>

class TimingPolicy {
public:
    static constexpr std::chrono::milliseconds MinInterval{200};
    static constexpr std::chrono::milliseconds MaxInterval{600000};
    static constexpr std::chrono::milliseconds DefaultInterval{1000};
    static constexpr std::chrono::milliseconds DefaultJitter{80};
    static constexpr std::chrono::seconds DefaultCountdown{3};
    static constexpr std::chrono::seconds DefaultAutoStop{0};

    static std::chrono::milliseconds clampInterval(std::chrono::milliseconds interval);
    static std::chrono::milliseconds clampJitter(std::chrono::milliseconds jitter, std::chrono::milliseconds interval);
    static std::pair<std::chrono::milliseconds, std::chrono::milliseconds> calculatePressDurationBounds(std::chrono::milliseconds interval);
    static std::chrono::milliseconds randomPressDuration(std::chrono::milliseconds interval);
    
    static bool isAutoStopExceeded(
        std::chrono::steady_clock::time_point startTime,
        std::chrono::steady_clock::time_point currentTime,
        std::chrono::seconds autoStopLimit
    );

    void resetGrid(std::chrono::steady_clock::time_point start, std::chrono::milliseconds interval);
    std::chrono::steady_clock::time_point nextNominalDeadline();
    std::chrono::steady_clock::time_point applyJitter(
        std::chrono::steady_clock::time_point nominal,
        std::chrono::milliseconds clampedJitter
    );
    std::chrono::steady_clock::time_point computeNextExecution(
        std::chrono::steady_clock::time_point now,
        std::chrono::milliseconds interval,
        std::chrono::milliseconds jitter
    );

private:
    std::chrono::steady_clock::time_point m_currentDeadline{};
    std::chrono::milliseconds m_gridInterval{DefaultInterval};
};
