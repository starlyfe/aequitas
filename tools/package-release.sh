#!/usr/bin/env bash
# Package Release binaries into releases/v$VERSION/<platform>/ and a zip.
# Usage: package-release.sh <platform> <bin_dir>
#   platform: windows | macos | linux
# Env: AEQUITAS_VERSION overrides VERSION file (optional)

set -euo pipefail

PLATFORM="${1:?platform required (windows|macos|linux)}"
BIN_DIR="${2:?bin dir required}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="${AEQUITAS_VERSION:-$(tr -d '[:space:]' < "$REPO_ROOT/VERSION")}"

if [[ ! "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Invalid VERSION '$VERSION'" >&2
    exit 1
fi

case "$PLATFORM" in
    windows|macos|linux) ;;
    *) echo "Unknown platform: $PLATFORM" >&2; exit 1 ;;
esac

OUT_ROOT="$REPO_ROOT/releases/v${VERSION}"
OUT_DIR="$OUT_ROOT/$PLATFORM"
ZIP_PATH="$OUT_ROOT/aequitas-v${VERSION}-${PLATFORM}.zip"

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

echo "==> Packaging Aequitas v${VERSION} → $OUT_DIR"

# Copy executables (names differ on Windows)
if [[ "$PLATFORM" == "windows" ]]; then
    for name in aequitas.exe aequitas_headless.exe; do
        if [[ -f "$BIN_DIR/$name" ]]; then
            cp -f "$BIN_DIR/$name" "$OUT_DIR/"
        fi
    done
else
    for name in aequitas aequitas_headless; do
        if [[ -f "$BIN_DIR/$name" ]]; then
            cp -f "$BIN_DIR/$name" "$OUT_DIR/"
            chmod +x "$OUT_DIR/$name"
        fi
    done
fi

if [[ -d "$BIN_DIR/shaders" ]]; then
    cp -R "$BIN_DIR/shaders" "$OUT_DIR/"
fi

# Sidecar metadata
printf '%s\n' "$VERSION" > "$OUT_ROOT/VERSION.txt"
printf '%s\n' "$PLATFORM" > "$OUT_DIR/PLATFORM.txt"
date -u +"%Y-%m-%dT%H:%M:%SZ" > "$OUT_DIR/BUILT_AT.txt"

# Extract this version's changelog section into NOTES.md
python3 - <<PY || true
from pathlib import Path
import re
root = Path(r"$REPO_ROOT")
text = (root / "CHANGELOG.md").read_text(encoding="utf-8")
ver = "$VERSION"
pat = rf"## \[{re.escape(ver)}\][^\n]*\n(.*?)(?=\n## |\Z)"
m = re.search(pat, text, re.S)
notes = m.group(0).strip() + "\n" if m else f"## [{ver}]\n\n(No CHANGELOG section found.)\n"
(Path(r"$OUT_ROOT") / "NOTES.md").write_text(notes, encoding="utf-8")
PY

# Zip the platform folder
rm -f "$ZIP_PATH"
(
    cd "$OUT_ROOT"
    if command -v zip >/dev/null 2>&1; then
        zip -r "aequitas-v${VERSION}-${PLATFORM}.zip" "$PLATFORM" VERSION.txt NOTES.md >/dev/null
    else
        # Fallback: tar.gz if zip missing
        tar -czf "aequitas-v${VERSION}-${PLATFORM}.tar.gz" "$PLATFORM" VERSION.txt NOTES.md
        echo "  (zip not found — wrote .tar.gz instead)"
    fi
)

echo "  folder : $OUT_DIR"
if [[ -f "$ZIP_PATH" ]]; then
    echo "  zip    : $ZIP_PATH"
elif [[ -f "$OUT_ROOT/aequitas-v${VERSION}-${PLATFORM}.tar.gz" ]]; then
    echo "  archive: $OUT_ROOT/aequitas-v${VERSION}-${PLATFORM}.tar.gz"
fi
