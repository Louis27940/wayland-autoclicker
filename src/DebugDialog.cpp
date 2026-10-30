#include "DebugDialog.hpp"
#include "PortalManager.hpp"
#include "ClickController.hpp"
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QTextStream>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QScrollArea>
#include <QGuiApplication>
#include <QLocale>
#include <unistd.h>

DebugDialog::DebugDialog(
    const PortalManager* portalManager,
    const ClickController* clickController,
    QWidget* parent
)
    : QDialog(parent)
    , m_portalManager(portalManager)
    , m_clickController(clickController)
{
    setWindowTitle(QStringLiteral("Debug Information"));
    setMinimumSize(480, 520);
    resize(520, 560);

    m_uptimeTimer.start();

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(14, 14, 14, 14);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* scrollContent = new QWidget(scrollArea);
    auto* contentLayout = new QVBoxLayout(scrollContent);
    contentLayout->setSpacing(12);
    contentLayout->setContentsMargins(0, 0, 0, 0);

    // 1. Group: Process & Memory
    auto* memGroup = new QGroupBox(QStringLiteral("Process & Memory"), scrollContent);
    auto* memForm = new QFormLayout(memGroup);
    memForm->setSpacing(6);

    m_pidLabel = new QLabel(scrollContent);
    m_uptimeLabel = new QLabel(scrollContent);
    m_ramUsageLabel = new QLabel(scrollContent);
    m_ramPeakLabel = new QLabel(scrollContent);
    m_vmSizeLabel = new QLabel(scrollContent);
    m_threadsLabel = new QLabel(scrollContent);
    m_sessionTypeLabel = new QLabel(scrollContent);
    m_compositorLabel = new QLabel(scrollContent);

    memForm->addRow(QStringLiteral("PID:"), m_pidLabel);
    memForm->addRow(QStringLiteral("Uptime:"), m_uptimeLabel);
    memForm->addRow(QStringLiteral("RAM (RSS):"), m_ramUsageLabel);
    memForm->addRow(QStringLiteral("Peak RAM (HWM):"), m_ramPeakLabel);
    memForm->addRow(QStringLiteral("Virtual Memory:"), m_vmSizeLabel);
    memForm->addRow(QStringLiteral("Threads:"), m_threadsLabel);
    memForm->addRow(QStringLiteral("Session Type:"), m_sessionTypeLabel);
    memForm->addRow(QStringLiteral("Compositor / Desktop:"), m_compositorLabel);

    contentLayout->addWidget(memGroup);

    // 2. Group: Wayland Portals
    auto* portalGroup = new QGroupBox(QStringLiteral("XDG Desktop Portals"), scrollContent);
    auto* portalForm = new QFormLayout(portalGroup);
    portalForm->setSpacing(6);

    m_rdStateLabel = new QLabel(scrollContent);
    m_rdSessionLabel = new QLabel(scrollContent);
    m_rdTokenLabel = new QLabel(scrollContent);
    m_scStateLabel = new QLabel(scrollContent);
    m_scSessionLabel = new QLabel(scrollContent);
    m_scTriggersLabel = new QLabel(scrollContent);

    m_rdSessionLabel->setWordWrap(true);
    m_scSessionLabel->setWordWrap(true);

    portalForm->addRow(QStringLiteral("RemoteDesktop State:"), m_rdStateLabel);
    portalForm->addRow(QStringLiteral("RemoteDesktop Session:"), m_rdSessionLabel);
    portalForm->addRow(QStringLiteral("Restore Token:"), m_rdTokenLabel);
    portalForm->addRow(QStringLiteral("Shortcuts State:"), m_scStateLabel);
    portalForm->addRow(QStringLiteral("Shortcuts Session:"), m_scSessionLabel);
    portalForm->addRow(QStringLiteral("Bound Triggers:"), m_scTriggersLabel);

    contentLayout->addWidget(portalGroup);

    // 3. Group: Engine & Performance
    auto* engineGroup = new QGroupBox(QStringLiteral("Engine & Statistics"), scrollContent);
    auto* engineForm = new QFormLayout(engineGroup);
    engineForm->setSpacing(6);

    m_engineStateLabel = new QLabel(scrollContent);
    m_actionsCountLabel = new QLabel(scrollContent);
    m_modeLabel = new QLabel(scrollContent);
    m_intervalLabel = new QLabel(scrollContent);
    m_mouseButtonLabel = new QLabel(scrollContent);
    m_safetyLimitsLabel = new QLabel(scrollContent);

    engineForm->addRow(QStringLiteral("Engine Status:"), m_engineStateLabel);
    engineForm->addRow(QStringLiteral("Total Injected Actions:"), m_actionsCountLabel);
    engineForm->addRow(QStringLiteral("Active Mode:"), m_modeLabel);
    engineForm->addRow(QStringLiteral("Interval / Jitter:"), m_intervalLabel);
    engineForm->addRow(QStringLiteral("Configured Button:"), m_mouseButtonLabel);
    engineForm->addRow(QStringLiteral("Safety Settings:"), m_safetyLimitsLabel);

    contentLayout->addWidget(engineGroup);
    contentLayout->addStretch();

    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea);

    // Buttons layout
    auto* buttonLayout = new QHBoxLayout();
    m_refreshButton = new QPushButton(QStringLiteral("Refresh"), this);
    m_copyButton = new QPushButton(QStringLiteral("Copy to Clipboard"), this);
    m_closeButton = new QPushButton(QStringLiteral("Close"), this);

    buttonLayout->addWidget(m_refreshButton);
    buttonLayout->addWidget(m_copyButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_closeButton);
    mainLayout->addLayout(buttonLayout);

    connect(m_refreshButton, &QPushButton::clicked, this, &DebugDialog::refreshInfo);
    connect(m_copyButton, &QPushButton::clicked, this, &DebugDialog::copyToClipboard);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(1000);
    connect(m_refreshTimer, &QTimer::timeout, this, &DebugDialog::refreshInfo);

    refreshInfo();
}

void DebugDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    refreshInfo();
    m_refreshTimer->start();
}

void DebugDialog::hideEvent(QHideEvent* event) {
    m_refreshTimer->stop();
    QDialog::hideEvent(event);
}

DebugDialog::MemoryInfo DebugDialog::readProcessMemory() {
    MemoryInfo info;
    QFile file(QStringLiteral("/proc/self/status"));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.startsWith(QLatin1String("VmRSS:"))) {
                QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                if (parts.size() >= 2) info.vmRssKb = parts[1].toLongLong();
            } else if (line.startsWith(QLatin1String("VmHWM:"))) {
                QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                if (parts.size() >= 2) info.vmHwmKb = parts[1].toLongLong();
            } else if (line.startsWith(QLatin1String("VmSize:"))) {
                QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                if (parts.size() >= 2) info.vmSizeKb = parts[1].toLongLong();
            } else if (line.startsWith(QLatin1String("VmPeak:"))) {
                QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                if (parts.size() >= 2) info.vmPeakKb = parts[1].toLongLong();
            } else if (line.startsWith(QLatin1String("Threads:"))) {
                QStringList parts = line.split(QLatin1Char('\t'), Qt::SkipEmptyParts);
                if (parts.size() >= 2) info.threads = parts[1].trimmed().toInt();
            }
        }
    }
    return info;
}

QString DebugDialog::formatKb(qint64 kb) {
    if (kb <= 0) return QStringLiteral("N/A");
    if (kb >= 1024) {
        double mb = static_cast<double>(kb) / 1024.0;
        return QStringLiteral("%1 MB (%2 kB)").arg(
            QString::number(mb, 'f', 2),
            QLocale().toString(kb)
        );
    }
    return QStringLiteral("%1 kB").arg(QLocale().toString(kb));
}

void DebugDialog::refreshInfo() {
    // 1. Process & Memory
    MemoryInfo mem = readProcessMemory();
    m_pidLabel->setText(QString::number(getpid()));

    qint64 elapsedSec = m_uptimeTimer.elapsed() / 1000;
    qint64 h = elapsedSec / 3600;
    qint64 m = (elapsedSec % 3600) / 60;
    qint64 s = elapsedSec % 60;
    m_uptimeLabel->setText(QStringLiteral("%1h %2m %3s")
        .arg(h, 2, 10, QLatin1Char('0'))
        .arg(m, 2, 10, QLatin1Char('0'))
        .arg(s, 2, 10, QLatin1Char('0')));

    m_ramUsageLabel->setText(formatKb(mem.vmRssKb));
    m_ramPeakLabel->setText(formatKb(mem.vmHwmKb));
    m_vmSizeLabel->setText(formatKb(mem.vmSizeKb));
    m_threadsLabel->setText(QString::number(mem.threads));

    QString sessionType = QString::fromLocal8Bit(qgetenv("XDG_SESSION_TYPE"));
    QString platform = QGuiApplication::platformName();
    m_sessionTypeLabel->setText(QStringLiteral("%1 (Qt platform: %2)").arg(
        sessionType.isEmpty() ? QStringLiteral("Unknown") : sessionType,
        platform
    ));

    QString desktop = QString::fromLocal8Bit(qgetenv("XDG_CURRENT_DESKTOP"));
    QString waylandDisplay = QString::fromLocal8Bit(qgetenv("WAYLAND_DISPLAY"));
    QString comp = desktop;
    if (!waylandDisplay.isEmpty()) {
        comp += QStringLiteral(" (Display: %1)").arg(waylandDisplay);
    }
    m_compositorLabel->setText(comp.isEmpty() ? QStringLiteral("Unknown") : comp);

    // 2. Portals
    if (m_portalManager) {
        m_rdStateLabel->setText(m_portalManager->stateString());
        QString rdSession = m_portalManager->remoteDesktopSessionPath();
        m_rdSessionLabel->setText(rdSession.isEmpty() ? QStringLiteral("None") : rdSession);

        QString token = m_portalManager->lastRestoreToken();
        if (token.isEmpty()) {
            m_rdTokenLabel->setText(QStringLiteral("None (New authorization on restart)"));
        } else {
            m_rdTokenLabel->setText(QStringLiteral("Persisted (%1 chars)").arg(token.length()));
        }

        m_scStateLabel->setText(m_portalManager->areShortcutsActive()
            ? QStringLiteral("Active")
            : QStringLiteral("Inactive / Degraded"));

        QString scSession = m_portalManager->shortcutsSessionPath();
        m_scSessionLabel->setText(scSession.isEmpty() ? QStringLiteral("None") : scSession);

        m_scTriggersLabel->setText(QStringLiteral("Toggle: %1 (Req: %2) | Stop: %3 (Req: %4)")
            .arg(m_portalManager->toggleShortcutDescription(),
                 m_portalManager->requestedToggleTrigger(),
                 m_portalManager->stopShortcutDescription(),
                 m_portalManager->requestedStopTrigger()));
    }

    // 3. Engine
    if (m_clickController) {
        bool running = m_clickController->isRunning();
        m_engineStateLabel->setText(running ? QStringLiteral("Running") : QStringLiteral("Idle"));
        m_actionsCountLabel->setText(QString::number(m_clickController->actionCount()));

        auto mode = m_clickController->mode();
        m_modeLabel->setText(mode == ClickController::Mode::Click
            ? QStringLiteral("Click (Press / Release)")
            : QStringLiteral("Micro-Movement (rel ±1px)"));

        m_intervalLabel->setText(QStringLiteral("%1 ms (Jitter: ±%2 ms)")
            .arg(m_clickController->interval().count())
            .arg(m_clickController->jitter().count()));

        uint32_t btn = m_clickController->mouseButton();
        QString btnName;
        switch (btn) {
            case BTN_LEFT: btnName = QStringLiteral("Left (BTN_LEFT: 272)"); break;
            case BTN_RIGHT: btnName = QStringLiteral("Right (BTN_RIGHT: 273)"); break;
            case BTN_MIDDLE: btnName = QStringLiteral("Middle (BTN_MIDDLE: 274)"); break;
            default: btnName = QStringLiteral("Button %1").arg(btn); break;
        }
        m_mouseButtonLabel->setText(btnName);

        auto autoStop = m_clickController->autoStopLimit();
        auto countdown = m_clickController->startCountdown();
        m_safetyLimitsLabel->setText(QStringLiteral("Auto-stop: %1 s | Countdown: %2 s")
            .arg(autoStop.count())
            .arg(countdown.count()));
    }
}

void DebugDialog::copyToClipboard() {
    refreshInfo();
    QString report = QStringLiteral(
        "=== Wayland Autoclicker Debug Report ===\n"
        "[Process & Memory]\n"
        "- PID: %1\n"
        "- Uptime: %2\n"
        "- RAM Usage (RSS): %3\n"
        "- Peak RAM (HWM): %4\n"
        "- Virtual Memory (VmSize): %5\n"
        "- Threads: %6\n"
        "- Session Type: %7\n"
        "- Compositor: %8\n\n"
        "[XDG Portals]\n"
        "- RemoteDesktop State: %9\n"
        "- RemoteDesktop Session: %10\n"
        "- Restore Token: %11\n"
        "- GlobalShortcuts State: %12\n"
        "- GlobalShortcuts Session: %13\n"
        "- Triggers: %14\n\n"
        "[Engine & Statistics]\n"
        "- Engine Status: %15\n"
        "- Total Injected Actions: %16\n"
        "- Active Mode: %17\n"
        "- Interval / Jitter: %18\n"
        "- Button: %19\n"
        "- Safety: %20\n"
        "========================================\n"
    ).arg(
        m_pidLabel->text(),
        m_uptimeLabel->text(),
        m_ramUsageLabel->text(),
        m_ramPeakLabel->text(),
        m_vmSizeLabel->text(),
        m_threadsLabel->text(),
        m_sessionTypeLabel->text(),
        m_compositorLabel->text(),
        m_rdStateLabel->text(),
        m_rdSessionLabel->text(),
        m_rdTokenLabel->text(),
        m_scStateLabel->text(),
        m_scSessionLabel->text(),
        m_scTriggersLabel->text(),
        m_engineStateLabel->text(),
        m_actionsCountLabel->text(),
        m_modeLabel->text(),
        m_intervalLabel->text(),
        m_mouseButtonLabel->text(),
        m_safetyLimitsLabel->text()
    );

    QGuiApplication::clipboard()->setText(report);
}
