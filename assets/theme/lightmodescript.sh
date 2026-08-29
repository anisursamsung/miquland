#!/usr/bin/env bash
notify-send "Theme" "Light Mode Active"

gsettings set org.gnome.desktop.interface gtk-theme Adwaita
gsettings set org.gnome.desktop.interface color-scheme prefer-light

echo "include ~/.config/miquland/theme/kitty/kitty_light.conf" > ~/.config/miquland/theme/kitty/kitty_current.conf
pkill -SIGUSR1 kitty || true
