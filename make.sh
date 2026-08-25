#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build"
BUILD_TYPE="${BUILD_TYPE:-Release}"

echo "==> Configuring biway ($BUILD_TYPE)..."
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_INSTALL_PREFIX="/usr" "$@"

echo "==> Building biway..."
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "==> Build finished successfully! Binary is at $BUILD_DIR/biway"

# If invoked with sudo/root, automatically install to system
if [ "${EUID}" -eq 0 ]; then
    echo "==> Installing biway to system (/usr/bin, /usr/share/backgrounds/biway, /usr/share/wayland-sessions)..."
    cmake --install "$BUILD_DIR"

    TARGET_USER="${SUDO_USER:-}"
    if [ -n "$TARGET_USER" ] && [ "$TARGET_USER" != "root" ]; then
        USER_HOME=$(getent passwd "$TARGET_USER" | cut -d: -f6)
        if [ -n "$USER_HOME" ]; then
            CONFIG_DIR="$USER_HOME/.config/biway"
            CONFIG_FILE="$CONFIG_DIR/biway.conf"
            if [ ! -f "$CONFIG_FILE" ]; then
                echo "==> Setting up default user configuration at $CONFIG_FILE..."
                mkdir -p "$CONFIG_DIR"
                cp "assets/biway.conf" "$CONFIG_FILE"
                chown -R "$TARGET_USER:$(id -gn "$TARGET_USER")" "$CONFIG_DIR"
            fi
        fi
    fi

    echo "==> biway successfully installed! You can select 'biway' in your display manager or launch it via 'biway'."
fi
