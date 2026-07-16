# Aequitas Architecture

Aequitas is a C++20 agent-based economic sandbox. The codebase is split into a **headless simulation core** (`aequitas_sim`) and optional **presentation layers** (Vulkan renderer, ImGui HUD). The simulation never depends on graphics or windowing libraries.

## Module Map

```
┌─────────────────────────────────────────────────────────────────┐
│  apps/                                                          │
│    aequitas          — GLFW + Vulkan visual sandbox             │
│    aequitas_headless — CLI batch runner, no GPU                 │
└────────────┬───────────────────────────────┬────────────────────┘
             │                               │
    ┌────────▼────────┐              ┌───────▼───────┐
    │  aequitas_ui    │              │ aequitas_sim  │
    │  hud            │              │ (stdlib only) │
    └────────┬────────┘              └───────▲───────┘
             │                               │
    ┌────────▼────────┐                      │
    │ aequitas_render │──────────────────────┘
    │ vk_context …    │   reads sim state; mutates via Simulation commands
    └─────────────────┘
```

### `aequitas_sim` — headless economic core

| Module | Location | Responsibility |
|--------|----------|----------------|
| **params** | `src/sim/params.h` | All tuning constants (map size, agent counts, market spreads, sovereign knobs). Single source of truth — no magic numbers in `.cpp` files. |
| **rng** | `src/sim/rng.h` | Explicitly seeded `std::mt19937_64` wrapper. Every stochastic draw flows through a `Rng` instance owned by `Simulation`. |
| **hex** | `src/sim/hex.h` | Pointy-top axial hex grid: coordinates, distance, neighbors, world ↔ hex projection. |
| **world** | `src/sim/world.{h,cpp}` | Tile grid (biome, resource stock), agent placement, logistic regeneration, sovereign drought intervention. |
| **agent + Policy** | `src/sim/agent.{h,cpp}` | Agent state (position, inventory, cash, hunger) and the `Policy` interface that decides actions each tick. Foundation ships a simple heuristic policy. |
| **order_book** | `src/sim/order_book.{h,cpp}` | Per-commodity limit-order book at a hub: bid/ask queues, price-time priority, partial fills, expiry. |
| **market** | `src/sim/market.{h,cpp}` | Market facade keyed by `hubId`: routes orders to books, records last-trade prices, runs dusk settlement. |
| **telemetry** | `src/sim/telemetry.{h,cpp}` | Ring-buffered macro snapshots and trade log; CSV export hooks. |
| **simulation** | `src/sim/simulation.{h,cpp}` | Facade orchestrating tick phases: agent actions → matching → settlement → regeneration → telemetry flush. Exposes a command API for UI/sovereign input. |

**Dependency rule:** `aequitas_sim` links only the C++ standard library. It must never `#include` GLFW, Vulkan, ImGui, or any render/UI header.

### `aequitas_render` — Vulkan presentation

| Module | Location | Responsibility |
|--------|----------|----------------|
| **vk_context** | `src/render/vk_context.{h,cpp}` | Instance, surface (GLFW), physical/logical device, swapchain, sync primitives. Bootstrapped via volk + vk-bootstrap; memory via VMA. |
| **mesh** | `src/render/mesh.{h,cpp}` | GPU vertex/index buffers, draw descriptors. |
| **meshgen** | `src/render/meshgen.{h,cpp}` | Procedural geometry from sim state: hex tiles, agent markers, hub props. |
| **pipeline** | `src/render/pipeline.{h,cpp}` | Shader modules, descriptor layouts, graphics pipeline for lit mesh rendering. |
| **camera** | `src/render/camera.{h,cpp}` | Orbit/pan camera; sun direction derived from tick fraction for day/night. |
| **renderer** | `src/render/renderer.{h,cpp}` | Frame loop: poll input → advance sim (via commands) → rebuild dirty meshes → record command buffers → present. |

### `aequitas_ui` — overlay

| Module | Location | Responsibility |
|--------|----------|----------------|
| **hud** | `src/ui/hud.{h,cpp}` | ImGui + ImPlot panels: macro charts, agent inspector, sovereign controls. Reads sim telemetry; writes sovereign commands through `Simulation`. |

### `apps/` — entry points

| Binary | Location | Role |
|--------|----------|------|
| **aequitas** | `src/app/main.cpp` | Interactive Vulkan sandbox. Owns GLFW window, wires render + HUD to a live `Simulation`. |
| **aequitas_headless** | `src/app/headless_main.cpp` | Batch runner for CI and research. Runs N ticks, exports CSV, exits. No GPU required. |

## Tick Lifecycle

Each simulation tick represents one visual day/night cycle:

1. **Dawn** — agents observe local tile stock and book quotes; `Policy::decide()` emits harvest/move/trade intents.
2. **Day** — intents execute: movement, harvesting, limit orders posted to the hub book.
3. **Dusk** — `Market::settle()` matches resting orders, clears expired quotes, updates last-trade prices.
4. **Night** — logistic regeneration; hunger applied; starvation/removal; telemetry ring push.

The renderer maps tick progress to sun elevation: `sun_angle = 2π × (tick_fraction)`.

## Dependency Rules

```
aequitas_sim        →  (stdlib only)
aequitas_render     →  aequitas_sim, Vulkan stack (volk, vk-bootstrap, VMA, GLFW, glm)
aequitas_ui         →  aequitas_sim, aequitas_render, imgui, implot
aequitas            →  aequitas_ui, aequitas_render, aequitas_sim
aequitas_headless   →  aequitas_sim
```

**Mutation boundary:** UI and render layers must not reach into `World`, `Agent`, or `Market` internals. All side effects go through `Simulation` public commands (`step()`, `inject_drought()`, `omo_buy()`, etc.). Read-only queries (`macro()`, `agents()`, `trades()`) are permitted for display.

## Expansion Seams

Each seam is a deliberate extension point. Implement the listed interface or data structure; wire it in the noted location.

### Policy interface → Q-learning / AR forecasting / PlayerPolicy

**Where:** `src/sim/agent.h` — `struct Policy { virtual Action decide(const AgentView&, const MarketView&) = 0; }`

**To add X:**

| Goal | Implement |
|------|-----------|
| Q-learning agents | `QLearningPolicy : Policy` with a state hash over `(inventory, hunger, local_stock, mid_price)` and ε-greedy action selection. Register in `Simulation` ctor or a `PolicyFactory`. |
| AR price forecasting | `ForecastPolicy : Policy` that fits an AR(p) model on the telemetry ring's price series each dusk; uses forecast to skew limit prices. |
| Human player | `PlayerPolicy : Policy` that reads a thread-safe intent queue filled by `hud.cpp` keyboard/UI picks. Swap the active agent's policy at runtime via `Simulation::set_policy(id, policy)`. |

### Market keyed by `hubId` → multi-hub + caravans

**Where:** `src/sim/market.{h,cpp}` — `std::unordered_map<int, HubMarket> hubs_`

**To add X:**

| Goal | Implement |
|------|-----------|
| Multiple trade hubs | Add hub placements in `World`; construct a `HubMarket` per hub in `Market::Market()`. Key all order routing by `hubId`. |
| Caravan agents | Extend `Action` with `CARAVAN_LOAD` / `CARAVAN_TRAVEL`; caravan policy buys at low hub, moves over multiple ticks, sells at high hub. Inter-hub spread emerges from separate books. |
| Hub-specific tariffs | `HubMarket::apply_tariff(commodity, rate)` called during settlement; sovereign sets per-hub rates via `Simulation::set_tariff(hubId, …)`. |

### CentralLedger → interest rates, OMO, tariffs

**Where:** new `src/sim/ledger.{h,cpp}`, owned by `Simulation`

**To add X:**

| Goal | Implement |
|------|-----------|
| Interest on cash balances | `CentralLedger::accrue_interest(rate, tick)` — credit agents proportional to cash holdings each dusk. |
| Open-market operations | `CentralLedger::omo_buy(commodity, qty, max_price)` — sovereign posts aggressive bids into the LOB via `Market::inject_order()`. |
| Tariff revenue | `CentralLedger::collect_tariff(amount)` — flows tariff proceeds into sovereign cash; expose balance in telemetry macro snapshot. |

Wire sovereign HUD buttons in `hud.cpp` to `Simulation` commands that delegate to `CentralLedger`.

### Mesh registry / meshgen → new prop types

**Where:** `src/render/meshgen.{h,cpp}`

**To add X:**

| Goal | Implement |
|------|-----------|
| New biome visual | Add a generator function `build_<biome>_tile(MeshBuilder&, Hex, float height)` and register it in the biome → mesh dispatch table inside `meshgen_rebuild_tiles()`. |
| Agent body variants | Extend `meshgen_rebuild_agents()` with a `MeshRecipe` per agent archetype (farmer, trader, caravan). |
| Hub buildings / props | `meshgen_rebuild_props()` reads hub list from sim; instanced draw for market stalls, warehouses. |

Keep geometry generation pure: meshgen reads const sim snapshots, never mutates sim state.

### Telemetry sinks → alternate exporters

**Where:** `src/sim/telemetry.{h,cpp}` — `struct TelemetrySink { virtual void on_macro(const MacroSnapshot&) = 0; virtual void on_trade(const Trade&) = 0; }`

**To add X:**

| Goal | Implement |
|------|-----------|
| JSON Lines export | `JsonlSink : TelemetrySink` appending one JSON object per event; enable via headless CLI `--format jsonl`. |
| Live gRPC stream | `GrpcSink : TelemetrySink` batching snapshots; run a side-thread server (outside `aequitas_sim` if networking is needed — keep sink interface in sim, implementation in app layer). |
| In-memory-only (tests) | `VectorSink : TelemetrySink` — already the pattern used by unit tests; accumulate for assertions. |

`Simulation` holds a `std::vector<std::unique_ptr<TelemetrySink>>`; `telemetry_flush()` fans out each dusk.

## Testing

Unit tests live in `tests/` and link only `aequitas_sim`:

- `test_order_book` — matching, partial fills, expiry
- `test_conservation` — commodity + cash accounting in closed economy
- `test_determinism` — same seed → identical trade log
- `test_parity` — headless tick results match embedded golden vectors

Run via `ctest` after build, or `tools/build-*.ps1|.sh` with no extra flags.

## Build Targets (CMake)

| Target | Type | Notes |
|--------|------|-------|
| `aequitas_sim` | `STATIC` library | Always built |
| `aequitas_headless` | executable | Always built |
| `aequitas_render` | `STATIC` library | `AEQUITAS_BUILD_RENDERER=ON` (default) |
| `aequitas_ui` | `STATIC` library | same gate |
| `aequitas` | executable | same gate |
| `test_*` | executables | registered with CTest |

Presets are defined in `CMakePresets.json`: `debug`, `release`, `ci`, `windows-vs`.
