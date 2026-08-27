# Biway

A modern, fast, and opinionated binary-tiling Wayland compositor written in C++ using **wlroots**.

Biway is designed to deliver a smooth, clutter-free desktop experience out of the box with built-in shell components, an Android-inspired cascading theme system, and intelligent automatic window placement.

---

## Key Features

### 🪟 Opinionated Binary Tiling
- **Max 2 Windows Per Workspace:** Every workspace displays at most two windows side-by-side or stacked.
- **Auto-Workspace Overflow:** When a third window opens, Biway automatically moves it to the next workspace to keep your view organized.
- **Smart Fullscreen:** A single window on any workspace automatically expands to use the full screen area.
- **Dynamic Split Direction:** Toggle between horizontal (side-by-side) and vertical (top/bottom) split on the fly with a single keybinding.

### 🎨 Cascading Theme Engine (Android-Style)
- **Live Inotify Hot-Reload:** Configuration and theme files in `~/.config/biway/` and `~/.config/biway/theme/` are monitored in real time—changes apply instantly without restarting the compositor.
- **Modular Sourcing:** Built-in Light and Dark themes driven by `source = theme/theme_mode.conf`. Easily swap color palettes, wallpapers, border widths, and corner radii through modular config files.
- **Customizable Aesthetics:** Control active/inactive border colors, border thickness, corner rounding radius, window gaps, and edge padding.

### 🐚 Built-in Native Shell Components
- **Native Top Bar:** Displays the current workspace indicators, active window title, and live clock. Can be toggled on or off with `Super + B`.
- **Native Application Menu:** Clean, keyboard- and mouse-navigable launcher accessible via `Super + Space` with icon theme support (Papirus, Adwaita, Tela, etc.).
- **Built-in Wallpaper:** Automatically renders your configured wallpaper per theme mode or global setting.

### ⚡ Wayland Native & Compatibility
- **Full XDG Shell & Popups:** Native context menus, tooltips, and dropdowns for Firefox, Chromium, Nautilus, and all GTK/Qt applications.
- **XWayland Integration:** Seamless support for legacy X11 applications.
- **Layer Shell Support (`wlr-layer-shell`):** Compatible with notification daemons (Mako, Dunst), lockscreens (Swaylock), and external bars/launchers (Rofi, Waybar).
- **Touchpad Gestures:** Built-in 3-finger swipe to switch workspaces, natural scrolling option, and tap-to-click.

---

## Default Keybindings

### System & Launchers
| Keybinding | Action | Description |
|---|---|---|
| `Super + T` | `terminal` | Launch terminal (*System reserved*) |
| `Super + Shift + Q` | `exit` | Safely exit Biway compositor (*System reserved*) |
| `Super + Space` | `menu` | Toggle native application launcher |
| `Super + F` | `firefox` | Open Firefox browser |
| `Super + Y` | `kitty -e yazi` | Open file manager (Yazi in terminal) |

### Window Management
| Keybinding | Action | Description |
|---|---|---|
| `Super + Q` | `close` | Close focused window |
| `Super + B` | `toggle_bar` | Show / hide native top bar |
| `Super + 1` | `focus_win_1` | Focus first window in workspace |
| `Super + 2` | `focus_win_2` | Focus second window in workspace |
| `Super + Arrow Keys` | `toggle_focus` | Cycle focus between windows |
| `Super + Alt + Space` | `toggle_split` | Toggle split direction (Horizontal ↔ Vertical) |

### Workspace Navigation
| Keybinding | Action | Description |
|---|---|---|
| `Super + Shift + Left` | `prev_ws` | Switch to previous workspace |
| `Super + Shift + Right` | `next_ws` | Switch to next workspace |
| `Super + Shift + [1-9]` | `ws_[1-9]` | Jump directly to workspace 1–9 |
| *3-Finger Swipe Left / Right* | *Workspace switch* | Switch workspace via touchpad gesture |

### Media & Hardware Controls
| Keybinding | Action |
|---|---|
| `XF86AudioPlay` / `XF86AudioNext` / `XF86AudioPrev` | Play / Pause, Next, Previous track |
| `XF86AudioRaiseVolume` / `XF86AudioLowerVolume` | Adjust volume (+5% / -5%) |
| `XF86AudioMute` | Toggle audio mute |
| `XF86MonBrightnessUp` / `XF86MonBrightnessDown` | Adjust monitor brightness (+10% / -10%) |

---

## Installation Guide

### 1. Install Dependencies

#### Arch Linux / EndeavourOS
```bash
sudo pacman -S --needed \
    base-devel cmake git pkgconf \
    wlroots wayland wayland-protocols libxkbcommon pixman \
    cairo pango gdk-pixbuf2 libinput \
    libxcb xcb-util-wm xcb-util-image
```

#### Fedora
```bash
sudo dnf install \
    gcc-c++ cmake git pkgconf-pkg-config \
    wlroots-devel wayland-devel wayland-protocols-devel libxkbcommon-devel \
    pixman-devel cairo-devel pango-devel gdk-pixbuf2-devel \
    libinput-devel libxcb-devel xcb-util-wm-devel
```

#### Ubuntu / Debian (Testing / Sid)
```bash
sudo apt install \
    build-essential cmake git pkg-config \
    libwlroots-dev libwayland-dev wayland-protocols libxkbcommon-dev \
    libpixman-1-dev libcairo2-dev libpango1.0-dev libgdk-pixbuf-2.0-dev \
    libinput-dev libxcb1-dev libxcb-icccm4-dev libxcb-ewmh-dev
```

---

### 2. Build and Install

```bash
# Clone the repository
git clone https://github.com/anisursamsung/biway.git
cd biway

# Configure the build with CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr

# Compile the binary
cmake --build build -j$(nproc)

# Install system-wide (binary, desktop session entry, default configs & themes)
sudo cmake --install build
```

#### Installed Files and Locations:
- **Binary:** `/usr/bin/biway`
- **Wayland Session Entry:** `/usr/share/wayland-sessions/biway.desktop` (visible in display managers such as GDM, SDDM, Ly, greetd)
- **Default Assets & Themes:** `/usr/share/biway/`

---

### 3. User Configuration Setup

To configure Biway for your user, copy the default configuration and themes to your `~/.config` directory:

```bash
mkdir -p ~/.config/biway
cp -r assets/biway.conf assets/theme ~/.config/biway/
```

Biway will automatically read `~/.config/biway/biway.conf`. Any changes you save will be live-reloaded instantly!

---

### 4. Running Biway

#### From a Display Manager (GDM / SDDM / Ly / LightDM)
Select **Biway** from the session list on your login screen.

#### From a TTY (Command Line)
```bash
biway
```

---

## Recommended Companion Tools
- **Clipboard Management:** `wl-clipboard` and `wl-clip-persist` (preserves clipboard content after windows close).
- **Notifications:** `mako` or `dunst`.
- **Terminal:** `kitty`, `foot`, `alacritty`, or `wezterm`.
- **Icon Theme:** `papirus-icon-theme` or `adwaita-icon-theme` for menu icons.

---

## License

This project is open-source. See repository for licensing details.
