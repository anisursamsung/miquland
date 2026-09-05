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
# touch_output = eDP-1

# Colors
[colors]
color_primary              = #0066ff
color_on_primary           = #ffffff
color_primary_container    = #cce5ff
color_on_primary_container = #002b66
color_secondary            = #e6f0fa
color_on_secondary         = #0f172a
color_background           = #f4f8fc
color_surface              = #ffffff
color_surface_variant      = #e6eff8
color_on_surface           = #0f172a
color_on_surface_variant   = #475569
color_outline              = #99c2ff
color_outline_variant      = #dbeafe

# Windows
[windows]
layout = spiral
window_border_width = 2
window_border_radius = 10
space_between_windows = 8
screen_edge_padding = 12

# Blur
[blur]
blur = true
blur_radius = 5
blur_passes = 3
layerrule = blur, miqulauncher

# Keybindings
bind = Super+Space, miqulauncher
bind = Super+Return, swap_main
```
