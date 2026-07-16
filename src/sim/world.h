#pragma once

#include "sim/hex.h"
#include "sim/rng.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace aeq {

enum class Biome : std::uint8_t { Plains = 0, Forest = 1, Mountain = 2, Water = 3 };

enum class Resource : std::uint8_t { Food = 0, Wood = 1, Stone = 2, Count = 3 };

inline Resource biome_resource(Biome b) {
    switch (b) {
    case Biome::Plains:
        return Resource::Food;
    case Biome::Forest:
        return Resource::Wood;
    case Biome::Mountain:
        return Resource::Stone;
    default:
        return Resource::Food;
    }
}

inline const char* resource_name(Resource r) {
    switch (r) {
    case Resource::Food:
        return "FOOD";
    case Resource::Wood:
        return "WOOD";
    case Resource::Stone:
        return "STONE";
    default:
        return "?";
    }
}

// Visual prism top height (must stay in sync with Renderer terrain bake).
inline float tile_surface_height(Biome b) {
    switch (b) {
    case Biome::Plains:
        return 0.22f;
    case Biome::Forest:
        return 0.30f;
    case Biome::Mountain:
        return 0.62f;
    case Biome::Water:
    default:
        return 0.06f;
    }
}

inline const char* biome_name(Biome b) {
    switch (b) {
    case Biome::Plains:
        return "Plains";
    case Biome::Forest:
        return "Forest";
    case Biome::Mountain:
        return "Mountain";
    case Biome::Water:
        return "Water";
    default:
        return "?";
    }
}

struct Tile {
    Hex hex;
    Biome biome = Biome::Water;
    int stock = 0;
    int capacity = 0; // K
};

class World {
public:
    void generate(int radius, Rng& rng);

    int radius() const { return radius_; }
    const std::vector<Tile>& tiles() const { return tiles_; }
    std::vector<Tile>& tiles() { return tiles_; }

    std::optional<int> index_of(Hex h) const;
    const Tile* try_get(Hex h) const;
    Tile* try_get(Hex h);

    // Logistic regeneration on all non-water tiles. Returns total units added per resource.
    void regenerate(std::int64_t added_out[3]);

    // Drought: stock *= factor within radius of center.
    void drought(Hex center, int radius, double factor);

    Hex hub() const { return {0, 0}; }

    // Sum of tile stocks per resource (Food/Wood/Stone).
    void total_stock(std::int64_t out[3]) const;

private:
    int radius_ = 0;
    std::vector<Tile> tiles_;
};

} // namespace aeq
