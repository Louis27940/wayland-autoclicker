#include <QApplication>
#include <QSocketNotifier>
#include <csignal>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include "ClickController.hpp"
#include "PortalManager.hpp"
#include "Settings.hpp"
#include "TrayController.hpp"
#include "MainWindow.hpp"

namespace {
int g_signalSocketPair[2] = {-1, -1};

void posixSignalHandler(int sig) {
    const int savedErrno = errno;
    char sigByte = static_cast<char>(sig);
    // Write 1 byte to the non-blocking socket pair (async-signal-safe)
    [[maybe_unused]] const auto bytesWritten = ::write(g_signalSocketPair[0], &sigByte, sizeof(sigByte));
    errno = savedErrno;
}

void setupSignalHandlers() {
    if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0, g_signalSocketPair) != 0) {
        return;
    }

    struct sigaction sa{};
    sa.sa_handler = posixSignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    ::sigaction(SIGINT, &sa, nullptr);
    ::sigaction(SIGTERM, &sa, nullptr);
    ::sigaction(SIGHUP, &sa, nullptr);
}

void cleanupSignalHandlers() {
    if (g_signalSocketPair[0] != -1) {
        ::close(g_signalSocketPair[0]);
        g_signalSocketPair[0] = -1;
    }
    if (g_signalSocketPair[1] != -1) {
        ::close(g_signalSocketPair[1]);
        g_signalSocketPair[1] = -1;
    }
}
} // namespace

int main(int argc, char* argv[]) {
    // Application Identity for Wayland portals and KDE Plasma (DNS reverse format)
    QGuiApplication::setDesktopFileName(QStringLiteral("fr.perso.Autoclicker"));
    QCoreApplication::setApplicationName(QStringLiteral("WaylandAutoclicker"));
    QCoreApplication::setOrganizationName(QStringLiteral("Perso"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("perso.fr"));

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    setupSignalHandlers();

    // Components initialization
    Settings settings;
    PortalManager portalManager;
    ClickController clickController(&portalManager);
    TrayController trayController;

    MainWindow mainWindow(&portalManager, &clickController, &settings, &trayController);
    mainWindow.show();
    trayController.show();

    // Setup Qt SocketNotifier to receive signal bytes safely in the main Qt event loop
    QSocketNotifier signalNotifier(g_signalSocketPair[1], QSocketNotifier::Read);
    QObject::connect(&signalNotifier, &QSocketNotifier::activated, [&]() {
        signalNotifier.setEnabled(false);
        char sigByte = 0;
        [[maybe_unused]] const auto bytesRead = ::read(g_signalSocketPair[1], &sigByte, sizeof(sigByte));

        // Defined shutdown sequence (§8, rule 5)
        // 1. Request stop of the worker and join it
        clickController.stop(ClickController::StopReason::SignalTerminated);

        // 2. Allow pending D-Bus messages to flush
        QDBusConnection::sessionBus().interface();

        // 3. Explicitly close portal sessions
        portalManager.closeSessions();

        // 4. Save settings
        settings.sync();

        // 5. Quit application cleanly
        app.quit();
    });

    // Handle normal application quit
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        clickController.stop(ClickController::StopReason::Manual);
        portalManager.closeSessions();
        settings.sync();
        cleanupSignalHandlers();
    });

    return app.exec();
}
