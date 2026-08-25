# biway

A lightweight binary-tiling Wayland compositor / window manager built in Modern C++ and [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots).

## 🚀 Layout Concept

`biway` enforces an intuitive two-window-per-workspace binary tiling layout:
- **1 Window Open**: Expands to 100% full screen.
- **2 Windows Open**: Splits horizontally into two equal halves (50% left, 50% right).
- **3+ Windows Open**: Automatically creates and switches to the next workspace (e.g. Workspace 2, 3...) and places the window there.
- **Window Closed**: When a window is closed in a two-window workspace, the remaining window automatically returns to full screen.

## 📦 Dependencies

On Arch Linux:
```bash
sudo pacman -S wlroots0.20 wayland wayland-protocols libxkbcommon pixman cmake ninja gcc
```

## 🛠️ Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## ⌨️ Default Keybindings

| Keybinding | Action |
| :--- | :--- |
| `Super + Return` | Launch terminal (`$TERMINAL`, `foot`, `kitty`, `alacritty`) |
| `Super + D` / `Super + Space` | Launch app menu (`fuzzel`, `wofi`, `bemenu-run`) |
| `Super + Q` / `Super + Shift + C` | Close focused window |
| `Super + H` / `Super + Left` | Focus left / previous window |
| `Super + L` / `Super + Right` / `Super + Tab` | Focus right / next window |
| `Super + [1..9]` | Switch to workspace 1 to 9 |
| `Super + Shift + [1..9]` | Move focused window to workspace 1 to 9 |
| `Super + Shift + E` | Exit biway compositor |

## 🖥️ Running

You can run `biway` nested inside an existing Wayland or X11 session for development and testing:

```bash
./build/biway
```

Or pass a startup command (e.g. launch status bar or terminal on startup):

```bash
./build/biway -s "waybar & foot"
```
