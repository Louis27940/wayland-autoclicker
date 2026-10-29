#include "Settings.hpp"
#include <linux/input-event-codes.h>

Settings::Settings(QObject* parent)
    : QObject(parent),
      m_settings(QStringLiteral("Perso"), QStringLiteral("WaylandAutoclicker")) {}

std::chrono::milliseconds Settings::interval() const {
    const int val = m_settings.value(QStringLiteral("preferences/interval_ms"), 1000).toInt();
    return std::chrono::milliseconds{val};
}

void Settings::setInterval(std::chrono::milliseconds interval) {
    m_settings.setValue(QStringLiteral("preferences/interval_ms"), static_cast<int>(interval.count()));
}

std::chrono::milliseconds Settings::jitter() const {
    const int val = m_settings.value(QStringLiteral("preferences/jitter_ms"), 80).toInt();
    return std::chrono::milliseconds{val};
}

void Settings::setJitter(std::chrono::milliseconds jitter) {
    m_settings.setValue(QStringLiteral("preferences/jitter_ms"), static_cast<int>(jitter.count()));
}

uint32_t Settings::mouseButton() const {
    const auto val = m_settings.value(QStringLiteral("preferences/mouse_button"), static_cast<uint>(BTN_LEFT)).toUInt();
    return val;
}

void Settings::setMouseButton(uint32_t button) {
    m_settings.setValue(QStringLiteral("preferences/mouse_button"), button);
}

ClickController::Mode Settings::mode() const {
    const int val = m_settings.value(QStringLiteral("preferences/mode"), 0).toInt();
    return (val == 1) ? ClickController::Mode::MicroMovement : ClickController::Mode::Click;
}

void Settings::setMode(ClickController::Mode mode) {
    m_settings.setValue(QStringLiteral("preferences/mode"), (mode == ClickController::Mode::MicroMovement) ? 1 : 0);
}

std::chrono::seconds Settings::autoStopLimit() const {
    const int val = m_settings.value(QStringLiteral("preferences/auto_stop_sec"), 0).toInt();
    return std::chrono::seconds{val};
}

void Settings::setAutoStopLimit(std::chrono::seconds limit) {
    m_settings.setValue(QStringLiteral("preferences/auto_stop_sec"), static_cast<int>(limit.count()));
}

std::chrono::seconds Settings::startCountdown() const {
    const int val = m_settings.value(QStringLiteral("preferences/countdown_sec"), 3).toInt();
    return std::chrono::seconds{val};
}

void Settings::setStartCountdown(std::chrono::seconds countdown) {
    m_settings.setValue(QStringLiteral("preferences/countdown_sec"), static_cast<int>(countdown.count()));
}

bool Settings::minimizeToTrayOnStart() const {
    return m_settings.value(QStringLiteral("preferences/minimize_to_tray"), false).toBool();
}

void Settings::setMinimizeToTrayOnStart(bool enable) {
    m_settings.setValue(QStringLiteral("preferences/minimize_to_tray"), enable);
}

QString Settings::restoreToken() const {
    return m_settings.value(QStringLiteral("portal/restore_token"), QString{}).toString();
}

void Settings::setRestoreToken(const QString& token) {
    m_settings.setValue(QStringLiteral("portal/restore_token"), token);
    m_settings.sync();
}

void Settings::sync() {
    m_settings.sync();
}
