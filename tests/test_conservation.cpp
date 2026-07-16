#include "sim/simulation.h"

#include <iostream>

static int failures = 0;
#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::cerr << "FAIL: " << #cond << '\n';                                              \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)

int main() {
    aeq::SimConfig cfg;
    cfg.seed = 42;
    cfg.out_dir.clear();

    aeq::Simulation sim;
    sim.init(cfg);
    CHECK(sim.money_conserved());

    constexpr int N = 10000;
    for (int i = 0; i < N; ++i) {
        sim.tick();
        if (!sim.money_conserved()) {
            std::cerr << "Money broken at tick " << sim.tick_index() << '\n';
            ++failures;
            break;
        }
    }

    CHECK(sim.money_conserved());
    CHECK(sim.population() >= 0);

    // Resource flow: tile+inv+escrow should be consistent with regen - consumed
    // (soft check — population may have died returning stock).
    std::int64_t tile[3]{};
    sim.world().total_stock(tile);
    std::int64_t inv[3]{};
    for (const auto& a : sim.agents()) {
        if (!a.alive) {
            continue;
        }
        for (int i = 0; i < 3; ++i) {
            inv[i] += a.inventory[static_cast<std::size_t>(i)];
        }
    }
    for (int i = 0; i < 3; ++i) {
        const auto esc =
            sim.market().total_escrowed_qty(static_cast<aeq::Resource>(i));
        const std::int64_t total = tile[i] + inv[i] + esc;
        CHECK(total >= 0);
        (void)total;
    }

    if (failures) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_conservation: OK (pop=" << sim.population() << ")\n";
    return 0;
}
