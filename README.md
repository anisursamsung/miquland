# biway

A lightweight binary-tiling Wayland compositor / window manager built in Modern C++ and [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots).

## 🚀 Layout Concept

`biway` enforces an intuitive two-window-per-workspace binary tiling layout:
- **1 Window Open**: Expands to 100% full screen.
- **2 Windows Open**:
  - **Horizontal Mode**: Splits 50% left, 50% right.
  - **Vertical Mode**: Splits 50% top, 50% bottom.
  - Toggle between Horizontal & Vertical split at any time using `Super + Alt + Space` or `Super + Alt + Return`.
- **3+ Windows Open**: Automatically creates and switches to the next workspace (e.g. Workspace 2, 3...) and places the window there.
- **Window Closed**: When a window is closed in a two-window workspace, the remaining window automatically expands to full screen.

## ✨ Built-in Features

- **🖼️ Built-in Wallpaper Engine**: Native wallpaper background layer supporting PNG, JPG, JPEG, and BMP files with live reload on edit.
- **📊 Built-in Top Bar**: Interactive bar with `[ ☰ Menu ]` button on the left, centered workspace numbers (`1`..`6`), active window title, and live clock.
- **📱 Centered App Launcher Modal**: Built-in 800x400 modal application launcher powered by modular UI Palettes (`ListView`, `ListItemView`, `ModelListItem`) with 5-column 100x100 app icon grid, instant search/filter, and icon pack fallback.
- **👆 Touchpad Gestures & Tap-to-Click**: Built-in libinput touchpad integration (tap-to-click, natural scroll toggle, 3-finger horizontal workspace swiping).
- **👀 Mouse Hover-to-Focus**: Moving the mouse over any window brings it directly into focus without needing a click.
- **⚡ Inotify Live Config Auto-Reload**: Changes to `~/.config/biway/biway.conf` (wallpapers, keybindings, bar settings, icon themes) auto-apply instantly without restarting (just like `hyprland.conf`).
- **📁 Auto-Generated Config Template**: If `~/.config/biway/biway.conf` is missing on launch, a complete, fully-commented default configuration file is automatically created for you.

## 📦 Dependencies

On Arch Linux:
```bash
sudo pacman -S wlroots0.20 wayland wayland-protocols libxkbcommon libinput pixman cairo pango gdk-pixbuf2 cmake ninja gcc
```

## 🛠️ Building

```bash
./make.sh
```
Or via CMake manually:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## ⚙️ Configuration (`~/.config/biway/biway.conf`)

`biway` reads settings from `~/.config/biway/biway.conf`:

```ini
# biway configuration file

# Visual & Behavior Settings
wallpaper = /home/user/Pictures/wallpaper.png
show_bar = true
bar_height = 30
tap_to_click = true
natural_scroll = false
icon_theme = hicolor
terminal = kitty || foot || alacritty || wezterm || weston-terminal || xterm

# Keybindings: bind = <Modifiers>+<Key>, <Action/Command>
# Reserved system bindings: Super+Shift+Q (Exit), Super+T (Terminal)
bind = Super+Return, kitty
bind = Super+Space, menu
bind = Super+D, menu
bind = Super+F, firefox
bind = Super+Y, kitty -e yazi || foot -e yazi || yazi
bind = Super+W, close
bind = Super+B, toggle_bar

# Window focus in active workspace
bind = Super+1, focus_win_1
bind = Super+2, focus_win_2
bind = Super+Left, toggle_focus
bind = Super+Right, toggle_focus
bind = Super+Up, toggle_focus
bind = Super+Down, toggle_focus
bind = Super+H, toggle_focus
bind = Super+L, toggle_focus
bind = Super+Tab, toggle_focus

# Split orientation toggle (Horizontal Left/Right vs Vertical Top/Bottom)
bind = Super+Alt+Space, toggle_split
bind = Super+Alt+Return, toggle_split

# Workspace navigation
bind = Super+Shift+Left, prev_ws
bind = Super+Shift+Right, next_ws
bind = Super+Shift+H, prev_ws
bind = Super+Shift+L, next_ws
bind = Super+Shift+1, ws_1
bind = Super+Shift+2, ws_2
bind = Super+Shift+3, ws_3
```

## ⌨️ Default Keybindings

| Keybinding | Action |
| :--- | :--- |
| `Super + Shift + Q` | **System Reserved**: Exit biway compositor safely |
| `Super + T` | **System Reserved**: Launch terminal |
| `Super + 1` | Switch focus to Window 1 (first/top/left window) in workspace |
| `Super + 2` | Switch focus to Window 2 (second/bottom/right window) in workspace |
| `Super + Left/Right/Up/Down/H/L/Tab` | Toggle focus between windows in active workspace |
| `Super + Alt + Space` / `Super + Alt + Return` | Toggle between **Horizontal (Left/Right)** and **Vertical (Top/Bottom)** split |
| `Super + Shift + Left` / `Super + Shift + H` | Switch to previous workspace |
| `Super + Shift + Right` / `Super + Shift + L` | Switch to next workspace |
| `Super + Shift + [1..9]` | Jump directly to workspace 1 to 9 |
| `Super + Space` / `Super + D` | Toggle application launcher menu |
| `Super + F` | Launch Firefox |
| `Super + Y` / `Super + E` | Launch Yazi file manager |
| `Super + W` / `Super + C` | Close focused window |
| `Super + B` | Toggle top menu bar on/off |

## 👆 Touchpad Gestures

| Gesture | Action |
| :--- | :--- |
| **3-Finger Swipe Left** | Switch to next workspace (`workspace + 1`) |
| **3-Finger Swipe Right** | Switch to previous workspace (`workspace - 1`) |

## 🖥️ Running

```bash
./build/biway
```
