# releases/

Versioned build outputs land here when you run a **Release** platform build script.

```
releases/
  v0.2.0/
    VERSION.txt
    NOTES.md              # changelog section for this version
    windows/              # filled on Windows builds
      aequitas.exe
      aequitas_headless.exe
      shaders/
    macos/                # filled on macOS builds
      aequitas
      aequitas_headless
      shaders/
    linux/                # filled on Linux builds
      aequitas
      aequitas_headless
      shaders/
    aequitas-v0.2.0-windows.zip
    aequitas-v0.2.0-macos.zip
    aequitas-v0.2.0-linux.zip
```

## How it works

1. Agents (and humans) bump the single-line [`VERSION`](../VERSION) file and edit [`CHANGELOG.md`](../CHANGELOG.md) when shipping a feature — see [`AGENTS.md`](../AGENTS.md).
2. `scripts/build-windows.ps1` / `build-macos.sh` / `build-linux.sh` read `VERSION`, build Release, then copy binaries into `releases/v$VERSION/<platform>/` and write a zip beside it.
3. Debug builds (`--debug`) do **not** package into `releases/` (use `--package` to force).

## Git policy

Binary payloads under `releases/v*/` are **gitignored** (they are large and platform-specific). What *is* tracked:

- this README
- `VERSION` + `CHANGELOG.md` (the version history)
- git tags `vX.Y.Z` and GitHub Release assets from CI

Keep local `releases/` on disk as your personal time capsule of runnable builds; share via GitHub Releases when you push a tag.
