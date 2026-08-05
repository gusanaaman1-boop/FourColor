#!/usr/bin/env bash
# FOUR COLOR - macOS build. Mirrors scripts/build-windows.bat step for step so a
# failure on one platform can be reproduced on the other.
#
#   scripts/build-macos.sh [Release|Debug|both] [--clean] [--install]
#
# Installing into ~/Library/Audio/Plug-Ins is opt-in: a build must never touch
# the system plug-in folders unless it was asked to.

set -euo pipefail

cd "$(dirname "$0")/.."
ROOT="$PWD"

CONFIGS="Release"
CLEAN=0
INSTALL=OFF

for arg in "$@"; do
    case "$arg" in
        Release|Debug) CONFIGS="$arg" ;;
        both)          CONFIGS="Release Debug" ;;
        --clean)       CLEAN=1 ;;
        --install)     INSTALL=ON ;;
        *) echo "unknown argument: $arg" >&2; exit 2 ;;
    esac
done

for CONFIG in $CONFIGS; do
    if [ "$CONFIG" = "Release" ]; then BUILD_DIR="build"; else BUILD_DIR="build-debug"; fi

    if [ "$CLEAN" = "1" ]; then
        echo "== removing $BUILD_DIR"
        rm -rf "$BUILD_DIR"
    fi

    echo "== configure $CONFIG -> $BUILD_DIR"
    cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CONFIG" \
          -DFOURCOLOR_COPY_AFTER_BUILD="$INSTALL" \
          -DFOURCOLOR_WERROR=ON

    echo "== build $CONFIG"
    cmake --build "$BUILD_DIR" --config "$CONFIG" -j "$(sysctl -n hw.ncpu)"

    echo "== tests $CONFIG"
    "$BUILD_DIR/FourColorTests_artefacts/$CONFIG/FourColorTests"
done

echo
echo "Artefacts:"
find "$ROOT/build" -maxdepth 4 \( -name "FourColor.vst3" -o -name "FourColor.component" \
     -o -name "FourColor.app" \) 2>/dev/null || true
