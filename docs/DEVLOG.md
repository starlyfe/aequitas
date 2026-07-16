# Aequitas Development Log

> **Protocol:** This file is **append-only**. Add new entries at the bottom. Do not rewrite or delete prior entries ? they are the project record.

---

## Phase 0 ? 2026-07-15

**Repo scaffold created.**

Initial CMake project targeting C++20 with a headless-first layout:

- `aequitas_sim` ? economic core (stdlib only)
- `aequitas_render` ? Vulkan presentation layer
- `aequitas_ui` ? ImGui/ImPlot HUD
- `aequitas` ? interactive app
- `aequitas_headless` ? CLI batch runner
- `tests/` ? order book, conservation, determinism, parity

Build system: CMake ? 3.28, presets in `CMakePresets.json` (`debug`, `release`, `ci`, `windows-vs`). Dependencies fetched via `cmake/deps.cmake` (FetchContent).

**ImPlot pinned** to commit `d65a2bef53d32502407de3a4be80f191e2f412d7` (master tip compatible with ImGui 1.92+). Tagged ImPlot v1.0 predates the ImGui 1.92 API break; the SHA is recorded here and in `deps.cmake`.

**Pinned dependency versions:**

| Package | Version / Ref |
|---------|---------------|
| GLFW | 3.4 |
| volk | 1.4.350 |
| vk-bootstrap | v1.4.350 |
| Vulkan Memory Allocator | v3.4.0 |
| Dear ImGui | v1.92.8 |
| ImPlot | `d65a2bef?` (see above) |
| GLM | 1.0.1 |

**Docs and tooling added:** `ARCHITECTURE.md`, `GAMEPLAY.md`, `README.md`, platform build scripts, GitHub Actions CI workflow, `analysis/plot_run.py` for offline CSV charts.

---

## Phase 1 ? 2026-07-15

**Headless economic core complete.**

- Hex disc world (r=16), logistic regen, FOOD/WOOD/STONE biomes
- 128 agents with `HeuristicPolicy` behind the `Policy` seam (forage-first survival, opportunistic hub trading)
- Price-time-priority LOB with escrow, partial fills, expiry refunds
- Tick pipeline: regen ? metabolism ? decisions ? move/harvest ? orders ? match ? expire ? telemetry
- Exact money conservation (agent cash + escrow + CentralLedger == money supply)
- CSV export (`macro.csv`, `trades.csv`, `events.csv`) + FNV-1a `state_hash()`
- Tests: order_book, conservation (10k ticks), determinism, parity ? all green

**Tuning (homeostasis):** `HUNGER_EAT_AT=15`, `HUNGER_MAX=80`, `INITIAL_FOOD=12`, `HARVEST_AMOUNT=2`, `K_PLAINS=14`, `REGEN_R=0.30`. Sample run: `--ticks 20000 --seed 42` ? pop=128 entire run, ~11.7k trades, gini?0.22, money check PASS. Headless ?3k+ ticks/sec (Release, this machine).

**Deviation:** Starvation check stays in metabolism (pre-decision); ecological load is controlled via eat frequency rather than deferred death. Early attempts to force hub travel while food-poor caused extinction.

---

## Phase 2 ? 2026-07-15

**Vulkan foundation up.**

- volk + vk-bootstrap + VMA context, Vulkan 1.3 dynamic rendering, 2 frames in flight
- Swapchain + depth, resize recreate, validation in Debug
- Fixed-timestep sim driver (1 tick/sec ? speed multiplier; max = 4 ms budget)
- ImGui 1.92.8 + ImPlot (SHA `d65a2bef?`) with `IMGUI_IMPL_VULKAN_USE_VOLK`

---

## Phase 3 ? 2026-07-15

**Living map.**

- Procedural low-poly meshes: hex prism, tree, rock, wheat, meeple, hub banner (vertex-colored, flat-shaded)
- Single lit instanced pipeline (`shaders/lit.vert/.frag`)
- Baked terrain; per-frame prop/agent instances from sim state (stock density thins forests)
- Orbit camera + hex picking; day/night sun + sky clear from tick fraction

---

## Phase 4 ? 2026-07-15

**HUD & Sovereign.**

- Control / Inspector / Market / Macro ImGui windows (dark theme)
- ImPlot depth stairstep, price series, macro lines (Gini, money, population, price index)
- Observer | Sovereign modes; cash inject/drain; drought at selected tile
- ImGui capture flags gate camera/picking
- **Fix during live test:** ImGui draws in its own dynamic-rendering pass (LOAD) after the 3D pass ends

---

## Phase 5 ? 2026-07-15

**Polish & release readiness.**

- `test_parity` guards headless/app sim hash equivalence (frontend must not mutate sim except via commands)
- README controls table + roadmap; `docs/ARCHITECTURE.md` expansion seams
- CI workflow uploads `aequitas-{windows,linux,macos}.zip`; tag `v*` publishes GitHub Release
- GUI smoke-tested ~8s on Windows Release; all 4 CTest targets pass
- Offline plots: `analysis/plot_run.py --out out` ? `price_series.png`, `gini.png`, `wealth_population.png`

**Known issues / follow-ups:**

- GCC `-Wmissing-field-initializers` noise on Vulkan structs (warnings-as-errors only on `ci` preset; release scripts leave it OFF)
- End users need a Vulkan runtime; headless does not
- Participant mode, Q-learning, multi-hub caravans, firms ? deferred behind documented seams

**v0.1.0 foundation complete.** Fork ? `scripts/build-*.ps1|.sh` ? watch the aquarium.

---

## Versioned releases ? 2026-07-15

**Packaging + agent release protocol (v0.2.0).**

- Single-source `VERSION` file ? CMake `PROJECT_VERSION` + generated `aequitas_version.h`
- Release builds package into `releases/vX.Y.Z/<platform>/` and `aequitas-vX.Y.Z-<platform>.zip`
- `AGENTS.md` mandates SemVer bump + `CHANGELOG.md` on shippable work; `CLAUDE.md` imports it for Claude Code (`@AGENTS.md`)
- Binary payloads under `releases/v*/` remain gitignored; history is `VERSION` + changelog + git tags / GitHub Releases

---

## Camera / picking fix ? 2026-07-16

**v0.2.1.** Corrected ground-forward (WASD + pan), remapped RMB=pan / MMB=orbit with natural drag signs, hex pick uses framebuffer + Vulkan NDC, Inspector shows hover/select `q,r`.

---

## Homebase GUI ? 2026-07-16

**v0.3.0.** Renamed `scripts/` to `tools/` as the universal homebase. Added `tools/homebase.py` (tkinter) plus `AequitasHome.bat` / `.sh` / `.command` launchers with selectors for Release / Development / Debug, package into `releases/vX.Y.Z/`, run after build, and skip tests. CMake `dev` preset (`RelWithDebInfo` + `AEQUITAS_DEV`) for future development-only hooks.

**Input polish:** RMB pan uses true grab-the-map signs; picking iterates ray ? tile-top planes via `tile_surface_height` so hover/select matches raised hexes.

---

## Visual polish ? 2026-07-16

**v0.3.1.** Corner ImGui layout (Control TL / Market BL / Macro TR / Inspector BR). Depth SSAO post stack (`PostFx`). Wall-clock day cycle (~180s) with softer sky, wrap lighting, and elevation-driven ambient ? decoupled from sim tick speed.


---

## Visual polish — 2026-07-16

**v0.3.1.** Corner ImGui layout (Control TL / Market BL / Macro TR / Inspector BR). Depth SSAO post stack (`PostFx`). Wall-clock day cycle (~180s) with softer sky, wrap lighting, and elevation-driven ambient — decoupled from sim tick speed.
