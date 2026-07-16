#!/usr/bin/env bash
# Configure, build, test, and package Aequitas on macOS into releases/vX.Y.Z/macos/.
#
# Usage:
#   ./scripts/build-macos.sh                 # Release + package
#   ./scripts/build-macos.sh --debug         # Debug (no package)
#   ./scripts/build-macos.sh --run           # Build Release, package, launch
#   ./scripts/build-macos.sh --no-package    # Skip releases/ output
#   ./scripts/build-macos.sh --package       # Force package (e.g. with --debug)
#
# Supports Apple Silicon (arm64) and Intel (x86_64) natively.

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

if ! xcode-select -p &>/dev/null; then
    echo 'Xcode Command Line Tools not found.' >&2
    echo 'Install with: xcode-select --install' >&2
    exit 1
fi

if ! command -v cmake &>/dev/null; then
    echo 'CMake not found. Install via Homebrew: brew install cmake' >&2
    exit 1
fi

CMAKE_VER="$(cmake --version | head -n1)"
echo "Found $CMAKE_VER"

if ! command -v ninja &>/dev/null; then
    echo 'Ninja not found. Install via Homebrew: brew install ninja' >&2
    exit 1
fi

PROJECT_VERSION="$(read_version)"
echo "Project VERSION = $PROJECT_VERSION"

resolve_vulkan_sdk() {
    if [[ -n "${VULKAN_SDK:-}" && -d "$VULKAN_SDK" ]]; then
        echo "VULKAN_SDK = $VULKAN_SDK"
        return 0
    fi

    local candidates=(
        "/usr/local/share/vulkan/sdk"
        "/opt/homebrew/share/vulkan/sdk"
        "$HOME/VulkanSDK"
    )

    for base in "${candidates[@]}"; do
        if [[ -d "$base" ]]; then
            local latest
            latest="$(find "$base" -maxdepth 1 -mindepth 1 -type d 2>/dev/null | sort -V | tail -n1)"
            if [[ -n "$latest" && -f "$latest/macOS/lib/libMoltenVK.dylib" ]]; then
                export VULKAN_SDK="$latest"
                echo "Auto-detected VULKAN_SDK = $VULKAN_SDK"
                return 0
            fi
        fi
    done

    echo 'VULKAN_SDK is not set and MoltenVK was not found.' >&2
    echo 'Install the LunarG macOS Vulkan SDK from https://vulkan.lunarg.com/' >&2
    exit 1
}

resolve_vulkan_sdk

ARCH="$(uname -m)"
echo "Building for $ARCH (Apple Silicon and Intel supported)"

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
cmake --preset "$PRESET"

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
    step "Packaging releases/v${PROJECT_VERSION}/macos"
    chmod +x "$REPO_ROOT/scripts/package-release.sh"
    AEQUITAS_VERSION="$PROJECT_VERSION" "$REPO_ROOT/scripts/package-release.sh" macos "$BIN_DIR"
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
