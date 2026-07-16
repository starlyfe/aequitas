#pragma once

#include "render/mesh.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace aeq {

// A muted, low-poly earthy palette (no purple-on-white "AI slop" look):
// greens for plains/forest, warm stone/wheat tones, dark teal water.
inline constexpr glm::vec3 kPaletteGrass{0.36f, 0.49f, 0.27f};
inline constexpr glm::vec3 kPaletteForestGreen{0.20f, 0.34f, 0.21f};
inline constexpr glm::vec3 kPaletteWheatField{0.72f, 0.63f, 0.40f};
inline constexpr glm::vec3 kPaletteMountainGrey{0.55f, 0.53f, 0.49f};
inline constexpr glm::vec3 kPaletteStoneDark{0.36f, 0.35f, 0.33f};
inline constexpr glm::vec3 kPaletteWaterDeep{0.09f, 0.20f, 0.26f};
inline constexpr glm::vec3 kPaletteWaterShallow{0.15f, 0.30f, 0.34f};
inline constexpr glm::vec3 kPaletteTrunkBrown{0.38f, 0.27f, 0.17f};
inline constexpr glm::vec3 kPaletteWheatGold{0.80f, 0.68f, 0.34f};
inline constexpr glm::vec3 kPaletteRockWarm{0.47f, 0.43f, 0.39f};
inline constexpr glm::vec3 kPaletteMeepleBone{0.85f, 0.81f, 0.71f};
inline constexpr glm::vec3 kPaletteHubRust{0.60f, 0.34f, 0.23f};

inline constexpr std::array<glm::vec3, 12> kPalette{
    kPaletteGrass,
    kPaletteForestGreen,
    kPaletteWheatField,
    kPaletteMountainGrey,
    kPaletteStoneDark,
    kPaletteWaterDeep,
    kPaletteWaterShallow,
    kPaletteTrunkBrown,
    kPaletteWheatGold,
    kPaletteRockWarm,
    kPaletteMeepleBone,
    kPaletteHubRust,
};

// Flat-shaded CPU-side mesh: verts are duplicated per triangle so normals stay per-face.
struct CPUMesh {
    std::vector<Vertex> verts;
    std::vector<std::uint32_t> indices;
};

void push_tri(CPUMesh& m, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& color);
void push_quad(CPUMesh& m, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d,
               const glm::vec3& color);

// Merge `src` into `dst`, applying a Y rotation (radians) then uniform scale then translation.
void merge_into(CPUMesh& dst, const CPUMesh& src, const glm::vec3& offset, float y_rot = 0.f, float scale = 1.f);

// Pointy-top hex corner, matching sim/hex.h's hex_to_world convention (world XZ plane, Y up).
glm::vec2 hex_corner_xz(int i, float size = 1.f);

// Flat-top prism (hexagonal column): base at y=0, cap at y=height.
CPUMesh generate_hex_prism(const glm::vec3& color, float height = 0.25f, float size = 1.0f);

CPUMesh generate_tree();
CPUMesh generate_rock();
CPUMesh generate_wheat();
CPUMesh generate_meeple();
CPUMesh generate_hub_banner();

} // namespace aeq
