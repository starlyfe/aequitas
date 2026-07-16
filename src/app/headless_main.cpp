#include "sim/simulation.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace {

struct Args {
    int ticks = 1000;
    std::uint64_t seed = 42;
    std::string out_dir;
};

Args parse(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--ticks") == 0 && i + 1 < argc) {
            a.ticks = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            a.seed = static_cast<std::uint64_t>(std::strtoull(argv[++i], nullptr, 10));
        } else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            a.out_dir = argv[++i];
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::cout << "aequitas_headless --ticks N --seed S --out DIR\n";
            std::exit(0);
        }
    }
    return a;
}

} // namespace

int main(int argc, char** argv) {
    const Args args = parse(argc, argv);
    std::cout << "Aequitas headless " << "0.1.0"
              << " — ticks=" << args.ticks << " seed=" << args.seed << '\n';

    aeq::SimConfig cfg;
    cfg.seed = args.seed;
    cfg.out_dir = args.out_dir;

    aeq::Simulation sim;
    sim.init(cfg);

    for (int t = 0; t < args.ticks; ++t) {
        sim.tick();
        if ((t + 1) % 1000 == 0) {
            std::cout << "  tick " << (t + 1) << " pop=" << sim.population()
                      << " gini=" << sim.current_gini()
                      << " money_ok=" << (sim.money_conserved() ? "yes" : "NO") << '\n';
        }
    }

    std::cout << "Done.\n"
              << "  population : " << sim.population() << '\n'
              << "  money check: " << (sim.money_conserved() ? "PASS" : "FAIL") << '\n'
              << "  gini       : " << sim.current_gini() << '\n'
              << "  state hash : 0x" << std::hex << sim.state_hash() << std::dec << '\n';

    sim.telemetry().close();
    return sim.money_conserved() ? 0 : 1;
}
