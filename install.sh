#!/usr/bin/env bash
set -euo pipefail

# ==============================================================================
# Miquland Ecosystem: Migration & Full System Installation Script
# Cleans legacy 'biway' artifacts and builds/installs Miquland, Miqutoolkit,
# and Miqulauncher cleanly to /usr.
# ==============================================================================

# Ensure root privileges
if [ "${EUID}" -ne 0 ]; then
    echo "==> Elevating privileges with sudo..."
    exec sudo bash "$0" "$@"
fi

TARGET_USER="${SUDO_USER:-$(logname 2>/dev/null || echo "$USER")}"
TARGET_HOME="$(getent passwd "$TARGET_USER" | cut -d: -f6)"

echo "=========================================================="
echo "  Miquland Ecosystem Installation & System Cleanup"
echo "  Target User: $TARGET_USER ($TARGET_HOME)"
echo "=========================================================="

# ------------------------------------------------------------------------------
# STEP 1: Remove all legacy 'biway' files and binaries from system
# ------------------------------------------------------------------------------
echo ""
echo "==> [1/4] Purging legacy 'biway' binaries, libraries, and desktop entries..."

rm -f /usr/bin/biway /usr/local/bin/biway
rm -f /usr/bin/biwaymenu /usr/local/bin/biwaymenu
rm -f /usr/lib/libbiwaytoolkit* /usr/local/lib/libbiwaytoolkit*
rm -rf /usr/include/biwaytoolkit /usr/local/include/biwaytoolkit
rm -f /usr/lib/pkgconfig/biwaytoolkit.pc /usr/local/lib/pkgconfig/biwaytoolkit.pc /usr/share/pkgconfig/biwaytoolkit.pc
rm -rf /usr/share/biway /usr/local/share/biway /etc/biway
rm -f /usr/share/wayland-sessions/biway.desktop

# Clean legacy user-local caches and symlinks
rm -f "$TARGET_HOME/biway" 2>/dev/null || true
rm -rf "$TARGET_HOME/.config/biway" 2>/dev/null || true
rm -f "$TARGET_HOME/.local/bin/biway"* 2>/dev/null || true
rm -f "$TARGET_HOME/.local/lib/libbiway"* 2>/dev/null || true
rm -rf "$TARGET_HOME/.local/include/biway"* 2>/dev/null || true
rm -f "$TARGET_HOME/.local/lib/pkgconfig/biway"* 2>/dev/null || true

ldconfig || true
echo "    ✓ Legacy biway artifacts completely cleaned."

# ------------------------------------------------------------------------------
# STEP 2: Build & Install Miqutoolkit
# ------------------------------------------------------------------------------
echo ""
echo "==> [2/4] Building and installing miqutoolkit (Declarative C++20 UI Library)..."
if [ -d "$TARGET_HOME/miqutoolkit" ]; then
    cd "$TARGET_HOME/miqutoolkit"
    rm -rf build
    cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build -j"$(nproc)"
    cmake --install build
    ldconfig || true
    echo "    ✓ miqutoolkit successfully installed to /usr/lib and /usr/include/miqutoolkit."
else
    echo "    ✗ ERROR: Directory $TARGET_HOME/miqutoolkit not found!" >&2
    exit 1
fi

# ------------------------------------------------------------------------------
# STEP 3: Build & Install Miqulauncher
# ------------------------------------------------------------------------------
echo ""
echo "==> [3/4] Building and installing miqulauncher (Application Menu Launcher)..."
if [ -d "$TARGET_HOME/miqulauncher" ]; then
    cd "$TARGET_HOME/miqulauncher"
    rm -rf build
    cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build -j"$(nproc)"
    cmake --install build
    echo "    ✓ miqulauncher successfully installed to /usr/bin/miqulauncher."
else
    echo "    ✗ ERROR: Directory $TARGET_HOME/miqulauncher not found!" >&2
    exit 1
fi

# ------------------------------------------------------------------------------
# STEP 4: Build & Install Miquland Compositor
# ------------------------------------------------------------------------------
echo ""
echo "==> [4/4] Building and installing miquland (Wayland Compositor)..."
if [ -d "$TARGET_HOME/miquland" ]; then
    cd "$TARGET_HOME/miquland"
    rm -rf build
    cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build -j"$(nproc)"
    cmake --install build
    echo "    ✓ miquland successfully installed to /usr/bin/miquland."
    echo "    ✓ Session entry installed to /usr/share/wayland-sessions/miquland.desktop."
else
    echo "    ✗ ERROR: Directory $TARGET_HOME/miquland not found!" >&2
    exit 1
fi

# ------------------------------------------------------------------------------
# User Configuration Setup
# ------------------------------------------------------------------------------
CONFIG_DIR="$TARGET_HOME/.config/miquland"
if [ ! -f "$CONFIG_DIR/miquland.conf" ]; then
    echo ""
    echo "==> Initializing default user configuration at $CONFIG_DIR..."
    mkdir -p "$CONFIG_DIR"
    cp -r "$TARGET_HOME/miquland/assets/"* "$CONFIG_DIR/"
    chown -R "$TARGET_USER:$TARGET_USER" "$CONFIG_DIR"
    echo "    ✓ Default configuration installed to $CONFIG_DIR"
fi

echo ""
echo "=========================================================="
echo "  🎉 Miquland ecosystem successfully installed!"
echo "=========================================================="
echo "  • Compositor: /usr/bin/miquland"
echo "  • Launcher:   /usr/bin/miqulauncher"
echo "  • Toolkit:    /usr/lib/libmiqutoolkit.so"
echo "  • Config:     $CONFIG_DIR/miquland.conf"
echo "  • Session:    Select 'Miquland' in your Display Manager"
echo "=========================================================="
