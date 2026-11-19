#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PKG_FILE=$(find "${SCRIPT_DIR}" -maxdepth 1 -name "wayland-autoclicker-bin-*.pkg.tar.zst" | head -n 1)

if command -v pacman >/dev/null 2>&1 && [ -n "${PKG_FILE}" ]; then
    echo "Installing package via pacman: ${PKG_FILE}"
    sudo pacman -U --needed "${PKG_FILE}"
else
    echo "Direct installation to /usr/local..."
    sudo install -Dm755 "${SCRIPT_DIR}/wayland-autoclicker" "/usr/local/bin/wayland-autoclicker"
    sudo install -Dm644 "${SCRIPT_DIR}/fr.perso.Autoclicker.desktop" "/usr/local/share/applications/fr.perso.Autoclicker.desktop"
    sudo install -Dm644 "${SCRIPT_DIR}/fr.perso.Autoclicker.png" "/usr/local/share/icons/hicolor/48x48/apps/fr.perso.Autoclicker.png"
    echo "Installation complete."
fi
