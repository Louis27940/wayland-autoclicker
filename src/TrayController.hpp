#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>

class TrayController : public QObject {
    Q_OBJECT

public:
    explicit TrayController(QObject* parent = nullptr);

    void setRunning(bool running);
    void show();
    void hide();

signals:
    void toggleWindowRequested();
    void stopRequested();
    void quitRequested();

private:
    void updateIcon();

    QSystemTrayIcon* m_trayIcon{nullptr};
    QMenu* m_menu{nullptr};
    QAction* m_toggleAction{nullptr};
    QAction* m_stopAction{nullptr};
    QAction* m_quitAction{nullptr};
    bool m_running{false};
};
