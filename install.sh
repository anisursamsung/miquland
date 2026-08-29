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

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$(dirname "$SCRIPT_DIR")"

# Locate repo directories
if [ -d "$SCRIPT_DIR/src/core" ]; then
    MIQULAND_DIR="$SCRIPT_DIR"
    TOOLKIT_DIR="$BASE_DIR/miqutoolkit"
    LAUNCHER_DIR="$BASE_DIR/miqulauncher"
else
    MIQULAND_DIR="$SCRIPT_DIR/miquland"
    TOOLKIT_DIR="$SCRIPT_DIR/miqutoolkit"
    LAUNCHER_DIR="$SCRIPT_DIR/miqulauncher"
fi

echo "=========================================================="
echo "  Miquland Ecosystem Installation & System Cleanup"
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

ldconfig || true
echo "    ✓ Legacy biway system artifacts cleaned."

# ------------------------------------------------------------------------------
# STEP 2: Build & Install Miqutoolkit
# ------------------------------------------------------------------------------
echo ""
echo "==> [2/4] Building and installing miqutoolkit (Declarative C++20 UI Library)..."
if [ -d "$TOOLKIT_DIR" ]; then
    cd "$TOOLKIT_DIR"
    rm -rf build
    cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build -j"$(nproc)"
    cmake --install build
    ldconfig || true
    echo "    ✓ miqutoolkit successfully installed to /usr/lib and /usr/include/miqutoolkit."
else
    echo "    ✗ ERROR: Directory $TOOLKIT_DIR not found!" >&2
    exit 1
fi

# ------------------------------------------------------------------------------
# STEP 3: Build & Install Miqulauncher
# ------------------------------------------------------------------------------
echo ""
echo "==> [3/4] Building and installing miqulauncher (Application Menu Launcher)..."
if [ -d "$LAUNCHER_DIR" ]; then
    cd "$LAUNCHER_DIR"
    rm -rf build
    cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build -j"$(nproc)"
    cmake --install build
    echo "    ✓ miqulauncher successfully installed to /usr/bin/miqulauncher."
else
    echo "    ✗ ERROR: Directory $LAUNCHER_DIR not found!" >&2
    exit 1
fi

# ------------------------------------------------------------------------------
# STEP 4: Build & Install Miquland Compositor
# ------------------------------------------------------------------------------
echo ""
echo "==> [4/4] Building and installing miquland (Wayland Compositor)..."
if [ -d "$MIQULAND_DIR" ]; then
    cd "$MIQULAND_DIR"
    rm -rf build
    cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build -j"$(nproc)"
    cmake --install build
    echo "    ✓ miquland successfully installed to /usr/bin/miquland."
    echo "    ✓ Assets & default themes installed to /usr/share/miquland/."
    echo "    ✓ Session entry installed to /usr/share/wayland-sessions/miquland.desktop."
else
    echo "    ✗ ERROR: Directory $MIQULAND_DIR not found!" >&2
    exit 1
fi

echo ""
echo "=========================================================="
echo "  🎉 Miquland ecosystem successfully installed!"
echo "=========================================================="
echo "  • Compositor: /usr/bin/miquland"
echo "  • Launcher:   /usr/bin/miqulauncher"
echo "  • Toolkit:    /usr/lib/libmiqutoolkit.so"
echo "  • Assets:     /usr/share/miquland/"
echo "  • Session:    Select 'Miquland' in your Display Manager"
echo "=========================================================="
