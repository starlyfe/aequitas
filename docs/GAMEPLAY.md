# Aequitas Gameplay Design

Design inferences recorded from the foundation build and planned expansion seams. This document is the gameplay contract — implementation details live in `ARCHITECTURE.md`.

## Design Pillars

1. **Emergent economy** — prices, inequality, and survival emerge from local agent rules and a central limit-order book, not scripted quests.
2. **Aquarium first** — the default experience is observation: watch agents harvest, trade, and adapt under sovereign interventions you trigger.
3. **No win/lose** — the sandbox seeks macro homeostasis, not conquest. Success is measured, not rewarded with a game-over screen.
4. **One day per tick** — each simulation tick is one full visual day/night cycle; economic settlement happens at dusk.

## Time & Rhythm

| Phase | Sim moment | Player-visible behavior |
|-------|------------|-------------------------|
| Dawn | tick start | Agents wake; policies evaluate inventory, hunger, local stock, and book quotes. |
| Day | tick mid | Movement, harvesting, new limit orders posted. Sun climbs from horizon. |
| Dusk | settlement | Order book matches; trades execute; prices update. Sun touches horizon. |
| Night | tick end | Regeneration, hunger, starvation; telemetry flushed. Stars / dim ambient. |

**Sun position** is purely cosmetic but tightly coupled to tick progress:

```
sun_elevation = sin(2π × tick_fraction)   // 0 at dawn/dusk, 1 at noon
```

Trades **always settle at dusk**, never mid-day. This gives a readable pulse: activity → clearing → consequences.

## Foundation Mode (Phase 0)

What ships today.

### Observer

- Orbit camera over a hex world with plains, forest, and mountain biomes.
- 128 agents with a built-in heuristic policy: harvest when hungry, sell surplus, buy food when low, post limit orders around last trade ± spread.
- Single hub (`HUB_ID = 0`) at map center with order books for food, wood, and stone.
- ImPlot HUD panels: price series, Gini coefficient, population, aggregate wealth.
- Pause / step / speed controls.

The player watches an aquarium. The economy may stabilize, oscillate, or collapse depending on params and RNG seed — all three outcomes are interesting.

### Early Sovereign

Limited god-hand tools exposed in the HUD:

| Intervention | Effect |
|--------------|--------|
| **Drought** | Multiply food stock by `DROUGHT_FACTOR` within `DROUGHT_RADIUS` hexes of a chosen tile. |
| **Cash injection** | Add `SOVEREIGN_CASH_DELTA` cents to every agent (helicopter money). |

These are stress tests, not victory conditions. Use them to probe resilience: does the price spiral? Do agents starve? Does Gini widen?

## Participant Mode (Future)

**Seam:** `Policy` interface in `src/sim/agent.h`.

The player inhabits one agent (or a small firm) by swapping in a `PlayerPolicy`:

1. **Subsistence** — manually choose harvest vs. move vs. rest; learn hunger pressure and local depletion.
2. **Arbitrage** — notice spread between biomes and hub; post limit orders to capture it.
3. **Firms** — control multiple agents with shared treasury; specialize roles (harvester, hauler, market maker).

Progression is skill mastery, not tech-tree unlocks. The economy does not pause while you decide — the tick clock keeps running.

`PlayerPolicy` reads a thread-safe intent queue populated by keyboard shortcuts and HUD click-to-act. Other agents continue under AI policies.

## Sovereign Mode (Future)

**Seam:** `CentralLedger` in `src/sim/ledger.{h,cpp}` (planned).

Graduate from shock interventions to standing macro tools:

| Tool | Mechanism |
|------|-----------|
| **Interest rate** | `CentralLedger::accrue_interest(rate)` each dusk — reward/punish cash holdings. |
| **Open-market operations (OMO)** | Sovereign posts aggressive bids/asks into the LOB to defend a price band or inject liquidity. |
| **Tariffs** | Per-hub, per-commodity levy on settlement; revenue flows to sovereign balance. |

Multi-hub expansion (see `ARCHITECTURE.md`) makes tariffs and OMO spatially interesting: prop up the frontier hub, tax the capital hub.

## Agents & Resources

### Resources

| Commodity | Source biome | Use |
|-----------|--------------|-----|
| Food | Plains | Consumed each tick; starvation at `HUNGER_MAX`. |
| Wood | Forest | Building reserve (future); trade good now. |
| Stone | Mountain | Building reserve (future); trade good now. |

### Agent loop (heuristic policy)

1. If hungry and standing on a stocked tile → harvest.
2. If inventory exceeds personal keep thresholds → post sell limit order at dusk.
3. If food below `BUY_FOOD_THRESHOLD` → post buy limit order.
4. Otherwise → random walk within `MOVE_RANGE`.

Agents carry cash (cents), not barter. All exchange clears through the hub order book.

## Win / Lose Conditions

**None.**

Aequitas is a sandbox, not a roguelike. Agents may starve and population may crash, but the sim keeps running (or the player re-seeds). There is no game-over banner.

### Score = Macro Health

If we surface a single number, it should reflect systemic stability, not personal wealth:

| Metric | Weight | Rationale |
|--------|--------|-----------|
| Population / `AGENT_COUNT` | high | Are agents surviving? |
| Inverse Gini | medium | Is wealth concentrating catastrophically? |
| Price volatility (rolling σ) | medium | Is the market functioning or thrashing? |
| Trade volume | low | Is there economic activity? |

```
health = w₁ × (pop_ratio) + w₂ × (1 − gini) + w₃ × (1 − norm_vol) + w₄ × norm_volume
```

Display as a 0–100 dashboard gauge in the HUD. Useful for comparing seeds and sovereign policies, not for unlocking content.

## Homeostasis

The "interesting" outcome is a **dynamic equilibrium** — prices oscillate, agents migrate toward stocked tiles, Gini stays bounded, population hovers near carrying capacity. Collapse and hyperinflation are valid end states worth studying, not failures.

Design goal: a patient observer should be able to narrate a story from the charts alone ("drought at tick 400 → food spike → cash-poor agents starve → recovery").

## Controls (Placeholder)

Full bindings TBD in `README.md`. Intended layout:

| Input | Action |
|-------|--------|
| WASD / arrow keys | Pan camera |
| Scroll | Zoom |
| Space | Pause / resume |
| `.` | Single-step while paused |
| `1` / `2` | Decrease / increase sim speed |
| Click tile + drought button | Sovereign drought |
| Click agent | Inspect inventory in HUD |

Participant-mode bindings (move, harvest, post order) will layer on the same seam when `PlayerPolicy` lands.
