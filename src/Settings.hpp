#pragma once

#include <QObject>
#include <QSettings>
#include <chrono>
#include "ClickController.hpp"

class Settings : public QObject {
    Q_OBJECT

public:
    explicit Settings(QObject* parent = nullptr);

    // Application Settings
    [[nodiscard]] std::chrono::milliseconds interval() const;
    void setInterval(std::chrono::milliseconds interval);

    [[nodiscard]] std::chrono::milliseconds jitter() const;
    void setJitter(std::chrono::milliseconds jitter);

    [[nodiscard]] uint32_t mouseButton() const;
    void setMouseButton(uint32_t button);

    [[nodiscard]] ClickController::Mode mode() const;
    void setMode(ClickController::Mode mode);

    [[nodiscard]] std::chrono::seconds autoStopLimit() const;
    void setAutoStopLimit(std::chrono::seconds limit);

    [[nodiscard]] std::chrono::seconds startCountdown() const;
    void setStartCountdown(std::chrono::seconds countdown);

    [[nodiscard]] bool minimizeToTrayOnStart() const;
    void setMinimizeToTrayOnStart(bool enable);

    // Dedicated token persistence
    [[nodiscard]] QString restoreToken() const;
    void setRestoreToken(const QString& token);

    void sync();

private:
    QSettings m_settings;
};
