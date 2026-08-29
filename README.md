# Biway

A modern, fast, dynamic tiling Wayland compositor written in C++ using **wlroots**.

Biway is designed to deliver a smooth, high-performance desktop experience out of the box with intelligent dynamic layouts, built-in shell components, an Android-inspired cascading theme system, and rich Wayland/XWayland compatibility.

---

## Key Features

### 🪟 Intelligent Dynamic Tiling (`Spiral` & `Stack`)
- **`Spiral` Layout (Binary Space Partitioning):** Recursively subdivides available space in an alternating spiral, maximizing screen utilization for any number of windows.
- **`Stack` Layout (Main & Stack):** Keeps a prominent main window on the left, with all secondary windows neatly stacked in a vertical column on the right.
- **Live Layout Switching:** Instantly switch between `Spiral` and `Stack` layouts with `Super + L`.
- **Main Window Swap:** Promote any focused window to the primary position instantly with `Super + Return`.
- **Smart Fullscreen:** Single window automatically expands to use the full padded screen area.
- **Orientation Control:** Toggle horizontal and vertical split orientation on the fly with `Super + Alt + Space`.

### 🎨 Cascading Theme Engine (Android-Style)
- **Live Inotify Hot-Reload:** Configuration and theme files in `~/.config/biway/` and `~/.config/biway/theme/` are monitored in real time—changes apply instantly without restarting the compositor.
- **Modular Sourcing:** Built-in Light and Dark themes driven by `source = theme/theme_mode.conf`. Easily swap color palettes, wallpapers, border widths, and corner radii through modular config files.
- **Customizable Aesthetics:** Control active/inactive border colors, border thickness, corner rounding radius, window gaps, and edge padding.

### 🐚 Native Shell & External Panels Integration
- **Full Layer Shell Support (`wlr-layer-shell`):** Works natively with any modern bar, panel, or dock (Waybar, Eww, etc.) with automatic exclusive zone layout compensation.
- **Native Application Menu (`biwaymenu`):** Clean, keyboard- and mouse-navigable launcher accessible via `Super + Space` with icon theme support (Papirus, Adwaita, Tela, etc.).
- **Wallpaper Independence:** Seamlessly integrates with standard background managers (`swaybg`, `swww`, `hyprpaper`, `mpvpaper`).

### ⚡ Wayland Native & Compatibility
- **XWayland Integration:** Seamless support for legacy X11 applications.
- **Foreign Toplevel Management (`wlr-foreign-toplevel`):** Full integration with external taskbars, pagers, and window switchers.
- **Touchpad Gestures:** Built-in 3-finger swipe to switch workspaces, natural scrolling option, and tap-to-click.

---

## Default Keybindings

### System & Launchers
| Keybinding | Action | Description |
|---|---|---|
| `Super + Space` | `menu` | Launch application menu (`biwaymenu`) |
| `Super + T` | `terminal` | Launch configured terminal |
| `Super + F` | `firefox` | Launch web browser |
| `Super + E` | `file_manager` | Launch file manager |
| `Super + Shift + Q` | `exit` | Safely exit Biway compositor (*System reserved*) |

### Window & Layout Management
| Keybinding | Action | Description |
|---|---|---|
| `Super + Q` | `close` | Close focused window |
| `Super + L` | `toggle_layout` | Switch between `Spiral` and `Stack` tiling layouts |
| `Super + Return` | `swap_main` | Swap focused window with primary / main window |
| `Super + J` / `Super + Down` | `next_window` | Focus next window |
| `Super + K` / `Super + Up` | `prev_window` | Focus previous window |
| `Super + Alt + Space` | `toggle_split` | Toggle horizontal / vertical split direction |

### Workspaces
| Keybinding | Action | Description |
|---|---|---|
| `Super + Shift + Left` | `prev_ws` | Switch to previous workspace |
| `Super + Shift + Right` | `next_ws` | Switch to next workspace |
| `Super + Shift + [1-9]` | `ws_[1-9]` | Jump directly to workspace 1 through 9 |

---

## Installation Guide

### 1. Install Dependencies

```bash
sudo pacman -S --needed \
    base-devel cmake git pkgconf \
    wlroots wayland wayland-protocols libxkbcommon pixman \
    cairo pango gdk-pixbuf2 libinput \
    libxcb xcb-util-wm xcb-util-image
```

### 2. Build and Install

```bash
cd ~/biway
sudo ./make.sh
```

`make.sh` automatically installs:
- The compositor binary (`/usr/local/bin/biway` or `/usr/bin/biway`)
- Default themes, configuration files, and wallpapers to `~/.config/biway/`
- Wayland session desktop entry to `/usr/share/wayland-sessions/biway.desktop` for login managers (SDDM, GDM, LightDM).

---

## Recommended Companion Tools
- **Toolkit & Launcher:** `biwaytoolkit` & `biwaymenu`.
- **Clipboard Management:** `wl-clipboard` and `wl-clip-persist`.
- **Notifications:** `dunst` or `mako`.
- **Terminal:** `kitty`, `foot`, `alacritty`, or `wezterm`.
- **Icon Theme:** `papirus-icon-theme` or `tela-circle-icon-theme`.
