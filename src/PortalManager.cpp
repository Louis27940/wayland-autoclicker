#include "PortalManager.hpp"
#include <QCoreApplication>
#include <QDBusReply>
#include <QDebug>

PortalManager::PortalManager(QObject* parent)
    : QObject(parent) {}

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

void PortalManager::initGlobalShortcuts() {
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

    // Prepare shortcuts descriptions
    struct ShortcutDef {
        QString id;
        QString description;
        QString preferredTrigger;
    };
    QList<ShortcutDef> shortcutDefs = {
        {QStringLiteral("toggle"), QStringLiteral("Démarrer / Arrêter l'autoclicker"), QStringLiteral("F8")},
        {QStringLiteral("stop"), QStringLiteral("Arrêt immédiat de l'autoclicker"), QStringLiteral("F9")}
    };

    QVariantList shortcutsList;
    for (const auto& s : shortcutDefs) {
        QVariantMap sOpts;
        sOpts.insert(QStringLiteral("description"), s.description);
        sOpts.insert(QStringLiteral("preferred_trigger"), s.preferredTrigger);
        shortcutsList.append(QVariant::fromValue(QVariantList{s.id, sOpts}));
    }

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

    QDBusConnection::sessionBus().call(msg);
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
        {
            std::lock_guard<std::mutex> lock(m_sessionMutex);
            m_shortcutsActive = true;
        }

        // Connect to Activated signal
        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.freedesktop.portal.Desktop"),
            QStringLiteral("/org/freedesktop/portal/desktop"),
            QStringLiteral("org.freedesktop.portal.GlobalShortcuts"),
            QStringLiteral("Activated"),
            this,
            SLOT(handleShortcutActivated(QDBusObjectPath, QString, qulonglong, QVariantMap))
        );

        emit shortcutsStateChanged(true, QStringLiteral("Raccourcis globaux actifs (F8 / F9)"));
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
