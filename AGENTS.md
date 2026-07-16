# Aequitas — Agent Instructions

Canonical project instructions for AI coding agents (**Cursor, Codex, Claude Code, Copilot, Gemini, Windsurf**, etc.).

- **Codex / Cursor / most tools:** read this file (`AGENTS.md`) natively.
- **Claude Code:** loads [`CLAUDE.md`](CLAUDE.md), which imports this file via `@AGENTS.md` (Windows-safe; no symlink required).

## Project snapshot

Aequitas is a C++20 / Vulkan agent-based economic sandbox. Headless sim (`aequitas_sim`) is stdlib-only; renderer/UI never mutate sim state except through `Simulation` commands. See `docs/ARCHITECTURE.md` and `docs/GAMEPLAY.md`.

Ponytail: YAGNI ladder before new code. Do not invent ECS, job systems, asset pipelines, or Q-learning unless the user asks.

## Version & release protocol (mandatory on feature work)

Whenever you **finish a user-visible feature, fix, or tooling change that should be shippable**, do this before declaring done:

### 1. Decide the SemVer bump

Read the single line in [`VERSION`](VERSION) (e.g. `0.2.0`).

| Change kind | Bump |
|-------------|------|
| Breaking API / save format / CLI flags | **MAJOR** (`1.0.0`) |
| New feature, mode, system, packaging capability | **MINOR** (`0.3.0`) |
| Bugfix, tuning, docs-only polish that ships | **PATCH** (`0.2.1`) |

Do **not** bump for pure WIP / exploratory commits the user asked not to release.

### 2. Update version files (same change set)

1. Write the new version as the only line in `VERSION` (no `v` prefix, no whitespace noise).
2. Move items from `## [Unreleased]` in [`CHANGELOG.md`](CHANGELOG.md) into a new `## [X.Y.Z] — YYYY-MM-DD` section (Added / Changed / Fixed / Removed as needed).
3. Append a short entry to [`docs/DEVLOG.md`](docs/DEVLOG.md) (append-only).
4. CMake and the apps pick up the version automatically from `VERSION` via `aequitas_version.h` — do **not** hardcode version strings in `main.cpp` / `headless_main.cpp`.

### 3. Build settings / package target

Build scripts read `VERSION` and package Release artifacts into:

```text
releases/vX.Y.Z/<windows|macos|linux>/
releases/vX.Y.Z/aequitas-vX.Y.Z-<platform>.zip
```

- Default **Release** builds **do** package.
- `--debug` builds do **not** package unless `--package` is passed.
- `--no-package` skips packaging even on Release.

After bumping, tell the user to open **tools/AequitasHome** (GUI) or run:

```powershell
.\tools\build-windows.ps1
```

```bash
./tools/build-macos.sh
# or
./tools/build-linux.sh
```

Prefer the homebase GUI when demonstrating build options (profile / package / run).
### 4. Git tag (when the user wants to publish)

Only when the user asks to tag/release:

```bash
git tag -a "vX.Y.Z" -m "Aequitas vX.Y.Z"
# push tag → CI attaches platform zips to the GitHub Release
```

Tag name must match `VERSION` with a `v` prefix.

## Build & test expectations

- Prefer the platform scripts over raw cmake for end-to-end verification.
- Run `ctest` (scripts already do) after sim changes.
- Keep `aequitas_sim` free of Vulkan/GLFW/ImGui includes.

## Do not

- Commit large binaries under `releases/v*/` (gitignored); version history lives in `VERSION` + `CHANGELOG.md` + tags.
- Duplicate version numbers across files — `VERSION` is the only source of truth.
- Skip the changelog when bumping.
