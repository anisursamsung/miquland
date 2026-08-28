#!/usr/bin/env bash
notify-send "Theme" "Light Mode Active"
gsettings set org.gnome.desktop.interface gtk-theme Adwaita
gsettings set org.gnome.desktop.interface color-scheme prefer-light


# kwriteconfig6 --file ~/.config/dolphinrc --group UiSettings --key ColorScheme BreezeLight


echo "include ~/.config/biway/theme/kitty/kitty_light.conf" > ~/.config/biway/theme/kitty/kitty_current.conf
pkill -SIGUSR1 kitty || true
