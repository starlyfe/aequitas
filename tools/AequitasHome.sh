#!/usr/bin/env bash
# Launch the Aequitas Homebase GUI (Linux / macOS terminal).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

pick_python() {
    for cand in python3 python; do
        if command -v "$cand" >/dev/null 2>&1; then
            if "$cand" -c "import tkinter" 2>/dev/null; then
                echo "$cand"
                return 0
            fi
        fi
    done
    return 1
}

if ! PY="$(pick_python)"; then
    echo "Python 3 with tkinter is required." >&2
    echo "  Debian/Ubuntu: sudo apt install python3-tk" >&2
    echo "  macOS: brew install python-tk  (or use python.org installer)" >&2
    exit 1
fi

exec "$PY" "$ROOT/tools/homebase.py"
