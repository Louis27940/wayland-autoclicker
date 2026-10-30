#pragma once

#include <QMainWindow>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QKeySequenceEdit>
#include <QCloseEvent>
#include <memory>
#include "ClickController.hpp"
#include "PortalManager.hpp"
#include "Settings.hpp"
#include "TrayController.hpp"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(
        PortalManager* portalManager,
        ClickController* clickController,
        Settings* settings,
        TrayController* trayController,
        QWidget* parent = nullptr
    );
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onIntervalSpinBoxChanged();
    void onOptionChanged();
    void onStartClicked();
    void onStopClicked();
    void onApplyShortcutsClicked();

    void onPortalStateChanged(PortalManager::PortalState state, const QString& message);
    void onShortcutsStateChanged(bool active, const QString& details);
    void onEffectiveShortcutsUpdated(const QString& toggleKey, const QString& stopKey);
    void onRunStarted();
    void onRunFinished(ClickController::StopReason reason);
    void onCountdownTick(int remaining);
    void onAutoStopTick(int remaining);
    void onInjectionError(const QString& error);

private:
    void setupUi();
    void loadSettings();
    void saveSettings();
    void updateJitterMax();
    std::chrono::milliseconds calculateTotalInterval() const;
    void setSpinBoxesFromInterval(std::chrono::milliseconds interval);

    PortalManager* m_portalManager{nullptr};
    ClickController* m_clickController{nullptr};
    Settings* m_settings{nullptr};
    TrayController* m_trayController{nullptr};

    // UI Widgets - Interval
    QSpinBox* m_hoursSpinBox{nullptr};
    QSpinBox* m_minutesSpinBox{nullptr};
    QSpinBox* m_secondsSpinBox{nullptr};
    QSpinBox* m_msecsSpinBox{nullptr};

    // UI Widgets - Options
    QComboBox* m_mouseButtonCombo{nullptr};
    QComboBox* m_modeCombo{nullptr};
    QSpinBox* m_jitterSpinBox{nullptr};

    // UI Widgets - More Options / Safety
    QSpinBox* m_autoStopSpinBox{nullptr};
    QSpinBox* m_countdownSpinBox{nullptr};
    QCheckBox* m_minimizeTrayCheck{nullptr};

    // UI Widgets - Shortcuts
    QKeySequenceEdit* m_toggleShortcutEdit{nullptr};
    QKeySequenceEdit* m_stopShortcutEdit{nullptr};
    QPushButton* m_applyShortcutsButton{nullptr};
    QPushButton* m_configureShortcutsButton{nullptr};

    static bool isSequenceValidForGlobal(const QKeySequence& seq, QString& reason);

    // UI Widgets - Actions
    QPushButton* m_startButton{nullptr};
    QPushButton* m_stopButton{nullptr};

    // UI Widgets - Status Footer
    QLabel* m_portalStatusLabel{nullptr};
    QLabel* m_shortcutsStatusLabel{nullptr};
    QLabel* m_runStatusLabel{nullptr};
};
