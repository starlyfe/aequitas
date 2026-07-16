#pragma once

#include <cstdint>
#include <random>

namespace aeq {

// Explicitly seeded RNG — never use a global generator.
class Rng {
public:
    explicit Rng(std::uint64_t seed) : eng_(seed) {}

    std::uint64_t u64() { return eng_(); }

    // Uniform int in [lo, hi] inclusive.
    int uniform_int(int lo, int hi) {
        std::uniform_int_distribution<int> dist(lo, hi);
        return dist(eng_);
    }

    // Uniform double in [0, 1).
    double uniform01() {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(eng_);
    }

    // Uniform double in [lo, hi].
    double uniform(double lo, double hi) {
        std::uniform_real_distribution<double> dist(lo, hi);
        return dist(eng_);
    }

    // Symmetric noise: last * (1 + U(-amp, +amp))
    double noisy_mult(double amp) { return 1.0 + uniform(-amp, amp); }

private:
    std::mt19937_64 eng_;
};

} // namespace aeq
