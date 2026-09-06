# Miquland

A dynamic tiling Wayland compositor built with wlroots, featuring SceneFX blur and live configuration reloading.

## Features

- **Tiling Layouts**: `spiral` (recursive split) and `stack` (master + stack column).
- **Window Controls**: Floating toggle, fullscreen toggle, mouse drag/resize, gaps, and corner rounding.
- **Protocols**: Layer shell, XWayland, foreign toplevel, ext-workspace, idle notification/inhibition, screencopy, gamma control, session lock.
- **SceneFX Blur**: Hardware-accelerated background blur for windows and layer surfaces.
- **Live Reloading**: Automatically reloads upon changes to `~/.config/miquland/miquland.conf`.

## Dependencies

Arch Linux:
```bash
sudo pacman -S --needed \
    base-devel cmake git pkgconf \
    wlroots wayland wayland-protocols libxkbcommon pixman \
    cairo libinput pango \
    libxcb xcb-util-wm xcb-util-image
```

## Build & Install

```bash
cd miquland
# Build locally
./make.sh

# Install system-wide to /usr
sudo ./make.sh
```

## Keybindings

| Keybinding | Action |
| :--- | :--- |
| `Super + Shift + Q` | Exit compositor |
| `Super + T` | Terminal |
| `Super + Space` | Launcher (`miqulauncher`) |
| `Super + Q` | Close window |
| `Super + V` | Toggle floating |
| `Super + M` / `Super + Shift + F` | Toggle fullscreen |
| `Super + L` | Toggle layout (`spiral` / `stack`) |
| `Super + Return` | Swap focused window with main |
| `Super + Alt + Space` | Toggle split orientation |
| `Super + J` / `K` / `Left` / `Right` | Focus next / prev window |
| `Super + Shift + 1..0` | Switch to workspace 1–10 |
| `Super + Alt + 1..0` | Move window to workspace 1–10 |

## Configuration

Configuration is loaded from `~/.config/miquland/miquland.conf` (falls back to `/usr/share/miquland/miquland.conf`).

```ini
# Appearance & Cursor
cursor_theme = default
cursor_size = 24

# Input
kb_layout = us
# kb_options = grp:alt_shift_toggle,caps:escape
repeat_rate = 25
repeat_delay = 600
tap_to_click = true
natural_scroll = false
disable_while_typing = true
accel_speed = 0.0
accel_profile = adaptive
focus_follows_mouse = true
# touch_output = eDP-1

# Windows
layout = spiral
default_split_ratio = 0.5
window_border_width = 2
window_border_radius = 10
window_border_color_active = #0066ff
window_border_color_inactive = #99c2ff
space_between_windows = 8
screen_edge_padding = 12
smart_gaps = false
workspace_cycle = true

# Window Rules
windowrule = float, xdg-desktop-portal-gtk
windowrule = float, org.freedesktop.impl.portal.desktop.gtk
windowrule = float, zenity
# windowrule = workspace 2, class:firefox
# windowrule = opacity 0.85, class:kitty

# Blur
blur = true
blur_radius = 5
blur_passes = 3
layerrule = blur, miqulauncher

# Keybindings
bind = Super+Shift+Q, exit
bind = Super+T, terminal
bind = Super+Space, miqulauncher
bind = Super+Return, swap_main

# Touchpad & Touchscreen Gestures (swipe:<fingers>:<direction>)
gesture = swipe:3:left, next_ws
gesture = swipe:3:right, prev_ws
gesture = swipe:3:up, toggle_fullscreen
gesture = swipe:3:down, toggle_floating
gesture = swipe:4:up, miqulauncher
gesture = swipe:4:down, miqulock
```

