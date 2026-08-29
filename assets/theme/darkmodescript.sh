#!/usr/bin/env bash
notify-send "Theme" "Dark Mode Active"

gsettings set org.gnome.desktop.interface gtk-theme Adwaita-dark
gsettings set org.gnome.desktop.interface color-scheme prefer-dark

echo "include ~/.config/miquland/theme/kitty/kitty_dark.conf" > ~/.config/miquland/theme/kitty/kitty_current.conf
pkill -SIGUSR1 kitty || true
