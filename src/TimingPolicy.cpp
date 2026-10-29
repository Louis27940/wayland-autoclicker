#include "TimingPolicy.hpp"
#include <algorithm>

using namespace std::chrono_literals;

namespace {
thread_local std::mt19937_64 g_rng{std::random_device{}()};
}

std::chrono::milliseconds TimingPolicy::clampInterval(std::chrono::milliseconds interval) {
    return std::clamp(interval, MinInterval, MaxInterval);
}

std::chrono::milliseconds TimingPolicy::clampJitter(
    std::chrono::milliseconds jitter,
    std::chrono::milliseconds interval
) {
    const auto clampedInterval = clampInterval(interval);
    const auto maxJitter = clampedInterval / 2;
    return std::clamp(jitter, 0ms, maxJitter);
}

std::pair<std::chrono::milliseconds, std::chrono::milliseconds>
TimingPolicy::calculatePressDurationBounds(std::chrono::milliseconds interval) {
    const auto clampedInterval = clampInterval(interval);
    const auto percent35 = std::chrono::duration_cast<std::chrono::milliseconds>(clampedInterval * 35 / 100);
    const auto maxBound = std::min(35ms, percent35);
    const auto minBound = std::min(20ms, maxBound);
    return {minBound, maxBound};
}

std::chrono::milliseconds TimingPolicy::randomPressDuration(std::chrono::milliseconds interval) {
    const auto [minBound, maxBound] = calculatePressDurationBounds(interval);
    if (minBound >= maxBound) {
        return minBound;
    }
    std::uniform_int_distribution<long long> dist(minBound.count(), maxBound.count());
    return std::chrono::milliseconds{dist(g_rng)};
}

bool TimingPolicy::isAutoStopExceeded(
    std::chrono::steady_clock::time_point startTime,
    std::chrono::steady_clock::time_point currentTime,
    std::chrono::seconds autoStopLimit
) {
    if (autoStopLimit <= 0s) {
        return false;
    }
    return (currentTime - startTime) >= autoStopLimit;
}

void TimingPolicy::resetGrid(
    std::chrono::steady_clock::time_point start,
    std::chrono::milliseconds interval
) {
    m_gridInterval = clampInterval(interval);
    m_currentDeadline = start;
}

std::chrono::steady_clock::time_point TimingPolicy::nextNominalDeadline() {
    m_currentDeadline += m_gridInterval;
    return m_currentDeadline;
}

std::chrono::steady_clock::time_point TimingPolicy::applyJitter(
    std::chrono::steady_clock::time_point nominal,
    std::chrono::milliseconds clampedJitter
) {
    if (clampedJitter <= 0ms) {
        return nominal;
    }
    std::uniform_int_distribution<long long> dist(-clampedJitter.count(), clampedJitter.count());
    return nominal + std::chrono::milliseconds{dist(g_rng)};
}

std::chrono::steady_clock::time_point TimingPolicy::computeNextExecution(
    std::chrono::steady_clock::time_point now,
    std::chrono::milliseconds interval,
    std::chrono::milliseconds jitter
) {
    m_gridInterval = clampInterval(interval);
    const auto clampedJitter = clampJitter(jitter, m_gridInterval);

    // If lagged behind, advance deadline to future to prevent catch-up burst
    if (m_currentDeadline < now) {
        const auto diff = now - m_currentDeadline;
        const auto missedSteps = (diff / m_gridInterval) + 1;
        m_currentDeadline += missedSteps * m_gridInterval;
    } else {
        m_currentDeadline += m_gridInterval;
    }

    auto executionTime = applyJitter(m_currentDeadline, clampedJitter);
    if (executionTime < now) {
        executionTime = now;
    }
    return executionTime;
}
