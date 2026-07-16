# tools/ — Aequitas homebase

This folder is the single place to **build, package, and launch** Aequitas.

## GUI (recommended)

| OS | How to open |
|----|-------------|
| Windows | Double-click `AequitasHome.bat` |
| macOS | Double-click `AequitasHome.command` (or run `./AequitasHome.sh`) |
| Linux | `./AequitasHome.sh` |

Requires **Python 3** with **tkinter** (usually included). On Debian/Ubuntu: `sudo apt install python3-tk`.

The GUI lets you pick:

- **Profile** — Release / Development / Debug
- **Package into `releases/vX.Y.Z/`** — on by default for Release
- **Run after build**
- **Skip tests**

Development maps to CMake preset `dev` (`RelWithDebInfo` + `AEQUITAS_DEV=1`) so you can hang extra diagnostics on that flag later without touching Release.

## CLI (same options)

```powershell
# Windows
.\tools\build-windows.ps1
.\tools\build-windows.ps1 --dev --run
.\tools\build-windows.ps1 --debug --package --no-test
```

```bash
# macOS / Linux
./tools/build-macos.sh --dev --run
./tools/build-linux.sh --package
```

Flags: `--dev` · `--debug` · `--run` · `--package` · `--no-package` · `--no-test`

## Layout

| File | Role |
|------|------|
| `homebase.py` | Cross-platform build GUI |
| `AequitasHome.*` | OS launchers for the GUI |
| `build-windows.ps1` / `build-macos.sh` / `build-linux.sh` | Platform build scripts |
| `package-release.ps1` / `package-release.sh` | Copy binaries into `releases/vX.Y.Z/` |
