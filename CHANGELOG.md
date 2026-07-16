# Changelog

All notable changes to Aequitas are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/). Versioning follows [SemVer](https://semver.org/).

The live version string is the single line in [`VERSION`](VERSION). Build scripts read it and package artifacts into `releases/vX.Y.Z/<platform>/`.

## [Unreleased]

## [0.3.1] — 2026-07-16

### Added
- Depth-based SSAO with separable blur + composite (soft contact shadows on the aquarium)

### Changed
- ImGui panels default to non-overlapping corners (Control TL, Market BL, Macro TR, Inspector BR)
- Day/night is wall-clock (~3 min/cycle), independent of sim speed — smoother sky ramps
- Softer wrap lighting, hemisphere fill, and elevation-driven ambient / sun intensity

## [0.3.0] — 2026-07-16

### Added
- Renamed `scripts/` → `tools/` homebase
- Cross-platform Homebase GUI (`tools/homebase.py` + `AequitasHome.bat/.sh/.command`) with profile, package, run-after, skip-tests
- CMake `dev` preset (`RelWithDebInfo` + `AEQUITAS_DEV`) for future development-only hooks

### Fixed
- RMB pan now grabs the map (content follows the hand); prior signs were still inverted horizontally
- Hex picking raycasts tile surface heights instead of only y=0 (fixes cursor/selection offset on raised biomes)

## [0.2.1] — 2026-07-16

### Fixed
- Camera look / WASD directions were inverted (forward pointed toward the eye)
- Remapped mouse: RMB = pan (hand cursor), MMB = orbit; corrected pan/orbit drag signs
- Hex picking uses framebuffer coords + Vulkan NDC; Inspector shows hover/select axial coords

## [0.2.0] — 2026-07-15

### Added
- Single-source `VERSION` file consumed by CMake and all platform build scripts
- Versioned local release packaging: `releases/vX.Y.Z/{windows,macos,linux}/` plus per-platform zip
- `AGENTS.md` release protocol (bump version, patch notes, package) with `CLAUDE.md` import bridge for Claude Code
- Generated `aequitas_version.h` so binaries print the live version at runtime

## [0.1.0] — 2026-07-15

### Added
- Foundation: headless agent-based market simulator (hex world, LOB, 128 heuristic agents)
- Vulkan low-poly aquarium renderer + ImGui/ImPlot HUD (Observer / Sovereign)
- Cross-platform CMake presets and `scripts/build-{windows,macos,linux}`
- CTest suite (order book, conservation, determinism, parity)
- GitHub Actions matrix CI with downloadable zips on `v*` tags
