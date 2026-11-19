#!/usr/bin/env bash
set -e

if command -v pacman >/dev/null 2>&1 && pacman -Q wayland-autoclicker-bin >/dev/null 2>&1; then
    echo "Uninstalling wayland-autoclicker-bin via pacman..."
    sudo pacman -R wayland-autoclicker-bin
else
    echo "Removing /usr/local files..."
    sudo rm -f "/usr/local/bin/wayland-autoclicker"
    sudo rm -f "/usr/local/share/applications/fr.perso.Autoclicker.desktop"
    sudo rm -f "/usr/local/share/icons/hicolor/48x48/apps/fr.perso.Autoclicker.png"
    echo "Uninstallation complete."
fi
