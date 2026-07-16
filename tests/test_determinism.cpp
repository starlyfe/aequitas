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

static std::uint64_t run(std::uint64_t seed, int ticks) {
    aeq::SimConfig cfg;
    cfg.seed = seed;
    aeq::Simulation sim;
    sim.init(cfg);
    for (int i = 0; i < ticks; ++i) {
        sim.tick();
    }
    return sim.state_hash();
}

int main() {
    constexpr int T = 5000;
    const auto h1 = run(42, T);
    const auto h2 = run(42, T);
    const auto h3 = run(99, T);

    CHECK(h1 == h2);
    CHECK(h1 != h3);

    if (failures) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_determinism: OK (hash=0x" << std::hex << h1 << std::dec << ")\n";
    return 0;
}
