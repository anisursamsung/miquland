# 🌌 Miquland

A modern, lightweight, dynamic tiling Wayland compositor built with **wlroots**, featuring live configuration hot-reloading, Material-inspired dynamic theming, and an integrated companion ecosystem.

---

## ✨ Features

### 🪟 Intelligent Dynamic Tiling
- **`Spiral` Layout (Binary Space Partitioning):** Recursively subdivides available space in an alternating spiral pattern, maximizing screen real estate.
- **`Stack` Layout (Master + Stack):** Keeps a prominent primary window on the left with secondary windows neatly stacked in a vertical column.
- **Instant Layout Toggle:** Switch between `Spiral` and `Stack` layouts on the fly (`Super + L`).
- **Main Window Swap:** Promote any focused window to the primary position instantly (`Super + Return`).
- **Split Orientation Control:** Toggle between horizontal and vertical split orientations (`Super + Alt + Space`).
- **Smart Window Aesthetics:** Customizable rounded corners, inner gaps, and outer edge padding.

### 🎨 Live Theming & Hot Reloading
- **Zero-Restart Reloading (`inotify`):** Changes to `miquland.conf` or theme files are applied immediately without restarting the session.
- **Dynamic Theme Modes:** Switch seamlessly between dark and light modes via `theme/theme_mode.conf`.
- **Material Design 3 Palette:** Unified styling across active/inactive window borders, cards, and UI components.

### ⚡ Wayland Native & Performance
- Built on top of **wlroots** and modern **Wayland protocols**.
- **XWayland Support:** Seamless compatibility for legacy X11 applications.
- **Multi-Workspace Navigation:** Independent workspaces (1–9) with fast keyboard switching.
- **Input Customization:** Native support for touchpad tap-to-click and natural scrolling.

---

## 🧩 The Miquland Ecosystem

Miquland is barebone by design and works seamlessly with its companion native tools:

| Component | Directory | Description |
| :--- | :--- | :--- |
| **`miquland`** | `miquland/` | Core dynamic tiling Wayland compositor |
| **`miqutoolkit`** | `miqutoolkit/` | Declarative, lightweight C++20 Cairo/Pango UI framework |
| **`miqulauncher`** | `miqulauncher/` | Fast, layer-shell application launcher built with Miqutoolkit |

---

## 🚀 Installation

### 1. Install Dependencies (Arch Linux)

```bash
sudo pacman -S --needed \
    base-devel cmake git pkgconf \
    wlroots wayland wayland-protocols libxkbcommon pixman \
    cairo libinput pango \
    libxcb xcb-util-wm xcb-util-image
```

### 2. Build & Install via `make.sh`

Each component includes a `make.sh` script for building and installing to `/usr`:

#### Step A: Build & Install `miqutoolkit` (Required for launcher)
```bash
cd miqutoolkit
sudo ./make.sh
```

#### Step B: Build & Install `miqulauncher` (Application menu)
```bash
cd miqulauncher
sudo ./make.sh
```

#### Step C: Build & Install `miquland` (Compositor)
```bash
cd miquland
sudo ./make.sh
```

`make.sh` automatically installs:
- The compositor binary to `/usr/bin/miquland`
- Default templates and themes to `/usr/share/miquland/` (copied to `~/.config/miquland/` on first launch)
- Wayland session entry to `/usr/share/wayland-sessions/miquland.desktop` for display managers (SDDM, GDM, LightDM, etc.)

---

## ⌨️ Default Keybindings

### 🔒 System Reserved
| Keybinding | Action |
| :--- | :--- |
| <kbd>Super</kbd> + <kbd>Shift</kbd> + <kbd>Q</kbd> | Exit Miquland compositor |
| <kbd>Super</kbd> + <kbd>T</kbd> | Launch default terminal (`$TERMINAL` or `kitty`/`foot`/`alacritty`) |

### 🚀 Applications & Window Management
| Keybinding | Action |
| :--- | :--- |
| <kbd>Super</kbd> + <kbd>Space</kbd> | Launch Application Menu (`miqulauncher`) |
| <kbd>Super</kbd> + <kbd>F</kbd> | Launch Web Browser (`firefox`) |
| <kbd>Super</kbd> + <kbd>E</kbd> | Launch File Manager (`kitty -e yazi`) |
| <kbd>Super</kbd> + <kbd>Q</kbd> | Close active window |
| <kbd>Super</kbd> + <kbd>L</kbd> | Toggle tiling layout (`Spiral` ↔ `Stack`) |
| <kbd>Super</kbd> + <kbd>Return</kbd> | Swap focused window with master/main position |
| <kbd>Super</kbd> + <kbd>Alt</kbd> + <kbd>Space</kbd> | Toggle split orientation (Horizontal ↔ Vertical) |

### 🧭 Navigation & Workspaces
| Keybinding | Action |
| :--- | :--- |
| <kbd>Super</kbd> + <kbd>J</kbd> / <kbd>K</kbd> / <kbd>Arrows</kbd> | Focus Next / Previous window |
| <kbd>Super</kbd> + <kbd>1</kbd> / <kbd>2</kbd> | Focus Window 1 / Window 2 |
| <kbd>Super</kbd> + <kbd>Shift</kbd> + <kbd>Left</kbd> / <kbd>Right</kbd> | Switch to Previous / Next Workspace |
| <kbd>Super</kbd> + <kbd>Shift</kbd> + <kbd>1..9</kbd> | Switch directly to Workspace 1–9 |

---

## ⚙️ Configuration & Dynamic Theming

On first run, Miquland automatically generates the default configuration structure in `~/.config/miquland/`:

```
~/.config/miquland/
├── miquland.conf            # Main compositor settings & keybindings
└── theme/
    ├── theme_mode.conf      # Theme mode switcher
    ├── dark.conf            # Material Dark color palette
    ├── light.conf           # Material Light color palette
    ├── darkmodescript.sh    # Script executed on dark mode switch
    └── lightmodescript.sh   # Script executed on light mode switch
```

### Example `miquland.conf`
```ini
# Autostart applications
exec_once = waybar &
exec_once = dunst &

# Window Layout & Padding
[windows]
layout = spiral
window_border_width = 3
window_border_radius = 20
space_between_windows = 10
screen_edge_padding = 20

# Input Settings
[input]
tap_to_click = true
natural_scroll = false

# Theme Sourcing (Hot Reload Enabled)
[theme]
source = ~/.config/miquland/theme/theme_mode.conf
```

### Switching Themes Live
You can switch themes on the fly without restarting the compositor:

- **Switch to Light Mode:**
  ```bash
  echo "source = theme/light.conf" > ~/.config/miquland/theme/theme_mode.conf
  ```
- **Switch to Dark Mode:**
  ```bash
  echo "source = theme/dark.conf" > ~/.config/miquland/theme/theme_mode.conf
  ```

---

## 💡 Recommended Desktop Setup

Since Miquland focuses purely on window management, combine it with standard Wayland utilities:
- **Terminal:** `kitty`, `foot`, `alacritty`
- **Application Launcher:** `miqulauncher` (native)
- **Status Bar:** `waybar`
- **Wallpaper:** `swaybg` or `hyprpaper`
- **Notifications:** `dunst` or `mako`


