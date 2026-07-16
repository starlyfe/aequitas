#!/usr/bin/env bash
# Configure, build, and test Aequitas on Linux.
#
# Usage:
#   ./scripts/build-linux.sh            # Release
#   ./scripts/build-linux.sh --debug    # Debug
#   ./scripts/build-linux.sh --run      # Build Release and launch aequitas

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

DEBUG=0
RUN=0

for arg in "$@"; do
    case "$arg" in
        --debug) DEBUG=1 ;;
        --run)   RUN=1 ;;
        -h|--help)
            echo "Usage: $0 [--debug] [--run]"
            exit 0
            ;;
        *)
            echo "Unknown argument: $arg" >&2
            exit 1
            ;;
    esac
done

APT_DEPS_HINT='sudo apt install -y build-essential cmake ninja-build libx11-dev libxrandr-dev libxi-dev libxxf86vm-dev libxinerama-dev libxcursor-dev xorg-dev libwayland-dev libxkbcommon-dev wayland-protocols'

step() { printf '\n==> %s\n' "$1"; }

# ---------------------------------------------------------------------------
# Prerequisites
# ---------------------------------------------------------------------------
step 'Checking prerequisites'

for tool in cmake g++ ninja; do
    if ! command -v "$tool" &>/dev/null; then
        echo "$tool not found on PATH." >&2
        echo "On Debian/Ubuntu: $APT_DEPS_HINT" >&2
        exit 1
    fi
done

CMAKE_VER="$(cmake --version | head -n1)"
GXX_VER="$(g++ --version | head -n1)"
echo "Found $CMAKE_VER"
echo "Found $GXX_VER"

if [[ -z "${VULKAN_SDK:-}" || ! -d "$VULKAN_SDK" ]]; then
    echo 'VULKAN_SDK is not set or does not exist.' >&2
    echo 'Install the LunarG Vulkan SDK from https://vulkan.lunarg.com/' >&2
    echo '  wget -qO- https://sdk.lunarg.com/sdk/download/latest/linux/vulkan-sdk.tar.gz | tar xz -C ~' >&2
    echo '  source ~/setup-env.sh' >&2
    exit 1
fi
echo "VULKAN_SDK = $VULKAN_SDK"

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
if [[ "$DEBUG" -eq 1 ]]; then
    PRESET='debug'
else
    PRESET='release'
fi

BUILD_DIR="$REPO_ROOT/build/$PRESET"
BIN_DIR="$BUILD_DIR/bin"

step "Configuring preset '$PRESET'"
if ! cmake --preset "$PRESET"; then
    echo >&2
    echo 'CMake configure failed. GLFW may need system headers:' >&2
    echo "  $APT_DEPS_HINT" >&2
    exit 1
fi

step "Building preset '$PRESET'"
cmake --build --preset "$PRESET"

step "Running tests (preset '$PRESET')"
ctest --preset "$PRESET"

HEADLESS="$BIN_DIR/aequitas_headless"
VISUAL="$BIN_DIR/aequitas"

step 'Build complete'
echo "  aequitas_headless : $HEADLESS"
if [[ -f "$VISUAL" ]]; then
    echo "  aequitas          : $VISUAL"
else
    echo "  aequitas          : (not built — AEQUITAS_BUILD_RENDERER=OFF?)"
fi

if [[ "$RUN" -eq 1 ]]; then
    if [[ ! -f "$VISUAL" ]]; then
        echo "Cannot --run: $VISUAL not found." >&2
        exit 1
    fi
    step 'Launching aequitas'
    exec "$VISUAL"
fi
