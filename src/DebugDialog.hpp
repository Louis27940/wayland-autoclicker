#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QElapsedTimer>
#include <chrono>

class PortalManager;
class ClickController;

class DebugDialog : public QDialog {
    Q_OBJECT

public:
    struct MemoryInfo {
        qint64 vmRssKb{0};
        qint64 vmHwmKb{0};
        qint64 vmSizeKb{0};
        qint64 vmPeakKb{0};
        int threads{1};
    };

    static MemoryInfo readProcessMemory();
    static QString formatKb(qint64 kb);

    explicit DebugDialog(
        const PortalManager* portalManager,
        const ClickController* clickController,
        QWidget* parent = nullptr
    );
    ~DebugDialog() override = default;

public slots:
    void refreshInfo();
    void copyToClipboard();

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    const PortalManager* m_portalManager{nullptr};
    const ClickController* m_clickController{nullptr};
    QTimer* m_refreshTimer{nullptr};
    QElapsedTimer m_uptimeTimer;

    // UI Labels - Memory & System
    QLabel* m_pidLabel{nullptr};
    QLabel* m_uptimeLabel{nullptr};
    QLabel* m_ramUsageLabel{nullptr};
    QLabel* m_ramPeakLabel{nullptr};
    QLabel* m_vmSizeLabel{nullptr};
    QLabel* m_threadsLabel{nullptr};
    QLabel* m_sessionTypeLabel{nullptr};
    QLabel* m_compositorLabel{nullptr};

    // UI Labels - Portals
    QLabel* m_rdStateLabel{nullptr};
    QLabel* m_rdSessionLabel{nullptr};
    QLabel* m_rdTokenLabel{nullptr};
    QLabel* m_scStateLabel{nullptr};
    QLabel* m_scSessionLabel{nullptr};
    QLabel* m_scTriggersLabel{nullptr};

    // UI Labels - Engine
    QLabel* m_engineStateLabel{nullptr};
    QLabel* m_actionsCountLabel{nullptr};
    QLabel* m_modeLabel{nullptr};
    QLabel* m_intervalLabel{nullptr};
    QLabel* m_mouseButtonLabel{nullptr};
    QLabel* m_safetyLimitsLabel{nullptr};

    QPushButton* m_refreshButton{nullptr};
    QPushButton* m_copyButton{nullptr};
    QPushButton* m_closeButton{nullptr};
};
