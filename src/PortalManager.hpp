#pragma once

#include <QObject>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QVariantMap>
#include <QString>
#include <QDBusArgument>
#include <QDBusMetaType>
#include <atomic>
#include <memory>
#include <mutex>
#include "InputInjector.hpp"

struct PortalShortcut {
    QString id;
    QVariantMap options;
};
Q_DECLARE_METATYPE(PortalShortcut)

QDBusArgument &operator<<(QDBusArgument &argument, const PortalShortcut &shortcut);
const QDBusArgument &operator>>(const QDBusArgument &argument, PortalShortcut &shortcut);

class PortalManager : public QObject, public InputInjector {
    Q_OBJECT

public:
    enum class PortalState {
        Uninitialized,
        CreatingSession,
        SelectingDevices,
        Starting,
        Active,
        Refused,
        Cancelled,
        Closed,
        Error
    };
    Q_ENUM(PortalState)

    explicit PortalManager(QObject* parent = nullptr);
    ~PortalManager() override;

    // InputInjector interface implementation (thread-safe for worker thread)
    bool sendButton(uint32_t button, bool pressed) override;
    bool sendMotion(double dx, double dy) override;

    [[nodiscard]] PortalState state() const;
    [[nodiscard]] QString stateString() const;
    [[nodiscard]] bool hasActiveSession() const;
    [[nodiscard]] bool areShortcutsActive() const;
    [[nodiscard]] QString toggleShortcutDescription() const;
    [[nodiscard]] QString stopShortcutDescription() const;

    static QString formatTriggerForXdg(const QString& trigger);

public slots:
    void initRemoteDesktopSession(const QString& savedRestoreToken = QString{});
    void initGlobalShortcuts(
        const QString& toggleTrigger = QStringLiteral("F6"),
        const QString& stopTrigger = QStringLiteral("F7")
    );
    void rebindShortcuts(
        const QString& toggleTrigger,
        const QString& stopTrigger
    );
    void configureShortcuts();
    void closeSessions();

signals:
    void stateChanged(PortalManager::PortalState newState, const QString& message);
    void shortcutsStateChanged(bool active, const QString& details);
    void effectiveShortcutsUpdated(const QString& toggleKey, const QString& stopKey);
    void toggleShortcutTriggered();
    void stopShortcutTriggered();
    void restoreTokenUpdated(const QString& newToken);
    void sessionClosed();

private slots:
    void handleRemoteDesktopResponse(uint responseCode, const QVariantMap& results);
    void handleGlobalShortcutsResponse(uint responseCode, const QVariantMap& results);
    void handleShortcutActivated(
        const QDBusObjectPath& sessionHandle,
        const QString& actionId,
        qulonglong timestamp,
        const QVariantMap& options
    );
    void handleRemoteDesktopSessionClosed();
    void handleShortcutsSessionClosed();

private:
    QString generateToken(const QString& prefix);
    QString expectedRequestPath(const QString& token) const;

    void requestSelectDevices();
    void requestStartRemoteDesktop();
    void requestBindShortcuts();

    mutable std::mutex m_sessionMutex;
    QDBusObjectPath m_remoteDesktopSessionPath;
    QDBusObjectPath m_shortcutsSessionPath;

    PortalState m_state{PortalState::Uninitialized};
    QString m_lastRestoreToken;
    bool m_retryWithoutToken{false};
    bool m_shortcutsActive{false};
    bool m_shortcutSignalConnected{false};
    QString m_requestedToggleTrigger{QStringLiteral("F6")};
    QString m_requestedStopTrigger{QStringLiteral("F7")};
    QString m_toggleShortcutDesc{QStringLiteral("Bascule (F6)")};
    QString m_stopShortcutDesc{QStringLiteral("Arrêt (F7)")};

    QString m_currentRdRequestToken;
    QString m_currentRdRequestPath;
    QString m_currentScRequestToken;
    QString m_currentScRequestPath;

    std::atomic<uint64_t> m_tokenCounter{1};
};
