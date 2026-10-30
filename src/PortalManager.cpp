#include "PortalManager.hpp"
#include <QCoreApplication>
#include <QDBusReply>
#include <QDebug>
#include <QKeySequence>
#include <QKeyCombination>
#include <QProcess>

QDBusArgument &operator<<(QDBusArgument &argument, const PortalShortcut &shortcut) {
    argument.beginStructure();
    argument << shortcut.id << shortcut.options;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalShortcut &shortcut) {
    argument.beginStructure();
    argument >> shortcut.id >> shortcut.options;
    argument.endStructure();
    return argument;
}

PortalManager::PortalManager(QObject* parent)
    : QObject(parent) {
    qDBusRegisterMetaType<PortalShortcut>();
    qDBusRegisterMetaType<QList<PortalShortcut>>();
}

PortalManager::~PortalManager() {
    closeSessions();
}

PortalManager::PortalState PortalManager::state() const {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    return m_state;
}

QString PortalManager::stateString() const {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    switch (m_state) {
        case PortalState::Uninitialized: return QStringLiteral("Non initialisé");
        case PortalState::CreatingSession: return QStringLiteral("Création de session...");
        case PortalState::SelectingDevices: return QStringLiteral("Sélection des périphériques...");
        case PortalState::Starting: return QStringLiteral("Démarrage de session...");
        case PortalState::Active: return QStringLiteral("Session active");
        case PortalState::Refused: return QStringLiteral("Autorisation refusée");
        case PortalState::Cancelled: return QStringLiteral("Annulé par l'utilisateur");
        case PortalState::Closed: return QStringLiteral("Session fermée");
        case PortalState::Error: return QStringLiteral("Erreur de portail");
    }
    return QStringLiteral("Inconnu");
}

bool PortalManager::hasActiveSession() const {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    return m_state == PortalState::Active && !m_remoteDesktopSessionPath.path().isEmpty();
}

bool PortalManager::areShortcutsActive() const {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    return m_shortcutsActive;
}

QString PortalManager::toggleShortcutDescription() const {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    return m_toggleShortcutDesc;
}

QString PortalManager::stopShortcutDescription() const {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    return m_stopShortcutDesc;
}

QString PortalManager::generateToken(const QString& prefix) {
    const auto pid = QCoreApplication::applicationPid();
    const auto counter = m_tokenCounter.fetch_add(1);
    return QStringLiteral("%1_%2_%3").arg(prefix).arg(pid).arg(counter);
}

QString PortalManager::expectedRequestPath(const QString& token) const {
    QString sender = QDBusConnection::sessionBus().baseService();
    if (sender.startsWith(QLatin1Char(':'))) {
        sender.remove(0, 1);
    }
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    return QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
}

void PortalManager::initRemoteDesktopSession(const QString& savedRestoreToken) {
    m_lastRestoreToken = savedRestoreToken;

    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        m_state = PortalState::CreatingSession;
    }
    emit stateChanged(PortalState::CreatingSession, QStringLiteral("Connexion au portail RemoteDesktop..."));

    m_currentRdRequestToken = generateToken(QStringLiteral("u_rd_cs"));
    m_currentRdRequestPath = expectedRequestPath(m_currentRdRequestToken);

    // Subscribe to Request.Response BEFORE sending method call to avoid race condition
    QDBusConnection::sessionBus().connect(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        m_currentRdRequestPath,
        QStringLiteral("org.freedesktop.portal.Request"),
        QStringLiteral("Response"),
        this,
        SLOT(handleRemoteDesktopResponse(uint, QVariantMap))
    );

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), m_currentRdRequestToken);
    options.insert(QStringLiteral("session_handle_token"), generateToken(QStringLiteral("s_rd")));

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.RemoteDesktop"),
        QStringLiteral("CreateSession")
    );
    msg << options;

    QDBusMessage reply = QDBusConnection::sessionBus().call(msg);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        {
            std::lock_guard<std::mutex> lock(m_sessionMutex);
            m_state = PortalState::Error;
        }
        emit stateChanged(PortalState::Error, reply.errorMessage());
    }
}

void PortalManager::requestSelectDevices() {
    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        m_state = PortalState::SelectingDevices;
    }
    emit stateChanged(PortalState::SelectingDevices, QStringLiteral("Sélection du pointeur..."));

    m_currentRdRequestToken = generateToken(QStringLiteral("u_rd_sd"));
    m_currentRdRequestPath = expectedRequestPath(m_currentRdRequestToken);

    QDBusConnection::sessionBus().connect(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        m_currentRdRequestPath,
        QStringLiteral("org.freedesktop.portal.Request"),
        QStringLiteral("Response"),
        this,
        SLOT(handleRemoteDesktopResponse(uint, QVariantMap))
    );

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), m_currentRdRequestToken);
    options.insert(QStringLiteral("types"), 2u);        // Pointer only
    options.insert(QStringLiteral("persist_mode"), 2u); // Persistent permissions

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.RemoteDesktop"),
        QStringLiteral("SelectDevices")
    );

    QDBusObjectPath sessionPath;
    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        sessionPath = m_remoteDesktopSessionPath;
    }

    msg << QVariant::fromValue(sessionPath) << options;

    QDBusMessage reply = QDBusConnection::sessionBus().call(msg);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        {
            std::lock_guard<std::mutex> lock(m_sessionMutex);
            m_state = PortalState::Error;
        }
        emit stateChanged(PortalState::Error, reply.errorMessage());
    }
}

void PortalManager::requestStartRemoteDesktop() {
    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        m_state = PortalState::Starting;
    }
    emit stateChanged(PortalState::Starting, QStringLiteral("Démarrage de la session..."));

    m_currentRdRequestToken = generateToken(QStringLiteral("u_rd_st"));
    m_currentRdRequestPath = expectedRequestPath(m_currentRdRequestToken);

    QDBusConnection::sessionBus().connect(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        m_currentRdRequestPath,
        QStringLiteral("org.freedesktop.portal.Request"),
        QStringLiteral("Response"),
        this,
        SLOT(handleRemoteDesktopResponse(uint, QVariantMap))
    );

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), m_currentRdRequestToken);
    if (!m_retryWithoutToken && !m_lastRestoreToken.isEmpty()) {
        options.insert(QStringLiteral("restore_token"), m_lastRestoreToken);
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.RemoteDesktop"),
        QStringLiteral("Start")
    );

    QDBusObjectPath sessionPath;
    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        sessionPath = m_remoteDesktopSessionPath;
    }

    msg << QVariant::fromValue(sessionPath)
        << QStringLiteral("") // Parent window id
        << options;

    QDBusMessage reply = QDBusConnection::sessionBus().call(msg);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        {
            std::lock_guard<std::mutex> lock(m_sessionMutex);
            m_state = PortalState::Error;
        }
        emit stateChanged(PortalState::Error, reply.errorMessage());
    }
}

void PortalManager::handleRemoteDesktopResponse(uint responseCode, const QVariantMap& results) {
    // Disconnect listener for this request
    QDBusConnection::sessionBus().disconnect(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        m_currentRdRequestPath,
        QStringLiteral("org.freedesktop.portal.Request"),
        QStringLiteral("Response"),
        this,
        SLOT(handleRemoteDesktopResponse(uint, QVariantMap))
    );

    if (responseCode == 1) { // User Cancelled
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        m_state = PortalState::Cancelled;
        emit stateChanged(PortalState::Cancelled, QStringLiteral("Autorisation annulée par l'utilisateur"));
        return;
    }

    if (responseCode != 0) { // Failure or Token Expired
        if (!m_retryWithoutToken && !m_lastRestoreToken.isEmpty()) {
            // Restore token was rejected. Restart fresh authorization once
            m_retryWithoutToken = true;
            m_lastRestoreToken.clear();
            emit restoreTokenUpdated(QString{});
            initRemoteDesktopSession();
            return;
        }

        std::lock_guard<std::mutex> lock(m_sessionMutex);
        m_state = PortalState::Refused;
        emit stateChanged(PortalState::Refused, QStringLiteral("Autorisation refusée"));
        return;
    }

    PortalState currentState;
    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        currentState = m_state;
    }

    if (currentState == PortalState::CreatingSession) {
        if (results.contains(QStringLiteral("session_handle"))) {
            QString pathStr = results.value(QStringLiteral("session_handle")).toString();
            {
                std::lock_guard<std::mutex> lock(m_sessionMutex);
                m_remoteDesktopSessionPath = QDBusObjectPath(pathStr);
            }

            // Listen for Session.Closed signal on this session
            QDBusConnection::sessionBus().connect(
                QStringLiteral("org.freedesktop.portal.Desktop"),
                pathStr,
                QStringLiteral("org.freedesktop.portal.Session"),
                QStringLiteral("Closed"),
                this,
                SLOT(handleRemoteDesktopSessionClosed())
            );

            requestSelectDevices();
        } else {
            std::lock_guard<std::mutex> lock(m_sessionMutex);
            m_state = PortalState::Error;
            emit stateChanged(PortalState::Error, QStringLiteral("Session handle manquant"));
        }
    } else if (currentState == PortalState::SelectingDevices) {
        requestStartRemoteDesktop();
    } else if (currentState == PortalState::Starting) {
        // Validate granted devices
        uint devicesGranted = results.value(QStringLiteral("devices")).toUInt();
        if ((devicesGranted & 2u) == 0) {
            std::lock_guard<std::mutex> lock(m_sessionMutex);
            m_state = PortalState::Refused;
            emit stateChanged(PortalState::Refused, QStringLiteral("Permission pointeur non accordée"));
            return;
        }

        // Single-use restore_token: rewrite immediately!
        if (results.contains(QStringLiteral("restore_token"))) {
            QString newToken = results.value(QStringLiteral("restore_token")).toString();
            m_lastRestoreToken = newToken;
            emit restoreTokenUpdated(newToken);
        }

        {
            std::lock_guard<std::mutex> lock(m_sessionMutex);
            m_state = PortalState::Active;
        }
        emit stateChanged(PortalState::Active, QStringLiteral("Session active et prête"));
    }
}

void PortalManager::handleRemoteDesktopSessionClosed() {
    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        m_state = PortalState::Closed;
        m_remoteDesktopSessionPath = QDBusObjectPath{};
    }
    emit sessionClosed();
    emit stateChanged(PortalState::Closed, QStringLiteral("Session fermée par le compositeur Wayland"));
}

void PortalManager::initGlobalShortcuts(const QString& toggleTrigger, const QString& stopTrigger) {
    if (!toggleTrigger.isEmpty()) m_requestedToggleTrigger = toggleTrigger;
    if (!stopTrigger.isEmpty()) m_requestedStopTrigger = stopTrigger;

    m_currentScRequestToken = generateToken(QStringLiteral("u_sc_cs"));
    m_currentScRequestPath = expectedRequestPath(m_currentScRequestToken);

    QDBusConnection::sessionBus().connect(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        m_currentScRequestPath,
        QStringLiteral("org.freedesktop.portal.Request"),
        QStringLiteral("Response"),
        this,
        SLOT(handleGlobalShortcutsResponse(uint, QVariantMap))
    );

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), m_currentScRequestToken);
    options.insert(QStringLiteral("session_handle_token"), generateToken(QStringLiteral("s_sc")));

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.GlobalShortcuts"),
        QStringLiteral("CreateSession")
    );
    msg << options;

    QDBusMessage reply = QDBusConnection::sessionBus().call(msg);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        emit shortcutsStateChanged(false, QStringLiteral("Erreur de création session raccourcis"));
    }
}

QString PortalManager::formatTriggerForXdg(const QString& trigger) {
    if (trigger.isEmpty()) return QStringLiteral("F6");
    QKeySequence seq(trigger);
    if (seq.isEmpty()) return trigger;

    QKeyCombination comb = seq[0];
    auto mods = comb.keyboardModifiers();
    auto key = comb.key();

    QStringList parts;
    if (mods.testFlag(Qt::ControlModifier)) parts << QStringLiteral("CTRL");
    if (mods.testFlag(Qt::AltModifier)) parts << QStringLiteral("ALT");
    if (mods.testFlag(Qt::ShiftModifier)) parts << QStringLiteral("SHIFT");
    if (mods.testFlag(Qt::MetaModifier)) parts << QStringLiteral("LOGO");

    QString keyName;
    if (key >= Qt::Key_F1 && key <= Qt::Key_F35) {
        keyName = QStringLiteral("F%1").arg(key - Qt::Key_F1 + 1);
    } else if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        keyName = QChar('a' + (key - Qt::Key_A));
    } else if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        keyName = QChar('0' + (key - Qt::Key_0));
    } else {
        switch (key) {
            case Qt::Key_Space: keyName = QStringLiteral("space"); break;
            case Qt::Key_Tab: keyName = QStringLiteral("Tab"); break;
            case Qt::Key_Return:
            case Qt::Key_Enter: keyName = QStringLiteral("Return"); break;
            case Qt::Key_Escape: keyName = QStringLiteral("Escape"); break;
            case Qt::Key_Backspace: keyName = QStringLiteral("BackSpace"); break;
            case Qt::Key_Delete: keyName = QStringLiteral("Delete"); break;
            case Qt::Key_Insert: keyName = QStringLiteral("Insert"); break;
            case Qt::Key_Home: keyName = QStringLiteral("Home"); break;
            case Qt::Key_End: keyName = QStringLiteral("End"); break;
            case Qt::Key_PageUp: keyName = QStringLiteral("Page_Up"); break;
            case Qt::Key_PageDown: keyName = QStringLiteral("Page_Down"); break;
            case Qt::Key_Pause: keyName = QStringLiteral("Pause"); break;
            case Qt::Key_Print: keyName = QStringLiteral("Print"); break;
            default:
                return trigger;
        }
    }

    parts << keyName;
    return parts.join(QLatin1Char('+'));
}

void PortalManager::rebindShortcuts(const QString& toggleTrigger, const QString& stopTrigger) {
    if (!toggleTrigger.isEmpty()) m_requestedToggleTrigger = toggleTrigger;
    if (!stopTrigger.isEmpty()) m_requestedStopTrigger = stopTrigger;

    // 1. Unregister previously bound actions in KGlobalAccel to clear any stale Plasma state
    QDBusMessage unregToggle = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.kglobalaccel"),
        QStringLiteral("/kglobalaccel"),
        QStringLiteral("org.kde.KGlobalAccel"),
        QStringLiteral("unregister")
    );
    unregToggle << QStringLiteral("fr.perso.Autoclicker") << QStringLiteral("toggle");
    QDBusConnection::sessionBus().call(unregToggle);

    QDBusMessage unregStop = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.kglobalaccel"),
        QStringLiteral("/kglobalaccel"),
        QStringLiteral("org.kde.KGlobalAccel"),
        QStringLiteral("unregister")
    );
    unregStop << QStringLiteral("fr.perso.Autoclicker") << QStringLiteral("stop");
    QDBusConnection::sessionBus().call(unregStop);

    // 2. Close current shortcuts portal session if active
    QDBusObjectPath oldSession;
    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        oldSession = m_shortcutsSessionPath;
        m_shortcutsSessionPath = QDBusObjectPath{};
        m_shortcutsActive = false;
    }

    if (!oldSession.path().isEmpty()) {
        QDBusMessage closeMsg = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.portal.Desktop"),
            oldSession.path(),
            QStringLiteral("org.freedesktop.portal.Session"),
            QStringLiteral("Close")
        );
        QDBusConnection::sessionBus().call(closeMsg);
    }

    // 3. Create fresh portal session and bind with updated triggers
    initGlobalShortcuts(m_requestedToggleTrigger, m_requestedStopTrigger);
}

void PortalManager::configureShortcuts() {
    QDBusObjectPath scSession;
    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        scSession = m_shortcutsSessionPath;
    }

    if (!scSession.path().isEmpty()) {
        QVariantMap options;
        QDBusMessage msg = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.portal.Desktop"),
            QStringLiteral("/org/freedesktop/portal/desktop"),
            QStringLiteral("org.freedesktop.portal.GlobalShortcuts"),
            QStringLiteral("ConfigureShortcuts")
        );
        msg << QVariant::fromValue(scSession)
            << QStringLiteral("") // Parent window
            << options;
        QDBusMessage reply = QDBusConnection::sessionBus().call(msg);
        if (reply.type() != QDBusMessage::ErrorMessage) {
            return;
        }
    }

    // Fallback: directly launch KDE Shortcuts settings
    QProcess::startDetached(QStringLiteral("kcmshell6"), {QStringLiteral("kcm_keys")});
}

void PortalManager::requestBindShortcuts() {
    m_currentScRequestToken = generateToken(QStringLiteral("u_sc_bs"));
    m_currentScRequestPath = expectedRequestPath(m_currentScRequestToken);

    QDBusConnection::sessionBus().connect(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        m_currentScRequestPath,
        QStringLiteral("org.freedesktop.portal.Request"),
        QStringLiteral("Response"),
        this,
        SLOT(handleGlobalShortcutsResponse(uint, QVariantMap))
    );

    QList<PortalShortcut> shortcutsList;
    shortcutsList.append(PortalShortcut{
        QStringLiteral("toggle"),
        QVariantMap{
            {QStringLiteral("description"), QStringLiteral("Démarrer / Arrêter l'autoclicker")},
            {QStringLiteral("preferred_trigger"), formatTriggerForXdg(m_requestedToggleTrigger)}
        }
    });
    shortcutsList.append(PortalShortcut{
        QStringLiteral("stop"),
        QVariantMap{
            {QStringLiteral("description"), QStringLiteral("Arrêt immédiat de l'autoclicker")},
            {QStringLiteral("preferred_trigger"), formatTriggerForXdg(m_requestedStopTrigger)}
        }
    });

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), m_currentScRequestToken);

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.GlobalShortcuts"),
        QStringLiteral("BindShortcuts")
    );

    QDBusObjectPath scSession;
    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        scSession = m_shortcutsSessionPath;
    }

    msg << QVariant::fromValue(scSession)
        << QVariant::fromValue(shortcutsList)
        << QStringLiteral("") // Parent window
        << options;

    QDBusMessage reply = QDBusConnection::sessionBus().call(msg);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "BindShortcuts call error:" << reply.errorMessage();
        emit shortcutsStateChanged(false, QStringLiteral("Erreur: %1").arg(reply.errorMessage()));
    }
}

void PortalManager::handleGlobalShortcutsResponse(uint responseCode, const QVariantMap& results) {
    QDBusConnection::sessionBus().disconnect(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        m_currentScRequestPath,
        QStringLiteral("org.freedesktop.portal.Request"),
        QStringLiteral("Response"),
        this,
        SLOT(handleGlobalShortcutsResponse(uint, QVariantMap))
    );

    if (responseCode != 0) {
        emit shortcutsStateChanged(false, QStringLiteral("Raccourcis non accordés ou refusés"));
        return;
    }

    if (results.contains(QStringLiteral("session_handle"))) {
        QString pathStr = results.value(QStringLiteral("session_handle")).toString();
        {
            std::lock_guard<std::mutex> lock(m_sessionMutex);
            m_shortcutsSessionPath = QDBusObjectPath(pathStr);
        }

        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.freedesktop.portal.Desktop"),
            pathStr,
            QStringLiteral("org.freedesktop.portal.Session"),
            QStringLiteral("Closed"),
            this,
            SLOT(handleShortcutsSessionClosed())
        );

        requestBindShortcuts();
    } else if (results.contains(QStringLiteral("shortcuts"))) {
        QString toggleEffective = m_requestedToggleTrigger;
        QString stopEffective = m_requestedStopTrigger;

        // Try to read actual assigned triggers from results
        QVariant scVal = results.value(QStringLiteral("shortcuts"));
        if (scVal.canConvert<QDBusArgument>()) {
            QDBusArgument arg = scVal.value<QDBusArgument>();
            QList<PortalShortcut> boundShortcuts;
            arg >> boundShortcuts;
            for (const auto& s : boundShortcuts) {
                QString trigger = s.options.value(QStringLiteral("trigger_description")).toString();
                if (!trigger.isEmpty()) {
                    if (s.id == QStringLiteral("toggle")) toggleEffective = trigger;
                    else if (s.id == QStringLiteral("stop")) stopEffective = trigger;
                }
            }
        }

        bool toggleValid = (toggleEffective != QStringLiteral("none") && !toggleEffective.isEmpty());
        bool stopValid = (stopEffective != QStringLiteral("none") && !stopEffective.isEmpty());

        {
            std::lock_guard<std::mutex> lock(m_sessionMutex);
            m_shortcutsActive = (toggleValid || stopValid);
            m_toggleShortcutDesc = toggleEffective;
            m_stopShortcutDesc = stopEffective;
        }

        // Connect to Activated signal once to prevent duplicate triggers across rebinds
        if (!m_shortcutSignalConnected) {
            bool ok = QDBusConnection::sessionBus().connect(
                QStringLiteral("org.freedesktop.portal.Desktop"),
                QStringLiteral("/org/freedesktop/portal/desktop"),
                QStringLiteral("org.freedesktop.portal.GlobalShortcuts"),
                QStringLiteral("Activated"),
                this,
                SLOT(handleShortcutActivated(QDBusObjectPath, QString, qulonglong, QVariantMap))
            );
            if (ok) {
                m_shortcutSignalConnected = true;
            }
        }

        emit effectiveShortcutsUpdated(m_toggleShortcutDesc, m_stopShortcutDesc);
        if (toggleValid && stopValid) {
            emit shortcutsStateChanged(
                true,
                QStringLiteral("Raccourcis globaux actifs (%1 / %2)").arg(m_toggleShortcutDesc, m_stopShortcutDesc)
            );
        } else if (toggleValid || stopValid) {
            emit shortcutsStateChanged(
                true,
                QStringLiteral("Raccourcis partiels (%1 / %2)").arg(m_toggleShortcutDesc, m_stopShortcutDesc)
            );
        } else {
            emit shortcutsStateChanged(
                false,
                QStringLiteral("Aucun raccourci actif (définis sur 'none')")
            );
        }
    }
}

void PortalManager::handleShortcutActivated(
    const QDBusObjectPath& sessionHandle,
    const QString& actionId,
    qulonglong timestamp,
    const QVariantMap& options
) {
    Q_UNUSED(sessionHandle);
    Q_UNUSED(timestamp);
    Q_UNUSED(options);

    if (actionId == QStringLiteral("toggle")) {
        emit toggleShortcutTriggered();
    } else if (actionId == QStringLiteral("stop")) {
        emit stopShortcutTriggered();
    }
}

void PortalManager::handleShortcutsSessionClosed() {
    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        m_shortcutsActive = false;
        m_shortcutsSessionPath = QDBusObjectPath{};
    }
    emit shortcutsStateChanged(false, QStringLiteral("Session raccourcis fermée"));
}

bool PortalManager::sendButton(uint32_t button, bool pressed) {
    QDBusObjectPath sessionPath;
    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        if (m_state != PortalState::Active || m_remoteDesktopSessionPath.path().isEmpty()) {
            return false;
        }
        sessionPath = m_remoteDesktopSessionPath;
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.RemoteDesktop"),
        QStringLiteral("NotifyPointerButton")
    );

    msg << QVariant::fromValue(sessionPath)
        << QVariant::fromValue(QVariantMap{})
        << static_cast<int32_t>(button)
        << static_cast<uint32_t>(pressed ? 1u : 0u);

    return QDBusConnection::sessionBus().send(msg);
}

bool PortalManager::sendMotion(double dx, double dy) {
    QDBusObjectPath sessionPath;
    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        if (m_state != PortalState::Active || m_remoteDesktopSessionPath.path().isEmpty()) {
            return false;
        }
        sessionPath = m_remoteDesktopSessionPath;
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.RemoteDesktop"),
        QStringLiteral("NotifyPointerMotionRelative")
    );

    msg << QVariant::fromValue(sessionPath)
        << QVariant::fromValue(QVariantMap{})
        << dx
        << dy;

    return QDBusConnection::sessionBus().send(msg);
}

void PortalManager::closeSessions() {
    QDBusObjectPath rdPath;
    QDBusObjectPath scPath;
    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        rdPath = m_remoteDesktopSessionPath;
        scPath = m_shortcutsSessionPath;
        m_state = PortalState::Closed;
        m_shortcutsActive = false;
        m_remoteDesktopSessionPath = QDBusObjectPath{};
        m_shortcutsSessionPath = QDBusObjectPath{};
    }

    if (!rdPath.path().isEmpty()) {
        QDBusMessage closeMsg = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.portal.Desktop"),
            rdPath.path(),
            QStringLiteral("org.freedesktop.portal.Session"),
            QStringLiteral("Close")
        );
        QDBusConnection::sessionBus().call(closeMsg);
    }

    if (!scPath.path().isEmpty()) {
        QDBusMessage closeMsg = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.portal.Desktop"),
            scPath.path(),
            QStringLiteral("org.freedesktop.portal.Session"),
            QStringLiteral("Close")
        );
        QDBusConnection::sessionBus().call(closeMsg);
    }
}
