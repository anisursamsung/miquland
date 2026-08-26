# biway

A lightweight binary-tiling Wayland compositor and window manager built in Modern C++ and [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots).

---

## 🚀 Layout Concept

`biway` enforces an intuitive **two-window-per-workspace binary tiling** layout designed for productive multitasking without complex manual window management:

- **1 Window Open**: Automatically expands to fill 100% of the screen (respecting edge padding).
- **2 Windows Open**:
  - **Horizontal Mode**: Splits 50% left / 50% right.
  - **Vertical Mode**: Splits 50% top / 50% bottom.
  - Switch between Horizontal & Vertical split at any time using `Super + Alt + Space` (or `toggle_split`).
- **3+ Windows Open**: Automatically creates and transitions to the next workspace (e.g. Workspace 2, 3...) and places the window there.
- **Window Closed**: When a window closes in a dual-window workspace, the remaining window automatically returns to full screen.

---

## ✨ Key Features

- **🖼️ Built-in Wallpaper Engine**: Native background layer supporting PNG, JPG, JPEG, and BMP files with theme-based wallpapers configured directly in `light.conf` and `dark.conf`.
- **🪟 Configurable Window Styling**:
  - Rounded window borders (`window_border_radius`) with exterior corner masking against the wallpaper (eliminates sharp client protrusions).
  - Configurable active & inactive border colors (`window_border_color_active`, `window_border_color_inactive`).
  - Adjustable inner gaps between split windows (`space_between_windows`) and outer screen margins (`screen_edge_padding`).
- **📊 Built-in Status Bar**: Top bar displaying an interactive `[ ☰ Menu ]` button, clickable workspace indicators (`1`..`6`), active window title, and live clock. Can be toggled on/off on the fly (`Super + B`).
- **📱 Modal App Launcher**: Centered application launcher modal (`Super + Space`) with instant fuzzy filtering, desktop icon resolution, keyboard arrow navigation, and mouse selection.
- **🧩 Reusable UI Widgets Library (`src/ui/widgets/`)**:
  - `TableView`: Flexible layout container for multi-column grids (`Grid`) and single-column lists (`RowsOnly`).
  - `CellItemView`: Compound item renderer (supports top-down grid tiles and left-to-right list rows).
  - `TextInputView`: Search input box with cursor, placeholder, backspace, and active border glow.
  - `CardView`: Rounded container boxes with background fills and border strokes.
  - `TextView`: Pango text rendering with fonts, colors, alignment, and auto-ellipsizing.
  - `ImageView`: High-performance icon/image loader with resolution scaling and Cairo surface caching.
- **👆 Touchpad Gestures**: Smooth 3-finger horizontal swiping for instant workspace switching.
- **🖱️ Mouse & Pointer Interaction**:
  - Hover-to-focus: Moving the mouse over any window brings it directly into focus without needing a click.
  - Contextual cursor shapes: Cursor dynamically switches to an I-beam (`text`) over input fields and a pointer hand (`pointer`) over interactive elements.
- **⚡ Inotify Live Config Auto-Reload**: Editing `~/.config/biway/biway.conf` automatically applies changes in real-time (wallpapers, borders, gaps, padding, keybindings, natural scroll, icon themes) without restarting.

---

## 📦 Dependencies

On Arch Linux / EndeavourOS / Manjaro:
```bash
sudo pacman -S wlroots0.20 wayland wayland-protocols libxkbcommon libinput pixman cairo pango gdk-pixbuf2 cmake ninja gcc playerctl brightnessctl
```

---

## 🛠️ Building & Installation

`biway` provides a single `./make.sh` script that handles both local development builds and full system installations:

### 1. Local Build (Non-Sudo)
If you run without `sudo`, the script compiles the project locally without modifying any system files:
```bash
./make.sh
```
- Produces a standalone binary at `build/biway`.
- Can be tested directly inside an existing Wayland or X11 session.

### 2. System-Wide Installation (Sudo)
If you run with `sudo`, the script compiles and automatically installs all system assets:
```bash
sudo ./make.sh
```
This automatically configures:
- **Executable**: `/usr/bin/biway`
- **Wayland Session**: `/usr/share/wayland-sessions/biway.desktop` (selectable in GDM, SDDM, LightDM, greetd)
- **System Config & Theme Templates**: `/usr/share/biway/biway.conf`, `light.conf`, `dark.conf`
- **Default Wallpapers**: `/usr/share/backgrounds/biway/lightwallpaper.png`, `/usr/share/backgrounds/biway/darkwallpaper.jpg`
- **User Config**: Automatically creates `~/.config/biway/biway.conf`, `light.conf`, and `dark.conf` with proper non-root user permissions if they do not already exist.

---

## ⚙️ Configuration (`~/.config/biway/biway.conf`)

`biway` reads settings from `~/.config/biway/biway.conf`. All settings reload live on save:

```ini
# biway configuration file
# A binary-tiling Wayland compositor

# ==========================================
# Appearance & Bar Settings
# ==========================================
show_bar = true
bar_height = 30
# Icon Theme (e.g. Papirus, Adwaita, Tela-circle; falls back to hicolor/pixmaps)
icon_theme = Papirus

# ==========================================
# Theme & Colors Configuration
# ==========================================
# Source an external theme file (e.g. light.conf or dark.conf)
[theme]
source = ~/.config/biway/light.conf

# You can customize individual theme colors in ~/.config/biway/light.conf and dark.conf!

# ==========================================
# Input Settings
# ==========================================
natural_scroll = false

# ==========================================
# Window Borders, Spacing & Padding
# ==========================================
[windows]
window_border_width = 2
window_border_radius = 8
space_between_windows = 8
screen_edge_padding = 10

# ==========================================
# Applications
# ==========================================
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

---

## ⌨️ Default Keybindings

| Keybinding | Action |
| :--- | :--- |
| **`Super + Shift + Q`** | **System Reserved**: Exit biway compositor safely |
| **`Super + T`** | **System Reserved**: Launch terminal |
| **`Super + Q`** | Close focused window |
| **`Super + 1`** | Focus Window 1 (first / left / top window) in workspace |
| **`Super + 2`** | Focus Window 2 (second / right / bottom window) in workspace |
| **`Super + Left` / `Right` / `Up` / `Down`** | Toggle focus between windows in active workspace |
| **`Super + Alt + Space`** | Toggle between **Horizontal** and **Vertical** split mode |
| **`Super + Shift + Left`** | Switch to previous workspace |
| **`Super + Shift + Right`** | Switch to next workspace |
| **`Super + Shift + [1..9]`** | Jump directly to workspace 1 to 9 |
| **`Super + Space`** | Toggle application launcher modal |
| **`Super + F`** | Launch web browser (`firefox`) |
| **`Super + Y`** | Launch terminal file manager (`kitty -e yazi`) |
| **`Super + B`** | Toggle top status bar visibility |
| **`XF86AudioPlay`** | Play / Pause media via `playerctl` |
| **`XF86AudioNext` / `Prev`** | Next / Previous track via `playerctl` |
| **`XF86AudioRaiseVolume` / `LowerVolume`** | Volume Up / Down via `wpctl` |
| **`XF86AudioMute`** | Toggle Audio Mute via `wpctl` |
| **`XF86MonBrightnessUp` / `Down`** | Brightness Up / Down via `brightnessctl` |

---

## 👆 Touchpad Gestures

| Gesture | Action |
| :--- | :--- |
| **3-Finger Swipe Left** | Switch to next workspace (`workspace + 1`) |
| **3-Finger Swipe Right** | Switch to previous workspace (`workspace - 1`) |

---

## 🖥️ Running & Testing

### Nested in an Existing Session
To run `biway` in a window inside your current Wayland or X11 session:
```bash
./build/biway
```

### CLI Options
```bash
./build/biway --help
```
- `--no-bar`: Start with the top status bar hidden.
- `-s, --startup <cmd>`: Run a custom command upon compositor initialization.
- `-h, --help`: Display command usage and keybindings.

### Standalone Session
Select **biway** from your display manager session menu (GDM, SDDM, LightDM, greetd) or launch via command line.
