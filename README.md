# Aequitas

**A Vulkan-powered agent-based economic sandbox.**

Watch autonomous agents harvest resources on a hex world, trade through a central limit-order book, and adapt to the interventions you impose as a nascent sovereign. One simulation tick is one full day — dawn activity, dusk settlement, night regeneration — rendered under a sun that tracks the tick clock.

No victory screen. No defeat screen. Just emergent prices, inequality, survival, and the macro health score that summarizes whether the aquarium is thriving.

```
        ☀ dusk settlement
       / \
  🌾 agents harvest by day
       |
  📈 LOB clears at dusk
```

> **Screenshot placeholder** — add `docs/screenshot.png` after first visual build.

## Features

- **Headless-first core** (`aequitas_sim`) — deterministic, testable, stdlib-only economic engine
- **Hex world** with logistic resource regeneration across plains, forest, and mountain biomes
- **Limit-order book market** at a central hub; food, wood, stone
- **128 autonomous agents** with a heuristic policy (Q-learning and player control planned)
- **Vulkan renderer** with procedural mesh generation and day/night sun
- **ImGui + ImPlot HUD** — prices, Gini, population, sovereign shock controls
- **CI-friendly headless runner** — batch ticks, export CSV, plot offline

## Prerequisites

| Requirement | Notes |
|-------------|-------|
| **CMake ≥ 3.28** | Presets in `CMakePresets.json` |
| **C++20 compiler** | MSVC 2022, Clang 15+, or GCC 12+ |
| **[LunarG Vulkan SDK](https://vulkan.lunarg.com/)** | Headers, `glslc`, loader. Set `VULKAN_SDK` to the SDK root. |
| **Ninja** (recommended) | Faster builds; tools fall back to Visual Studio on Windows |
| **Python 3.10+** | Homebase GUI (`tkinter`) + optional `analysis/plot_run.py` |

### End-user note

Players downloading a release zip need the **Vulkan runtime** installed (bundled with most GPU drivers, or via the [LunarG SDK](https://vulkan.lunarg.com/) / vendor graphics drivers). The headless binary (`aequitas_headless`) does **not** require Vulkan.

## Quick Start

### Homebase GUI (easiest)

From the repo root, open the tools homebase:

| OS | Launch |
|----|--------|
| Windows | Double-click [`tools/AequitasHome.bat`](tools/AequitasHome.bat) |
| macOS | Double-click [`tools/AequitasHome.command`](tools/AequitasHome.command) |
| Linux | `./tools/AequitasHome.sh` |

Pick **Release / Development / Debug**, toggle **Package**, **Run after**, **Skip tests**, then hit **Build**. Details: [`tools/README.md`](tools/README.md).

### CLI

#### Windows

```powershell
git clone https://github.com/starlyfe/aequitas.git
cd aequitas
.\tools\build-windows.ps1              # Release + tests + package
.\tools\build-windows.ps1 --dev --run  # Development profile, then launch
.\tools\build-windows.ps1 --debug
```

Requires `VULKAN_SDK` (the script also probes `C:\VulkanSDK\*`).

Build tree: `build\<profile>\bin\`  
Versioned release: `releases\v<VERSION>\windows\`

#### macOS

```bash
git clone https://github.com/starlyfe/aequitas.git
cd aequitas
chmod +x tools/AequitasHome.sh tools/build-macos.sh
./tools/build-macos.sh
./tools/build-macos.sh --dev --run
```

#### Linux

```bash
git clone https://github.com/starlyfe/aequitas.git
cd aequitas
chmod +x tools/AequitasHome.sh tools/build-linux.sh
./tools/build-linux.sh
./tools/build-linux.sh --run
```

## Controls

| Input | Action |
|-------|--------|
| LMB | Select tile / agent |
| RMB drag | Pan map (hand cursor) |
| MMB drag | Orbit camera |
| Scroll | Zoom |
| WASD | Move look target |
| HUD Control | Pause, step, speed 1×/2×/4×/8×/max |
| HUD mode | Observer or Sovereign |
| Sovereign tools | Inject/drain cash; drought at selected tile |

See [docs/GAMEPLAY.md](docs/GAMEPLAY.md) for the full design.

## Repository Map

```
Aequitas/
├── src/
│   ├── sim/           # aequitas_sim — params, rng, hex, world, agent, market, telemetry
│   ├── render/        # aequitas_render — Vulkan context, mesh, pipeline, camera
│   ├── ui/            # aequitas_ui — HUD
│   └── app/           # main.cpp (visual), headless_main.cpp
├── shaders/           # lit.vert, lit.frag
├── tests/             # CTest unit tests (sim only)
├── cmake/             # deps.cmake, shaders.cmake
├── tools/             # HOMEBASE — GUI + build/package scripts (see tools/README.md)
├── analysis/          # plot_run.py — offline CSV charts
├── docs/
│   ├── ARCHITECTURE.md
│   ├── GAMEPLAY.md
│   └── DEVLOG.md
└── .github/workflows/ # CI matrix build + release zips
```

## Documentation

- [Architecture & expansion seams](docs/ARCHITECTURE.md)
- [Gameplay design](docs/GAMEPLAY.md)
- [Development log](docs/DEVLOG.md)

## Analysis

After a headless run that writes `macro.csv` and `trades.csv`:

```bash
pip install pandas matplotlib
python analysis/plot_run.py --out path/to/run/output
```

PNG charts land in the output directory.

## Roadmap (post-foundation)

- Q-learning / AR forecasting policies (`Policy` seam)
- Multi-hub markets + merchant caravans
- Firms, wages, and Participant (tycoon) mode
- Avellaneda-Stoikov market makers
- Threaded agent updates; memory-pooled LOB
- Interest-rate / OMO Sovereign levers (`CentralLedger` seam)

## License

MIT — see [LICENSE](LICENSE).

Copyright (c) 2026 Aequitas Contributors
