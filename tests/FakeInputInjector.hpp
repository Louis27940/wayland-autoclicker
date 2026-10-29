#pragma once

#include "InputInjector.hpp"
#include <vector>
#include <mutex>
#include <chrono>

class FakeInputInjector : public InputInjector {
public:
    struct ButtonEvent {
        uint32_t button;
        bool pressed;
        std::chrono::steady_clock::time_point time;
    };

    struct MotionEvent {
        double dx;
        double dy;
        std::chrono::steady_clock::time_point time;
    };

    bool sendButton(uint32_t button, bool pressed) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_failButton) {
            return false;
        }
        if (m_failuresRemaining > 0) {
            --m_failuresRemaining;
            return false;
        }
        m_buttonEvents.push_back({button, pressed, std::chrono::steady_clock::now()});
        return true;
    }

    bool sendMotion(double dx, double dy) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_failMotion) {
            return false;
        }
        if (m_failuresRemaining > 0) {
            --m_failuresRemaining;
            return false;
        }
        m_motionEvents.push_back({dx, dy, std::chrono::steady_clock::now()});
        return true;
    }

    void setFailButton(bool fail) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_failButton = fail;
    }

    void setFailMotion(bool fail) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_failMotion = fail;
    }

    void setFailuresRemaining(int count) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_failuresRemaining = count;
    }

    std::vector<ButtonEvent> getButtonEvents() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_buttonEvents;
    }

    std::vector<MotionEvent> getMotionEvents() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_motionEvents;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_buttonEvents.clear();
        m_motionEvents.clear();
        m_failuresRemaining = 0;
        m_failButton = false;
        m_failMotion = false;
    }

private:
    mutable std::mutex m_mutex;
    std::vector<ButtonEvent> m_buttonEvents;
    std::vector<MotionEvent> m_motionEvents;
    int m_failuresRemaining{0};
    bool m_failButton{false};
    bool m_failMotion{false};
};
