#!/usr/bin/env bash
# Configure, build, test, and optionally package Aequitas on Linux.
#
# Usage:
#   ./tools/build-linux.sh
#   ./tools/build-linux.sh --dev --run
#   ./tools/build-linux.sh --debug --package
#   ./tools/build-linux.sh --no-package --no-test

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

DEBUG=0
DEV=0
RUN=0
FORCE_PACKAGE=0
NO_PACKAGE=0
NO_TEST=0

for arg in "$@"; do
    case "$arg" in
        --debug)      DEBUG=1 ;;
        --dev)        DEV=1 ;;
        --run)        RUN=1 ;;
        --package)    FORCE_PACKAGE=1 ;;
        --no-package) NO_PACKAGE=1 ;;
        --no-test)    NO_TEST=1 ;;
        -h|--help)
            echo "Usage: $0 [--release|--debug|--dev] [--run] [--package] [--no-package] [--no-test]"
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

step 'Checking prerequisites'
for tool in cmake g++ ninja; do
    if ! command -v "$tool" &>/dev/null; then
        echo "$tool not found on PATH." >&2
        echo "On Debian/Ubuntu: $APT_DEPS_HINT" >&2
        exit 1
    fi
done

PROJECT_VERSION="$(read_version)"
echo "Project VERSION = $PROJECT_VERSION"

if [[ -z "${VULKAN_SDK:-}" || ! -d "$VULKAN_SDK" ]]; then
    echo 'VULKAN_SDK is not set or does not exist.' >&2
    exit 1
fi
echo "VULKAN_SDK = $VULKAN_SDK"

if [[ "$DEV" -eq 1 ]]; then
    PRESET='dev'
    IS_RELEASE_LIKE=0
elif [[ "$DEBUG" -eq 1 ]]; then
    PRESET='debug'
    IS_RELEASE_LIKE=0
else
    PRESET='release'
    IS_RELEASE_LIKE=1
fi

BUILD_DIR="$REPO_ROOT/build/$PRESET"
BIN_DIR="$BUILD_DIR/bin"

step "Configuring preset '$PRESET'"
if ! cmake --preset "$PRESET"; then
    echo "CMake configure failed. GLFW may need: $APT_DEPS_HINT" >&2
    exit 1
fi

step "Building preset '$PRESET'"
cmake --build --preset "$PRESET"

if [[ "$NO_TEST" -eq 0 ]]; then
    step "Running tests (preset '$PRESET')"
    ctest --preset "$PRESET"
else
    echo
    echo '(Skipping tests)'
fi

HEADLESS="$BIN_DIR/aequitas_headless"
VISUAL="$BIN_DIR/aequitas"

step 'Build complete'
echo "  version           : $PROJECT_VERSION"
echo "  profile           : $PRESET"
echo "  aequitas_headless : $HEADLESS"
if [[ -f "$VISUAL" ]]; then
    echo "  aequitas          : $VISUAL"
else
    echo "  aequitas          : (not built - AEQUITAS_BUILD_RENDERER=OFF?)"
fi

SHOULD_PACKAGE=0
if [[ "$NO_PACKAGE" -eq 0 ]]; then
    if [[ "$IS_RELEASE_LIKE" -eq 1 || "$FORCE_PACKAGE" -eq 1 ]]; then
        SHOULD_PACKAGE=1
    fi
fi

if [[ "$SHOULD_PACKAGE" -eq 1 ]]; then
    step "Packaging releases/v${PROJECT_VERSION}/linux"
    chmod +x "$REPO_ROOT/tools/package-release.sh"
    AEQUITAS_VERSION="$PROJECT_VERSION" "$REPO_ROOT/tools/package-release.sh" linux "$BIN_DIR"
else
    echo
    echo '(Skipping releases/ package - use Release, or pass --package)'
fi

if [[ "$RUN" -eq 1 ]]; then
    if [[ ! -f "$VISUAL" ]]; then
        echo "Cannot --run: $VISUAL not found." >&2
        exit 1
    fi
    step 'Launching aequitas'
    exec "$VISUAL"
fi
