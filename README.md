# biway

A lightweight binary-tiling Wayland compositor / window manager built in Modern C++ and [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots).

## 🚀 Layout Concept

`biway` enforces an intuitive two-window-per-workspace binary tiling layout:
- **1 Window Open**: Expands to 100% full screen.
- **2 Windows Open**:
  - **Horizontal Mode**: Splits 50% left, 50% right.
  - **Vertical Mode**: Splits 50% top, 50% bottom.
  - Toggle between Horizontal & Vertical split at any time using `Super + Alt + Space`.
- **3+ Windows Open**: Automatically creates and switches to the next workspace (e.g. Workspace 2, 3...) and places the window there.
- **Window Closed**: When a window is closed in a two-window workspace, the remaining window automatically returns to full screen.

## ✨ Built-in Features

- **🖼️ Built-in Wallpaper Engine**: Native wallpaper background layer supporting PNG, JPG, JPEG, and BMP files with automatic fallback to `/usr/share/backgrounds/biway/wallpaper.png`.
- **📊 Built-in Top Bar**: Interactive bar with `[ ☰ Menu ]` button on the left, centered workspace numbers (`1`..`6`), active window title, and live clock.
- **📱 Centered App Launcher Modal**: Built-in 800x400 modal application launcher powered by modular UI Palettes (`ListView`, `ListItemView`, `ModelListItem`) with 5-column 100x100 app icon grid, instant search/filter, and icon pack fallback.
- **👆 Touchpad Gestures & Tap-to-Click**: Built-in libinput touchpad integration (hardcoded tap-to-click, natural scroll toggle, 3-finger horizontal workspace swiping).
- **👀 Mouse Hover-to-Focus**: Moving the mouse over any window brings it directly into focus without needing a click.
- **⚡ Inotify Live Config Auto-Reload**: Changes to `~/.config/biway/biway.conf` (wallpapers, keybindings, bar settings, icon themes, natural scroll) auto-apply instantly upon saving without restarting.
- **📁 Two-Tier Configuration Architecture**: 
  - System master template at `/usr/share/biway/biway.conf`.
  - User configuration at `~/.config/biway/biway.conf` (auto-provisioned on first run if missing).

## 📦 Dependencies

On Arch Linux:
```bash
sudo pacman -S wlroots0.20 wayland wayland-protocols libxkbcommon libinput pixman cairo pango gdk-pixbuf2 cmake ninja gcc playerctl brightnessctl
```

## 🛠️ Building & Installing

### Build Locally
```bash
./make.sh
```

### Install System-Wide (Binary, Wayland Session, Assets, Default Config)
```bash
sudo ./make.sh
```
This automatically installs:
- Executable: `/usr/bin/biway`
- Wayland Session: `/usr/share/wayland-sessions/biway.desktop` (selectable in GDM, SDDM, LightDM, greetd)
- Default Config Template: `/usr/share/biway/biway.conf`
- Default Wallpaper: `/usr/share/backgrounds/biway/wallpaper.png`
- Auto-initializes `~/.config/biway/biway.conf` with proper user ownership.

## ⚙️ Configuration (`~/.config/biway/biway.conf`)

`biway` reads settings from `~/.config/biway/biway.conf`:

```ini
# biway configuration file
# A binary-tiling Wayland compositor

# ==========================================
# Visual & Behavior Settings
# ==========================================
wallpaper = /usr/share/backgrounds/biway/wallpaper.png
show_bar = true
bar_height = 30
natural_scroll = false
icon_theme = hicolor
terminal = kitty

# ==========================================
# Keybindings
# Format: bind = <Modifiers>+<Key>, <Action/Command>
# ==========================================

# --- System Reserved (Hardcoded) ---
# Super + Shift + Q : Exit biway compositor safely
# Super + T         : Launch terminal

# --- Application Launchers ---
bind = Super+Space, menu
bind = Super+F, firefox
bind = Super+Y, kitty -e yazi

# --- Window Management ---
bind = Super+Q, close
bind = Super+B, toggle_bar

# Switch focus between windows in active workspace
bind = Super+1, focus_win_1
bind = Super+2, focus_win_2
bind = Super+Left, toggle_focus
bind = Super+Right, toggle_focus
bind = Super+Up, toggle_focus
bind = Super+Down, toggle_focus

# Toggle between Horizontal (Left/Right) and Vertical (Top/Bottom) split
bind = Super+Alt+Space, toggle_split

# --- Workspace Navigation ---
bind = Super+Shift+Left, prev_ws
bind = Super+Shift+Right, next_ws

# Switch directly to workspace 1..9
bind = Super+Shift+1, ws_1
bind = Super+Shift+2, ws_2
bind = Super+Shift+3, ws_3
bind = Super+Shift+4, ws_4
bind = Super+Shift+5, ws_5
bind = Super+Shift+6, ws_6
bind = Super+Shift+7, ws_7
bind = Super+Shift+8, ws_8
bind = Super+Shift+9, ws_9

# --- Media & Brightness Controls ---
bind = XF86AudioPlay, playerctl play-pause
bind = XF86AudioNext, playerctl next
bind = XF86AudioPrev, playerctl previous
bind = XF86AudioRaiseVolume, wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%+
bind = XF86AudioLowerVolume, wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%-
bind = XF86AudioMute, wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle
bind = XF86MonBrightnessUp, brightnessctl set +10%
bind = XF86MonBrightnessDown, brightnessctl set 10%-
```

## ⌨️ Default Keybindings

| Keybinding | Action |
| :--- | :--- |
| **`Super + Shift + Q`** | **System Reserved**: Exit biway compositor safely |
| **`Super + T`** | **System Reserved**: Launch terminal |
| **`Super + Q`** | Close focused window |
| **`Super + 1`** | Focus Window 1 (first/top/left window) in workspace |
| **`Super + 2`** | Focus Window 2 (second/bottom/right window) in workspace |
| **`Super + Left` / `Right` / `Up` / `Down`** | Toggle focus between windows in active workspace |
| **`Super + Alt + Space`** | Toggle between **Horizontal** and **Vertical** split mode |
| **`Super + Shift + Left`** | Switch to previous workspace |
| **`Super + Shift + Right`** | Switch to next workspace |
| **`Super + Shift + [1..9]`** | Jump directly to workspace 1 to 9 |
| **`Super + Space`** | Toggle application launcher menu |
| **`Super + F`** | Launch Firefox |
| **`Super + Y`** | Launch Yazi file manager (`kitty -e yazi`) |
| **`Super + B`** | Toggle top status/menu bar on/off |
| **`XF86AudioPlay`** | Play / Pause media via `playerctl` |
| **`XF86AudioNext` / `Prev`** | Next / Previous track via `playerctl` |
| **`XF86AudioRaiseVolume` / `LowerVolume`** | Volume Up / Down via `wpctl` |
| **`XF86AudioMute`** | Toggle Audio Mute via `wpctl` |
| **`XF86MonBrightnessUp` / `Down`** | Brightness Up / Down via `brightnessctl` |

## 👆 Touchpad Gestures

| Gesture | Action |
| :--- | :--- |
| **3-Finger Swipe Left** | Switch to next workspace (`workspace + 1`) |
| **3-Finger Swipe Right** | Switch to previous workspace (`workspace - 1`) |
| **Tap-to-Click** | Single tap on touchpad to click (hardcoded enabled) |

## 🖥️ Running

Launch directly or run nested in an existing desktop session for testing:
```bash
./build/biway
```
Or start directly via your display manager by selecting **biway** at login.
