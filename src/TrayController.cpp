#include "TrayController.hpp"
#include <QPainter>
#include <QPixmap>

TrayController::TrayController(QObject* parent)
    : QObject(parent),
      m_trayIcon(new QSystemTrayIcon(this)),
      m_menu(new QMenu()) {

    m_toggleAction = m_menu->addAction(QStringLiteral("Afficher / Masquer"), this, &TrayController::toggleWindowRequested);
    m_stopAction = m_menu->addAction(QStringLiteral("Arrêter"), this, &TrayController::stopRequested);
    m_stopAction->setEnabled(false);
    m_menu->addSeparator();
    m_quitAction = m_menu->addAction(QStringLiteral("Quitter"), this, &TrayController::quitRequested);

    m_trayIcon->setContextMenu(m_menu);

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            emit toggleWindowRequested();
        }
    });

    updateIcon();
}

void TrayController::setRunning(bool running) {
    m_running = running;
    m_stopAction->setEnabled(running);
    updateIcon();
}

void TrayController::show() {
    m_trayIcon->show();
}

void TrayController::hide() {
    m_trayIcon->hide();
}

void TrayController::updateIcon() {
    // Generate crisp 24x24 icon pixmap
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_running) {
        // Active: vibrant emerald green circle with white dot
        painter.setBrush(QColor(46, 204, 113));
        painter.setPen(QPen(QColor(39, 174, 96), 1.5));
        painter.drawEllipse(3, 3, 18, 18);

        painter.setBrush(Qt::white);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(9, 9, 6, 6);

        m_trayIcon->setToolTip(QStringLiteral("Wayland Autoclicker (Actif)"));
    } else {
        // Idle: slate grey circle
        painter.setBrush(QColor(127, 140, 141));
        painter.setPen(QPen(QColor(108, 122, 137), 1.5));
        painter.drawEllipse(3, 3, 18, 18);

        m_trayIcon->setToolTip(QStringLiteral("Wayland Autoclicker (Inactif)"));
    }

    m_trayIcon->setIcon(QIcon(pixmap));
}
