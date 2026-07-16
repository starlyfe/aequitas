#include "sim/world.h"

#include "sim/params.h"

#include <cmath>

namespace aeq {
namespace {

double value_noise(int x, int y, Rng& rng) {
    // Deterministic hash-ish noise from coordinates + a rng sample table approach:
    // Use a simple integer hash so regenerate doesn't depend on rng consumption order
    // beyond initial biome gen. For biome gen we sample rng per tile instead.
    (void)rng;
    std::uint32_t n = static_cast<std::uint32_t>(x) * 374761393u +
                      static_cast<std::uint32_t>(y) * 668265263u;
    n = (n ^ (n >> 13)) * 1274126177u;
    return (n & 0xFFFFu) / 65535.0;
}

int capacity_for(Biome b) {
    switch (b) {
    case Biome::Plains:
        return params::K_PLAINS;
    case Biome::Forest:
        return params::K_FOREST;
    case Biome::Mountain:
        return params::K_MOUNTAIN;
    default:
        return 0;
    }
}

} // namespace

void World::generate(int radius, Rng& rng) {
    radius_ = radius;
    tiles_.clear();
    tiles_.reserve(static_cast<std::size_t>(3 * radius * (radius + 1) + 1));

    for (int q = -radius; q <= radius; ++q) {
        const int r1 = std::max(-radius, -q - radius);
        const int r2 = std::min(radius, -q + radius);
        for (int r = r1; r <= r2; ++r) {
            Tile t;
            t.hex = {q, r};
            const int dist = hex_distance(t.hex, {0, 0});
            if (dist >= radius - 1) {
                t.biome = Biome::Water;
                t.stock = 0;
                t.capacity = 0;
            } else {
                // Blend value-noise blobs with rng jitter for variety.
                const double n1 = value_noise(q, r, rng);
                const double n2 = value_noise(q + 17, r - 9, rng);
                const double n3 = rng.uniform01();
                const double score_food = n1 + 0.15 * n3;
                const double score_wood = n2 + 0.15 * rng.uniform01();
                const double score_stone = (1.0 - n1) * 0.7 + 0.3 * rng.uniform01();
                if (score_stone > 0.72 && dist > 3) {
                    t.biome = Biome::Mountain;
                } else if (score_wood > score_food) {
                    t.biome = Biome::Forest;
                } else {
                    t.biome = Biome::Plains;
                }
                t.capacity = capacity_for(t.biome);
                t.stock = t.capacity; // start at carrying capacity
            }
            tiles_.push_back(t);
        }
    }
}

std::optional<int> World::index_of(Hex h) const {
    for (int i = 0; i < static_cast<int>(tiles_.size()); ++i) {
        if (tiles_[static_cast<std::size_t>(i)].hex == h) {
            return i;
        }
    }
    return std::nullopt;
}

const Tile* World::try_get(Hex h) const {
    if (auto i = index_of(h)) {
        return &tiles_[static_cast<std::size_t>(*i)];
    }
    return nullptr;
}

Tile* World::try_get(Hex h) {
    if (auto i = index_of(h)) {
        return &tiles_[static_cast<std::size_t>(*i)];
    }
    return nullptr;
}

void World::regenerate(std::int64_t added_out[3]) {
    added_out[0] = added_out[1] = added_out[2] = 0;
    for (auto& t : tiles_) {
        if (t.biome == Biome::Water || t.capacity <= 0) {
            continue;
        }
        if (t.stock <= 0) {
            // Seed a unit so logistic can restart from near-zero.
            t.stock = 1;
            const Resource res = biome_resource(t.biome);
            added_out[static_cast<int>(res)] += 1;
            continue;
        }
        const double growth =
            params::REGEN_R * static_cast<double>(t.stock) *
            (1.0 - static_cast<double>(t.stock) / static_cast<double>(t.capacity));
        int add = static_cast<int>(std::lround(growth));
        if (add < 0) {
            add = 0;
        }
        if (t.stock + add > t.capacity) {
            add = t.capacity - t.stock;
        }
        t.stock += add;
        const Resource res = biome_resource(t.biome);
        added_out[static_cast<int>(res)] += add;
    }
}

void World::drought(Hex center, int radius, double factor) {
    for (auto& t : tiles_) {
        if (hex_distance(t.hex, center) <= radius && t.biome != Biome::Water) {
            t.stock = static_cast<int>(std::lround(t.stock * factor));
        }
    }
}

void World::total_stock(std::int64_t out[3]) const {
    out[0] = out[1] = out[2] = 0;
    for (const auto& t : tiles_) {
        if (t.biome == Biome::Water) {
            continue;
        }
        out[static_cast<int>(biome_resource(t.biome))] += t.stock;
    }
}

} // namespace aeq
