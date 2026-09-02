#!/usr/bin/env bash
set -euo pipefail

# Ensure script is run with sudo/root privileges
if [ "${EUID}" -ne 0 ]; then
    echo "==> Error: Please run this script with sudo: sudo ./uninstall.sh"
    exit 1
fi

echo "==> Uninstalling miquland from system..."

# 1. Use CMake install_manifest.txt if available
if [ -f "build/install_manifest.txt" ]; then
    echo "==> Removing installed files recorded in build/install_manifest.txt..."
    xargs rm -vf < "build/install_manifest.txt" || true
elif [ -f "build_dev/install_manifest.txt" ]; then
    echo "==> Removing installed files recorded in build_dev/install_manifest.txt..."
    xargs rm -vf < "build_dev/install_manifest.txt" || true
fi

# 2. Explicit cleanup of standard paths
FILES_TO_REMOVE=(
    "/usr/bin/miquland"
    "/usr/local/bin/miquland"
    "/usr/share/wayland-sessions/miquland.desktop"
    "/usr/local/share/wayland-sessions/miquland.desktop"
    "/usr/share/miquland/miquland.conf"
    "/usr/share/miquland/wallpaper.png"
    "/usr/local/share/miquland/miquland.conf"
    "/usr/local/share/miquland/wallpaper.png"
)

for FILE in "${FILES_TO_REMOVE[@]}"; do
    if [ -f "$FILE" ]; then
        echo "Removing $FILE..."
        rm -f "$FILE"
    fi
done

# Remove data directories if present
for DIR in "/usr/share/miquland" "/usr/local/share/miquland"; do
    if [ -d "$DIR" ]; then
        rm -rf "$DIR"
        echo "Removed directory $DIR"
    fi
done

echo "==> miquland system uninstallation finished successfully!"

# 3. Optional purge for user config directory
if [ "${1:-}" == "--purge" ] || [ "${1:-}" == "-p" ]; then
    TARGET_USER="${SUDO_USER:-$USER}"
    USER_HOME=$(getent passwd "$TARGET_USER" | cut -d: -f6)
    if [ -d "$USER_HOME/.config/miquland" ]; then
        echo "==> Purging user configuration at $USER_HOME/.config/miquland..."
        rm -rf "$USER_HOME/.config/miquland"
    fi
else
    echo "==> Note: User configuration at ~/.config/miquland was preserved."
    echo "    (To also remove user configs, run: sudo ./uninstall.sh --purge)"
fi
