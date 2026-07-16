#pragma once

#include "sim/params.h"

#include <cmath>
#include <cstdlib>

namespace aeq {

// Pointy-top axial coordinates (Red Blob Games).
struct Hex {
    int q = 0;
    int r = 0;

    bool operator==(const Hex& o) const { return q == o.q && r == o.r; }
    bool operator!=(const Hex& o) const { return !(*this == o); }
};

inline Hex hex_add(Hex a, Hex b) { return {a.q + b.q, a.r + b.r}; }

inline int hex_distance(Hex a, Hex b) {
    const int dq = a.q - b.q;
    const int dr = a.r - b.r;
    return (std::abs(dq) + std::abs(dq + dr) + std::abs(dr)) / 2;
}

// Six neighbor directions (pointy-top).
inline constexpr Hex kHexDirs[6] = {
    {+1, 0}, {+1, -1}, {0, -1}, {-1, 0}, {-1, +1}, {0, +1},
};

inline Hex hex_neighbor(Hex h, int dir) { return hex_add(h, kHexDirs[dir % 6]); }

struct Vec2 {
    float x = 0.f;
    float y = 0.f;
};

// Axial → world (pointy-top), size = center-to-vertex.
inline Vec2 hex_to_world(Hex h, float size = params::HEX_SIZE) {
    const float x = size * (std::sqrt(3.f) * static_cast<float>(h.q) +
                            std::sqrt(3.f) / 2.f * static_cast<float>(h.r));
    const float y = size * (3.f / 2.f * static_cast<float>(h.r));
    return {x, y};
}

inline Hex cube_round(float fq, float fr, float fs) {
    int q = static_cast<int>(std::lround(fq));
    int r = static_cast<int>(std::lround(fr));
    int s = static_cast<int>(std::lround(fs));
    const float dq = std::fabs(static_cast<float>(q) - fq);
    const float dr = std::fabs(static_cast<float>(r) - fr);
    const float ds = std::fabs(static_cast<float>(s) - fs);
    if (dq > dr && dq > ds) {
        q = -r - s;
    } else if (dr > ds) {
        r = -q - s;
    }
    return {q, r};
}

inline Hex world_to_hex(Vec2 p, float size = params::HEX_SIZE) {
    const float fq = (std::sqrt(3.f) / 3.f * p.x - 1.f / 3.f * p.y) / size;
    const float fr = (2.f / 3.f * p.y) / size;
    const float fs = -fq - fr;
    return cube_round(fq, fr, fs);
}

} // namespace aeq
