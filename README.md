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
If you want it working and installed right way:

```bash
# Clone the repository
git clone https://github.com/anisursamsung/biway.git
cd biway
sudo ./make.sh
```
The make.sh handles everything if passed sudo. It installs the
- binary
- default wallpapers
- creates the directory of configuration. 
- creates the .desktop file needed in wayland-sessions to appear in Login Managers like SDDM, GDM.

If you want to just test then run "./make.sh" without sudo -> cd build  -> ./biway

## Recommended Companion Tools
- **Clipboard Management:** `wl-clipboard` and `wl-clip-persist` (preserves clipboard content after windows close).
- **Notifications:** `mako` or `dunst`.
- **Terminal:** `kitty`, `foot`, `alacritty`, or `wezterm`.
- **Icon Theme:** `papirus-icon-theme` or `adwaita-icon-theme` for menu icons.

---

## License

This project is open-source. See repository for licensing details.
