#include "MainWindow.hpp"
#include "DebugDialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QApplication>
#include <linux/input-event-codes.h>

MainWindow::MainWindow(
    PortalManager* portalManager,
    ClickController* clickController,
    Settings* settings,
    TrayController* trayController,
    QWidget* parent
) : QMainWindow(parent),
    m_portalManager(portalManager),
    m_clickController(clickController),
    m_settings(settings),
    m_trayController(trayController) {

    setupUi();
    loadSettings();

    // Portal connections
    connect(m_portalManager, &PortalManager::stateChanged, this, &MainWindow::onPortalStateChanged);
    connect(m_portalManager, &PortalManager::shortcutsStateChanged, this, &MainWindow::onShortcutsStateChanged);
    connect(m_portalManager, &PortalManager::effectiveShortcutsUpdated, this, &MainWindow::onEffectiveShortcutsUpdated);
    connect(m_portalManager, &PortalManager::restoreTokenUpdated, m_settings, &Settings::setRestoreToken);
    connect(m_portalManager, &PortalManager::sessionClosed, this, [this] {
        m_clickController->stop(ClickController::StopReason::SessionClosed);
    });

    // Shortcuts triggered
    connect(m_portalManager, &PortalManager::toggleShortcutTriggered, this, [this] {
        if (m_clickController->isRunning()) {
            onStopClicked();
        } else {
            onStartClicked();
        }
    });
    connect(m_portalManager, &PortalManager::stopShortcutTriggered, this, &MainWindow::onStopClicked);

    // Controller connections
    connect(m_clickController, &ClickController::runStarted, this, &MainWindow::onRunStarted);
    connect(m_clickController, &ClickController::runFinished, this, &MainWindow::onRunFinished);
    connect(m_clickController, &ClickController::countdownTick, this, &MainWindow::onCountdownTick);
    connect(m_clickController, &ClickController::autoStopTick, this, &MainWindow::onAutoStopTick);
    connect(m_clickController, &ClickController::injectionErrorOccurred, this, &MainWindow::onInjectionError);

    // Tray connections
    connect(m_trayController, &TrayController::toggleWindowRequested, this, [this] {
        if (isVisible()) {
            hide();
        } else {
            showNormal();
            activateWindow();
        }
    });
    connect(m_trayController, &TrayController::stopRequested, this, &MainWindow::onStopClicked);
    connect(m_trayController, &TrayController::quitRequested, qApp, &QApplication::quit);

    // Initialize portals
    m_portalManager->initRemoteDesktopSession(m_settings->restoreToken());
    m_portalManager->initGlobalShortcuts(m_settings->toggleShortcut(), m_settings->stopShortcut());
}

MainWindow::~MainWindow() {
    saveSettings();
}

void MainWindow::setupUi() {
    setWindowTitle(QStringLiteral("Wayland Autoclicker (KDE Plasma 6)"));
    setFixedSize(520, 500);

    auto* centralWidget = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    // 1. GroupBox "Click Interval"
    auto* intervalGroup = new QGroupBox(QStringLiteral("Click Interval"), this);
    auto* intervalGrid = new QGridLayout(intervalGroup);

    m_hoursSpinBox = new QSpinBox(this);
    m_hoursSpinBox->setRange(0, 166);
    m_minutesSpinBox = new QSpinBox(this);
    m_minutesSpinBox->setRange(0, 59);
    m_secondsSpinBox = new QSpinBox(this);
    m_secondsSpinBox->setRange(0, 59);
    m_msecsSpinBox = new QSpinBox(this);
    m_msecsSpinBox->setRange(0, 999);

    intervalGrid->addWidget(m_hoursSpinBox, 0, 0);
    intervalGrid->addWidget(m_minutesSpinBox, 0, 1);
    intervalGrid->addWidget(m_secondsSpinBox, 0, 2);
    intervalGrid->addWidget(m_msecsSpinBox, 0, 3);

    auto* l1 = new QLabel(QStringLiteral("Hours"), this);
    l1->setAlignment(Qt::AlignCenter);
    auto* l2 = new QLabel(QStringLiteral("Minutes"), this);
    l2->setAlignment(Qt::AlignCenter);
    auto* l3 = new QLabel(QStringLiteral("Seconds"), this);
    l3->setAlignment(Qt::AlignCenter);
    auto* l4 = new QLabel(QStringLiteral("Milliseconds"), this);
    l4->setAlignment(Qt::AlignCenter);

    intervalGrid->addWidget(l1, 1, 0);
    intervalGrid->addWidget(l2, 1, 1);
    intervalGrid->addWidget(l3, 1, 2);
    intervalGrid->addWidget(l4, 1, 3);

    mainLayout->addWidget(intervalGroup);

    // 2. Middle Row: Options & More Options
    auto* middleLayout = new QHBoxLayout();
    middleLayout->setSpacing(10);

    // Left Column: Options
    auto* optionsGroup = new QGroupBox(QStringLiteral("Options"), this);
    auto* optionsForm = new QFormLayout(optionsGroup);

    m_mouseButtonCombo = new QComboBox(this);
    m_mouseButtonCombo->addItem(QStringLiteral("Left"), static_cast<uint>(BTN_LEFT));
    m_mouseButtonCombo->addItem(QStringLiteral("Right"), static_cast<uint>(BTN_RIGHT));
    m_mouseButtonCombo->addItem(QStringLiteral("Middle"), static_cast<uint>(BTN_MIDDLE));

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(QStringLiteral("Click (Press/Release)"), static_cast<int>(ClickController::Mode::Click));
    m_modeCombo->addItem(QStringLiteral("Micro-movement (±1px)"), static_cast<int>(ClickController::Mode::MicroMovement));

    m_jitterSpinBox = new QSpinBox(this);
    m_jitterSpinBox->setRange(0, 500);
    m_jitterSpinBox->setSuffix(QStringLiteral(" ms"));

    optionsForm->addRow(QStringLiteral("Mouse Button:"), m_mouseButtonCombo);
    optionsForm->addRow(QStringLiteral("Mode:"), m_modeCombo);
    optionsForm->addRow(QStringLiteral("Jitter (±):"), m_jitterSpinBox);

    middleLayout->addWidget(optionsGroup);

    // Right Column: More Options / Safety
    auto* moreGroup = new QGroupBox(QStringLiteral("More Options / Safety"), this);
    auto* moreForm = new QFormLayout(moreGroup);

    m_autoStopSpinBox = new QSpinBox(this);
    m_autoStopSpinBox->setRange(0, 86400); // Up to 24h
    m_autoStopSpinBox->setSpecialValueText(QStringLiteral("Disabled"));
    m_autoStopSpinBox->setSuffix(QStringLiteral(" s"));

    m_countdownSpinBox = new QSpinBox(this);
    m_countdownSpinBox->setRange(0, 30);
    m_countdownSpinBox->setSuffix(QStringLiteral(" s"));

    m_minimizeTrayCheck = new QCheckBox(QStringLiteral("Minimize on start"), this);

    moreForm->addRow(QStringLiteral("Auto-stop:"), m_autoStopSpinBox);
    moreForm->addRow(QStringLiteral("Countdown:"), m_countdownSpinBox);
    moreForm->addRow(m_minimizeTrayCheck);

    middleLayout->addWidget(moreGroup);
    mainLayout->addLayout(middleLayout);

    // 3. GroupBox "Global Shortcuts"
    auto* shortcutsGroup = new QGroupBox(QStringLiteral("Global Shortcuts"), this);
    auto* shortcutsLayout = new QHBoxLayout(shortcutsGroup);
    shortcutsLayout->setSpacing(8);

    m_toggleShortcutEdit = new QKeySequenceEdit(this);
    m_stopShortcutEdit = new QKeySequenceEdit(this);
    m_applyShortcutsButton = new QPushButton(QStringLiteral("Apply"), this);
    m_applyShortcutsButton->setFixedWidth(75);

    m_configureShortcutsButton = new QPushButton(QStringLiteral("KDE Config"), this);
    m_configureShortcutsButton->setFixedWidth(90);
    m_configureShortcutsButton->setToolTip(QStringLiteral("Ouvrir la configuration des raccourcis dans les paramètres système de KDE"));

    shortcutsLayout->addWidget(new QLabel(QStringLiteral("Toggle:"), this));
    shortcutsLayout->addWidget(m_toggleShortcutEdit);
    shortcutsLayout->addWidget(new QLabel(QStringLiteral("Stop:"), this));
    shortcutsLayout->addWidget(m_stopShortcutEdit);
    shortcutsLayout->addWidget(m_applyShortcutsButton);
    shortcutsLayout->addWidget(m_configureShortcutsButton);

    mainLayout->addWidget(shortcutsGroup);

    // 4. Action Buttons
    auto* buttonsLayout = new QHBoxLayout();
    buttonsLayout->setSpacing(12);

    m_startButton = new QPushButton(QStringLiteral("Start"), this);
    m_startButton->setFixedHeight(38);
    m_startButton->setStyleSheet(QStringLiteral(
        "QPushButton { font-weight: bold; background-color: #27ae60; color: white; border-radius: 4px; padding: 6px; }"
        "QPushButton:disabled { background-color: #4b6584; color: #a5b1c2; }"
        "QPushButton:hover:!disabled { background-color: #2ecc71; }"
    ));

    m_stopButton = new QPushButton(QStringLiteral("Stop"), this);
    m_stopButton->setFixedHeight(38);
    m_stopButton->setEnabled(false);
    m_stopButton->setStyleSheet(QStringLiteral(
        "QPushButton { font-weight: bold; background-color: #c0392b; color: white; border-radius: 4px; padding: 6px; }"
        "QPushButton:disabled { background-color: #4b6584; color: #a5b1c2; }"
        "QPushButton:hover:!disabled { background-color: #e74c3c; }"
    ));

    buttonsLayout->addWidget(m_startButton);
    buttonsLayout->addWidget(m_stopButton);
    mainLayout->addLayout(buttonsLayout);

    // 5. Status Footer
    auto* statusGroup = new QGroupBox(QStringLiteral("Status"), this);
    auto* statusLayout = new QHBoxLayout(statusGroup);
    statusLayout->setSpacing(8);
    statusLayout->setContentsMargins(10, 8, 10, 8);

    m_runStatusLabel = new QLabel(QStringLiteral("Run: Inactif"), this);
    m_debugInfoButton = new QPushButton(QStringLiteral("Show Debug Info"), this);
    m_debugInfoButton->setFixedWidth(130);

    statusLayout->addWidget(m_runStatusLabel, 1);
    statusLayout->addWidget(m_debugInfoButton, 0, Qt::AlignRight);

    mainLayout->addWidget(statusGroup);
    setCentralWidget(centralWidget);

    // Signal connections for UI widgets
    connect(m_hoursSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onIntervalSpinBoxChanged);
    connect(m_minutesSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onIntervalSpinBoxChanged);
    connect(m_secondsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onIntervalSpinBoxChanged);
    connect(m_msecsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onIntervalSpinBoxChanged);

    connect(m_mouseButtonCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onOptionChanged);
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onOptionChanged);
    connect(m_jitterSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onOptionChanged);
    connect(m_autoStopSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onOptionChanged);
    connect(m_countdownSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onOptionChanged);
    connect(m_minimizeTrayCheck, &QCheckBox::toggled, this, &MainWindow::onOptionChanged);

    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(m_applyShortcutsButton, &QPushButton::clicked, this, &MainWindow::onApplyShortcutsClicked);
    connect(m_configureShortcutsButton, &QPushButton::clicked, m_portalManager, &PortalManager::configureShortcuts);
    connect(m_debugInfoButton, &QPushButton::clicked, this, &MainWindow::onShowDebugInfoClicked);
}

void MainWindow::loadSettings() {
    setSpinBoxesFromInterval(m_settings->interval());
    m_jitterSpinBox->setValue(static_cast<int>(m_settings->jitter().count()));

    int btnIdx = m_mouseButtonCombo->findData(m_settings->mouseButton());
    if (btnIdx >= 0) m_mouseButtonCombo->setCurrentIndex(btnIdx);

    int modeIdx = m_modeCombo->findData(static_cast<int>(m_settings->mode()));
    if (modeIdx >= 0) m_modeCombo->setCurrentIndex(modeIdx);

    m_autoStopSpinBox->setValue(static_cast<int>(m_settings->autoStopLimit().count()));
    m_countdownSpinBox->setValue(static_cast<int>(m_settings->startCountdown().count()));
    m_minimizeTrayCheck->setChecked(m_settings->minimizeToTrayOnStart());

    QString toggleKey = m_settings->toggleShortcut();
    QString stopKey = m_settings->stopShortcut();
    m_toggleShortcutEdit->setKeySequence(QKeySequence(toggleKey));
    m_stopShortcutEdit->setKeySequence(QKeySequence(stopKey));
    m_startButton->setText(QStringLiteral("Start (%1)").arg(toggleKey));
    m_stopButton->setText(QStringLiteral("Stop (%1)").arg(stopKey));

    updateJitterMax();
    onOptionChanged();
}

void MainWindow::saveSettings() {
    m_settings->setInterval(calculateTotalInterval());
    m_settings->setJitter(std::chrono::milliseconds(m_jitterSpinBox->value()));
    m_settings->setMouseButton(m_mouseButtonCombo->currentData().toUInt());
    m_settings->setMode(static_cast<ClickController::Mode>(m_modeCombo->currentData().toInt()));
    m_settings->setAutoStopLimit(std::chrono::seconds(m_autoStopSpinBox->value()));
    m_settings->setStartCountdown(std::chrono::seconds(m_countdownSpinBox->value()));
    m_settings->setMinimizeToTrayOnStart(m_minimizeTrayCheck->isChecked());

    QString toggleKey = m_toggleShortcutEdit->keySequence().toString();
    QString stopKey = m_stopShortcutEdit->keySequence().toString();
    if (!toggleKey.isEmpty()) m_settings->setToggleShortcut(toggleKey);
    if (!stopKey.isEmpty()) m_settings->setStopShortcut(stopKey);

    m_settings->sync();
}

bool MainWindow::isSequenceValidForGlobal(const QKeySequence& seq, QString& reason) {
    if (seq.isEmpty()) {
        reason = QStringLiteral("Le raccourci ne peut pas être vide.");
        return false;
    }
    QKeyCombination comb = seq[0];
    auto mods = comb.keyboardModifiers();
    auto key = comb.key();

    bool isFunctionKey = (key >= Qt::Key_F1 && key <= Qt::Key_F35) ||
                         key == Qt::Key_Pause || key == Qt::Key_Print;
    bool hasModifier = mods.testFlag(Qt::ControlModifier) ||
                       mods.testFlag(Qt::AltModifier) ||
                       mods.testFlag(Qt::MetaModifier);

    if (key == Qt::Key_NumLock || key == Qt::Key_CapsLock || key == Qt::Key_ScrollLock) {
        reason = QStringLiteral("Les touches de verrouillage (Verr num, Verr maj) ne sont pas autorisées comme déclencheurs.");
        return false;
    }

    if (!isFunctionKey && !hasModifier) {
        reason = QStringLiteral("Un raccourci global nécessite une touche de fonction (F1-F12) ou un modificateur (ex: Ctrl+O, Alt+S, Meta+O). Les lettres et signes seuls ne sont pas interceptés globalement.");
        return false;
    }

    return true;
}

void MainWindow::onApplyShortcutsClicked() {
    auto toggleSeq = m_toggleShortcutEdit->keySequence();
    auto stopSeq = m_stopShortcutEdit->keySequence();

    QString reason;
    if (!isSequenceValidForGlobal(toggleSeq, reason)) {
        QMessageBox::warning(this, QStringLiteral("Raccourci Démarrer / Toggle invalide"), reason);
        return;
    }
    if (!isSequenceValidForGlobal(stopSeq, reason)) {
        QMessageBox::warning(this, QStringLiteral("Raccourci Arrêt invalide"), reason);
        return;
    }

    QString toggleKey = toggleSeq.toString();
    QString stopKey = stopSeq.toString();

    m_settings->setToggleShortcut(toggleKey);
    m_settings->setStopShortcut(stopKey);
    m_settings->sync();

    m_startButton->setText(QStringLiteral("Start (%1)").arg(toggleKey));
    m_stopButton->setText(QStringLiteral("Stop (%1)").arg(stopKey));
    m_startButton->update();
    m_startButton->repaint();
    m_stopButton->update();
    m_stopButton->repaint();

    m_portalManager->rebindShortcuts(toggleKey, stopKey);
}

void MainWindow::onEffectiveShortcutsUpdated(const QString& toggleKey, const QString& stopKey) {
    m_startButton->setText(QStringLiteral("Start (%1)").arg(toggleKey));
    m_stopButton->setText(QStringLiteral("Stop (%1)").arg(stopKey));
    m_startButton->update();
    m_startButton->repaint();
    m_stopButton->update();
    m_stopButton->repaint();
}

std::chrono::milliseconds MainWindow::calculateTotalInterval() const {
    int64_t total = m_hoursSpinBox->value() * 3600000LL
                  + m_minutesSpinBox->value() * 60000LL
                  + m_secondsSpinBox->value() * 1000LL
                  + m_msecsSpinBox->value();
    return TimingPolicy::clampInterval(std::chrono::milliseconds{total});
}

void MainWindow::setSpinBoxesFromInterval(std::chrono::milliseconds interval) {
    auto clamped = TimingPolicy::clampInterval(interval);
    int64_t ms = clamped.count();

    int hours = static_cast<int>(ms / 3600000LL);
    ms %= 3600000LL;
    int minutes = static_cast<int>(ms / 60000LL);
    ms %= 60000LL;
    int seconds = static_cast<int>(ms / 1000LL);
    ms %= 1000LL;

    m_hoursSpinBox->blockSignals(true);
    m_minutesSpinBox->blockSignals(true);
    m_secondsSpinBox->blockSignals(true);
    m_msecsSpinBox->blockSignals(true);

    m_hoursSpinBox->setValue(hours);
    m_minutesSpinBox->setValue(minutes);
    m_secondsSpinBox->setValue(seconds);
    m_msecsSpinBox->setValue(static_cast<int>(ms));

    m_hoursSpinBox->blockSignals(false);
    m_minutesSpinBox->blockSignals(false);
    m_secondsSpinBox->blockSignals(false);
    m_msecsSpinBox->blockSignals(false);
}

void MainWindow::updateJitterMax() {
    auto totalInterval = calculateTotalInterval();
    int maxJitter = static_cast<int>(totalInterval.count() / 2);
    m_jitterSpinBox->setMaximum(maxJitter);
}

void MainWindow::onIntervalSpinBoxChanged() {
    updateJitterMax();
    onOptionChanged();
}

void MainWindow::onOptionChanged() {
    auto interval = calculateTotalInterval();
    m_clickController->setInterval(interval);
    m_clickController->setJitter(std::chrono::milliseconds(m_jitterSpinBox->value()));
    m_clickController->setMouseButton(m_mouseButtonCombo->currentData().toUInt());
    m_clickController->setMode(static_cast<ClickController::Mode>(m_modeCombo->currentData().toInt()));
    m_clickController->setAutoStopLimit(std::chrono::seconds(m_autoStopSpinBox->value()));
    m_clickController->setStartCountdown(std::chrono::seconds(m_countdownSpinBox->value()));
}

void MainWindow::onStartClicked() {
    if (!m_portalManager->hasActiveSession()) {
        QMessageBox::warning(
            this,
            QStringLiteral("Session Wayland requise"),
            QStringLiteral("La session RemoteDesktop Wayland n'est pas encore active. Veuillez autoriser l'accès au pointeur.")
        );
        return;
    }

    onOptionChanged();
    m_clickController->start();

    if (m_minimizeTrayCheck->isChecked()) {
        hide();
    }
}

void MainWindow::onStopClicked() {
    m_clickController->stop(ClickController::StopReason::Manual);
}

void MainWindow::onPortalStateChanged(PortalManager::PortalState state, const QString& message) {
    Q_UNUSED(state);
    Q_UNUSED(message);
    if (m_debugDialog && m_debugDialog->isVisible()) {
        m_debugDialog->refreshInfo();
    }
}

void MainWindow::onShortcutsStateChanged(bool active, const QString& details) {
    Q_UNUSED(active);
    Q_UNUSED(details);
    if (m_debugDialog && m_debugDialog->isVisible()) {
        m_debugDialog->refreshInfo();
    }
}

void MainWindow::onShowDebugInfoClicked() {
    if (!m_debugDialog) {
        m_debugDialog = new DebugDialog(m_portalManager, m_clickController, this);
    }
    m_debugDialog->show();
    m_debugDialog->raise();
    m_debugDialog->activateWindow();
}

void MainWindow::onRunStarted() {
    m_startButton->setEnabled(false);
    m_stopButton->setEnabled(true);
    m_startButton->update();
    m_startButton->repaint();
    m_stopButton->update();
    m_stopButton->repaint();
    m_trayController->setRunning(true);
    m_runStatusLabel->setText(QStringLiteral("Run: Démarré..."));
    m_runStatusLabel->setStyleSheet(QStringLiteral("color: #2980b9; font-weight: bold;"));
}

void MainWindow::onRunFinished(ClickController::StopReason reason) {
    m_startButton->setEnabled(true);
    m_stopButton->setEnabled(false);
    m_startButton->update();
    m_startButton->repaint();
    m_stopButton->update();
    m_stopButton->repaint();
    m_trayController->setRunning(false);

    QString reasonStr;
    switch (reason) {
        case ClickController::StopReason::Manual:
            reasonStr = QStringLiteral("Arrêt manuel");
            break;
        case ClickController::StopReason::AutoStopTimer:
            reasonStr = QStringLiteral("Arrêt automatique (temps écoulé)");
            break;
        case ClickController::StopReason::InjectionFailure:
            reasonStr = QStringLiteral("Échecs répétés d'injection");
            break;
        case ClickController::StopReason::SessionClosed:
            reasonStr = QStringLiteral("Session Wayland fermée");
            break;
        case ClickController::StopReason::SignalTerminated:
            reasonStr = QStringLiteral("Signal système");
            break;
    }

    m_runStatusLabel->setText(QStringLiteral("Run: Inactif (%1)").arg(reasonStr));
    m_runStatusLabel->setStyleSheet(QStringLiteral("color: #2c3e50;"));
}

void MainWindow::onCountdownTick(int remaining) {
    if (remaining > 0) {
        m_runStatusLabel->setText(QStringLiteral("Run: Démarrage dans %1 s...").arg(remaining));
        m_runStatusLabel->setStyleSheet(QStringLiteral("color: #e67e22; font-weight: bold;"));
    } else {
        m_runStatusLabel->setText(QStringLiteral("Run: En cours"));
        m_runStatusLabel->setStyleSheet(QStringLiteral("color: #27ae60; font-weight: bold;"));
    }
}

void MainWindow::onAutoStopTick(int remaining) {
    int h = remaining / 3600;
    int m = (remaining % 3600) / 60;
    int s = remaining % 60;
    m_runStatusLabel->setText(QStringLiteral("Run: En cours (Arrêt auto dans %1h %2m %3s)").arg(h).arg(m).arg(s));
}

void MainWindow::onInjectionError(const QString& error) {
    QMessageBox::critical(this, QStringLiteral("Erreur d'injection"), error);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (m_clickController->isRunning()) {
        auto res = QMessageBox::question(
            this,
            QStringLiteral("Autoclicker en cours"),
            QStringLiteral("L'autoclicker est en cours d'exécution. Voulez-vous le réduire en zone de notification ?"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel
        );

        if (res == QMessageBox::Yes) {
            hide();
            event->ignore();
            return;
        } else if (res == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
    }

    saveSettings();
    event->accept();
}
