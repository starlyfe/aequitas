#!/usr/bin/env bash
# Configure, build, test, and package Aequitas on Linux into releases/vX.Y.Z/linux/.
#
# Usage:
#   ./scripts/build-linux.sh                 # Release + package
#   ./scripts/build-linux.sh --debug         # Debug (no package)
#   ./scripts/build-linux.sh --run           # Build Release, package, launch
#   ./scripts/build-linux.sh --no-package    # Skip releases/ output
#   ./scripts/build-linux.sh --package       # Force package (e.g. with --debug)

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

DEBUG=0
RUN=0
FORCE_PACKAGE=0
NO_PACKAGE=0

for arg in "$@"; do
    case "$arg" in
        --debug)      DEBUG=1 ;;
        --run)        RUN=1 ;;
        --package)    FORCE_PACKAGE=1 ;;
        --no-package) NO_PACKAGE=1 ;;
        -h|--help)
            echo "Usage: $0 [--debug] [--run] [--package] [--no-package]"
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

read_version() {
    local v
    v="$(tr -d '[:space:]' < "$REPO_ROOT/VERSION")"
    if [[ ! "$v" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
        echo "VERSION file must contain SemVer MAJOR.MINOR.PATCH (got '$v')" >&2
        exit 1
    fi
    printf '%s' "$v"
}

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

PROJECT_VERSION="$(read_version)"
echo "Project VERSION = $PROJECT_VERSION"

if [[ -z "${VULKAN_SDK:-}" || ! -d "$VULKAN_SDK" ]]; then
    echo 'VULKAN_SDK is not set or does not exist.' >&2
    echo 'Install the LunarG Vulkan SDK from https://vulkan.lunarg.com/' >&2
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
echo "  version           : $PROJECT_VERSION"
echo "  aequitas_headless : $HEADLESS"
if [[ -f "$VISUAL" ]]; then
    echo "  aequitas          : $VISUAL"
else
    echo "  aequitas          : (not built — AEQUITAS_BUILD_RENDERER=OFF?)"
fi

SHOULD_PACKAGE=0
if [[ "$NO_PACKAGE" -eq 0 ]]; then
    if [[ "$DEBUG" -eq 0 || "$FORCE_PACKAGE" -eq 1 ]]; then
        SHOULD_PACKAGE=1
    fi
fi

if [[ "$SHOULD_PACKAGE" -eq 1 ]]; then
    step "Packaging releases/v${PROJECT_VERSION}/linux"
    chmod +x "$REPO_ROOT/scripts/package-release.sh"
    AEQUITAS_VERSION="$PROJECT_VERSION" "$REPO_ROOT/scripts/package-release.sh" linux "$BIN_DIR"
else
    echo
    echo '(Skipping releases/ package — use Release build or pass --package)'
fi

if [[ "$RUN" -eq 1 ]]; then
    if [[ ! -f "$VISUAL" ]]; then
        echo "Cannot --run: $VISUAL not found." >&2
        exit 1
    fi
    step 'Launching aequitas'
    exec "$VISUAL"
fi
