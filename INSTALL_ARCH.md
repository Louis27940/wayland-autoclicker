# Installation on Arch Linux / CachyOS / Manjaro

This branch provides the compiled (Release) version of Wayland Autoclicker along with all necessary files to install it on Arch Linux and its derivatives.

## Branch Contents

- **`bin/wayland-autoclicker`**: Standalone binary compiled in Release mode, optimized, and stripped.
- **`packaging/arch/PKGBUILD`**: Arch Linux package build specification (`wayland-autoclicker-bin`).
- **`packaging/arch/.SRCINFO`**: Package metadata for repositories and AUR helpers.
- **`packaging/arch/wayland-autoclicker-bin-2.0.0-1-x86_64.pkg.tar.zst`**: Pre-built binary package ready for direct installation via `pacman`.
- **`packaging/arch/install.sh`**: Automated installation helper script.
- **`packaging/arch/uninstall.sh`**: Uninstallation helper script.

---

## Runtime Requirements

To run Wayland Autoclicker on Arch Linux, the following packages are required:

- `qt6-base`
- `xdg-desktop-portal`
- `hicolor-icon-theme`
- `xdg-desktop-portal-kde` (strongly recommended on KDE Plasma 6 for native global shortcut and remote desktop portal integration)

To install these dependencies manually with `pacman`:

```bash
sudo pacman -S --needed qt6-base xdg-desktop-portal hicolor-icon-theme xdg-desktop-portal-kde
```

---

## Installation Methods

### Method 1: Direct Package Installation (Recommended)

The ready-to-use package is included directly in the repository. You can install it immediately with `pacman`:

```bash
sudo pacman -U packaging/arch/wayland-autoclicker-bin-2.0.0-1-x86_64.pkg.tar.zst
```

### Method 2: Using the Installation Script

Run the provided installation script:

```bash
./packaging/arch/install.sh
```

### Method 3: Building the Package with makepkg

If you prefer to rebuild or verify the package locally using `makepkg`:

```bash
cd packaging/arch
makepkg -si
```

### Method 4: Direct Standalone Execution

You can also run the compiled binary directly without installing it system-wide:

```bash
./bin/wayland-autoclicker
```

---

## Uninstallation

To remove the package installed via `pacman`:

```bash
sudo pacman -R wayland-autoclicker-bin
```

Or by running the uninstallation script:

```bash
./packaging/arch/uninstall.sh
```
