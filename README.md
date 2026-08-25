# biway

A lightweight binary-tiling Wayland compositor / window manager built in Modern C++ and [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots).

## 🚀 Layout Concept

`biway` enforces an intuitive two-window-per-workspace binary tiling layout:
- **1 Window Open**: Expands to 100% full screen.
- **2 Windows Open**: Splits horizontally into two equal halves (50% left, 50% right).
- **3+ Windows Open**: Automatically creates and switches to the next workspace (e.g. Workspace 2, 3...) and places the window there.
- **Window Closed**: When a window is closed in a two-window workspace, the remaining window automatically returns to full screen.

## ✨ Built-in Features

- **🖼️ Built-in Wallpaper Engine**: Native wallpaper background layer supporting PNG, JPG, JPEG, and BMP files (falls back cleanly to solid black if not specified).
- **📊 Built-in Status / Menu Bar**: Interactive top bar with workspace switcher buttons (clickable), active window title, and live clock.
- **👆 Touchpad Gestures**: 3-finger horizontal swipe gestures (swipe left for next workspace, swipe right for previous workspace).
- **⚙️ Config File**: Auto-loads and persists settings to `~/.config/biway/biway.conf`.

## 📦 Dependencies

On Arch Linux:
```bash
sudo pacman -S wlroots0.20 wayland wayland-protocols libxkbcommon pixman cairo pango gdk-pixbuf2 cmake ninja gcc
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
wallpaper = /home/user/Pictures/wallpaper.png
show_bar = true
bar_height = 30
```

You can set the wallpaper dynamically at any time using:
```bash
biway --setwallpaper "/path/to/wallpaper.png"
```
*(This automatically updates `wallpaper =` in `~/.config/biway/biway.conf`)*

## ⌨️ Default Keybindings

| Keybinding | Action |
| :--- | :--- |
| `Super + Return` | Launch terminal (`$TERMINAL`, `foot`, `kitty`, `alacritty`) |
| `Super + D` / `Super + Space` | Launch app menu (`fuzzel`, `wofi`, `bemenu-run`) |
| `Super + B` | Toggle top menu bar on/off |
| `Super + Q` / `Super + Shift + C` | Close focused window |
| `Super + H` / `Super + Left` | Focus left / previous window |
| `Super + L` / `Super + Right` / `Super + Tab` | Focus right / next window |
| `Super + [1..9]` | Switch to workspace 1 to 9 |
| `Super + Shift + [1..9]` | Move focused window to workspace 1 to 9 |
| `Super + Shift + E` | Exit biway compositor |

## 👆 Touchpad Gestures

| Gesture | Action |
| :--- | :--- |
| **3-Finger Swipe Left** | Switch to next workspace (`workspace + 1`) |
| **3-Finger Swipe Right** | Switch to previous workspace (`workspace - 1`) |

## 🖥️ Running

You can run `biway` nested inside an existing Wayland or X11 session for testing:

```bash
./build/biway
```

Or start with options:

```bash
./build/biway --setwallpaper "/path/to/image.png"
./build/biway --no-bar
./build/biway -s "foot"
```

