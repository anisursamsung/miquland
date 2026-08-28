notify-send "Theme" "Dark Mode Active"

gsettings set org.gnome.desktop.interface gtk-theme Adwaita-dark
gsettings set org.gnome.desktop.interface color-scheme prefer-dark

#  kwriteconfig6 --file ~/.config/dolphinrc --group UiSettings --key ColorScheme BreezeDark


echo "include ~/.config/biway/theme/kitty/kitty_dark.conf" > ~/.config/biway/kitty/kitty_current.conf
