# Wayland Autoclicker

Native Wayland autoclicker and anti-AFK utility designed for KDE Plasma 6 on Linux, operating without root privileges, without access to `/dev/uinput`, and without `udev` rules.

The application relies exclusively on standardized XDG Desktop Portal D-Bus interfaces (`RemoteDesktop` and `GlobalShortcuts`), ensuring native, secure, and compositor-integrated input injection on Wayland.

---

## Features

- **Two Operating Modes**:
  - **Click Mode**: Press and release of the selected mouse button (Left, Right, Middle) at the current pointer coordinates.
  - **Micro-movement Mode**: Alternating relative displacement of +/- 1 pixel with a net-zero return, simulating user activity without triggering accidental clicks in user interfaces or games.
- **Configurable Cadence and Jitter**:
  - Fully adjustable intervals in hours, minutes, seconds, and milliseconds (ranging from 200 ms to 10 minutes).
  - Dynamic randomized jitter (up to 50% of the interval) to prevent strictly periodic patterns.
- **Safety and Anti-Runaway Controls**:
  - Start countdown (0 to 30 seconds) to position the pointer safely before activation.
  - Option to automatically minimize to the system tray on start.
  - Programmable auto-stop timer (from 1 second to 24 hours, or disabled).
  - RAII Failsafe: Guarantees immediate release of any pressed mouse button upon stop, session termination, or signal interruption.
  - Async-signal-safe POSIX signal handler (`SIGINT`, `SIGTERM`, `SIGHUP`) using a `socketpair` for graceful application shutdown.
- **Native Wayland Global Shortcuts**:
  - Two XDG Desktop Portal shortcuts: Start/Stop toggle and an idempotent emergency stop.
  - Direct in-app key sequence capture and rebinding, or configuration through the native KDE Plasma shortcut settings.
  - Bidirectional synchronization with the portal and automatic filtering of modifier lock keys (NumLock, CapsLock).
- **Diagnostics and Debug Window**:
  - Real-time process resource metrics: resident set size (RSS), peak memory (HWM), virtual memory size (VSIZE), and active thread count.
  - Detailed XDG Desktop Portal D-Bus session states and persistent restore token information.
  - Real-time engine statistics (injection attempts, button releases, net displacement).
- **System Integration**:
  - System tray icon with dynamic run-state indicator and context menu.
  - Secure settings persistence in `~/.config/Perso/WaylandAutoclicker.conf` with restricted owner-only permissions (0600 on file, 0700 on directory).

---

## Technical Architecture

The application is built with modern C++20 and Qt6, structured into decoupled single-responsibility modules:

- `PortalManager` ([src/PortalManager.hpp](file:///home/louis/Documents/Projets_Github/Perso/Autoclicker/src/PortalManager.hpp)): Manages asynchronous lifecycles of XDG Desktop Portal sessions (`RemoteDesktop` and `GlobalShortcuts`), listens to D-Bus signals, generates cryptographically secure request tokens, and provides the concrete input injector.
- `InputInjector` ([src/InputInjector.hpp](file:///home/louis/Documents/Projets_Github/Perso/Autoclicker/src/InputInjector.hpp)): Thread-safe abstract interface decoupling click and movement execution from D-Bus internals.
- `ClickController` ([src/ClickController.hpp](file:///home/louis/Documents/Projets_Github/Perso/Autoclicker/src/ClickController.hpp)): Controls the dedicated background worker based on `std::jthread` and `std::stop_token`. Employs `std::condition_variable_any` to ensure instant wakeups without blocking sleeps.
- `TimingPolicy` ([src/TimingPolicy.hpp](file:///home/louis/Documents/Projets_Github/Perso/Autoclicker/src/TimingPolicy.hpp)): Pure domain module (no Qt or D-Bus dependencies) computing absolute deadlines, jitter offsets, and defensive parameter clamping.
- `DebugDialog` ([src/DebugDialog.hpp](file:///home/louis/Documents/Projets_Github/Perso/Autoclicker/src/DebugDialog.hpp)): Diagnostic dialog inspecting system metrics from `/proc/self/status` and live engine state.
- `Settings` ([src/Settings.hpp](file:///home/louis/Documents/Projets_Github/Perso/Autoclicker/src/Settings.hpp)): Encapsulates `QSettings` while enforcing secure POSIX file permissions.
- `MainWindow` / `TrayController`: Compact graphical user interface and desktop tray integration.

---

## Prerequisites

- **Operating System**: Linux with a Wayland session (KDE Plasma 6 recommended).
- **Desktop Portals**: `xdg-desktop-portal` and `xdg-desktop-portal-kde`.
- **Compiler**: GCC >= 10 or Clang supporting C++20 (`std::jthread`, `std::stop_token`).
- **Build System**: CMake >= 3.21 and Ninja.
- **Libraries**:
  - Qt6 (`Core`, `Widgets`, `DBus`, `Test`)
  - Linux system headers (`linux/input-event-codes.h`)

On Arch Linux / Manjaro:
```bash
sudo pacman -S base-devel cmake ninja qt6-base xdg-desktop-portal xdg-desktop-portal-kde
```

On Fedora:
```bash
sudo dnf install gcc-c++ cmake ninja-build qt6-qtbase-devel xdg-desktop-portal xdg-desktop-portal-kde
```

On Ubuntu / Debian (24.04+):
```bash
sudo apt install build-essential cmake ninja-build qt6-base-dev libxkbcommon-dev xdg-desktop-portal xdg-desktop-portal-kde
```

---

## Build and Installation

### 1. Configure and Build

```bash
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

### 2. Run Test Suite

```bash
ctest --test-dir build --output-on-failure
```

### 3. Local Installation (recommended to ~/.local)

```bash
cmake --install build --prefix ~/.local
```

Once installed, the application is available in your desktop application launcher or via command line:
```bash
~/.local/bin/wayland-autoclicker
```

---

## Usage

1. **First Launch**:
   - On initial start, KDE Plasma presents a permission dialog requesting access to pointer emulation (`RemoteDesktop`).
   - Check the option to remember the authorization. A secure persistent restore token will be saved for future sessions.
2. **Shortcut Setup**:
   - Default suggested triggers are `F6` (Toggle Start / Stop) and `F7` (Emergency Stop).
   - Customize shortcuts directly within the UI (Toggle and Stop inputs) or click **KDE Config** to configure via the native Plasma settings module.
3. **Running the Autoclicker**:
   - Configure the desired click interval, jitter, and start countdown.
   - Click **Start** or trigger your global shortcut.
   - Press the stop shortcut at any time to halt operations immediately.
4. **Diagnostics**:
   - Click the **Debug** button to view real-time memory metrics, D-Bus session descriptors, and injection telemetry.

---

## Revoking Portal Permissions

To reset or revoke the desktop portal authorization:
1. Open KDE Plasma **System Settings**.
2. Navigate to **Security & Privacy** -> **Application Permissions**.
3. Locate `WaylandAutoclicker` or `fr.perso.Autoclicker` and revoke pointer access.
4. Optionally remove the local configuration file:
   ```bash
   rm -f ~/.config/Perso/WaylandAutoclicker.conf
   ```

---

## Contributing

Contributions are welcome. Please refer to [CONTRIBUTING.md](file:///home/louis/Documents/Projets_Github/Perso/Autoclicker/CONTRIBUTING.md) for development guidelines, coding standards, and the pull request workflow.
