# Changelog

All notable changes to Aequitas are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/). Versioning follows [SemVer](https://semver.org/).

The live version string is the single line in [`VERSION`](VERSION). Build scripts read it and package artifacts into `releases/vX.Y.Z/<platform>/`.

## [Unreleased]

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
