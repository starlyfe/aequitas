#include "sim/simulation.h"

#include <iostream>

// Headless/app parity: two Simulation instances with the same seed must match.
// (GUI must not mutate sim state except via Simulation commands.)
int main() {
    constexpr int T = 2000;
    aeq::SimConfig cfg;
    cfg.seed = 12345;

    aeq::Simulation a;
    a.init(cfg);
    aeq::Simulation b;
    b.init(cfg);

    for (int i = 0; i < T; ++i) {
        a.tick();
        b.tick();
    }

    if (a.state_hash() != b.state_hash()) {
        std::cerr << "FAIL: parity hash mismatch\n";
        return 1;
    }
    if (!a.money_conserved() || !b.money_conserved()) {
        std::cerr << "FAIL: money not conserved\n";
        return 1;
    }
    std::cout << "test_parity: OK\n";
    return 0;
}
