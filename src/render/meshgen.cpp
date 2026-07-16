#include "render/meshgen.h"

#include <cmath>

namespace aeq {

void push_tri(CPUMesh& m, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& color) {
    glm::vec3 n = glm::cross(b - a, c - a);
    const float len = glm::length(n);
    n = len > 1e-8f ? n / len : glm::vec3(0.f, 1.f, 0.f);

    const std::uint32_t base = static_cast<std::uint32_t>(m.verts.size());
    m.verts.push_back(Vertex{a, n, color});
    m.verts.push_back(Vertex{b, n, color});
    m.verts.push_back(Vertex{c, n, color});
    m.indices.push_back(base + 0);
    m.indices.push_back(base + 1);
    m.indices.push_back(base + 2);
}

void push_quad(CPUMesh& m, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d,
               const glm::vec3& color) {
    push_tri(m, a, b, c, color);
    push_tri(m, a, c, d, color);
}

void merge_into(CPUMesh& dst, const CPUMesh& src, const glm::vec3& offset, float y_rot, float scale) {
    const float c = std::cos(y_rot);
    const float s = std::sin(y_rot);
    const std::uint32_t base = static_cast<std::uint32_t>(dst.verts.size());

    dst.verts.reserve(dst.verts.size() + src.verts.size());
    for (const auto& v : src.verts) {
        const glm::vec3 p = v.pos * scale;
        Vertex nv;
        nv.pos = glm::vec3(p.x * c - p.z * s, p.y, p.x * s + p.z * c) + offset;
        nv.normal = glm::vec3(v.normal.x * c - v.normal.z * s, v.normal.y, v.normal.x * s + v.normal.z * c);
        nv.color = v.color;
        dst.verts.push_back(nv);
    }
    dst.indices.reserve(dst.indices.size() + src.indices.size());
    for (auto idx : src.indices) {
        dst.indices.push_back(base + idx);
    }
}

glm::vec2 hex_corner_xz(int i, float size) {
    const float angle = glm::radians(60.f * static_cast<float>(i) - 30.f);
    return {size * std::cos(angle), size * std::sin(angle)};
}

namespace {

// Base ring at y0, top ring at y1; ring winds so that the derived side/cap
// triangles face outward under CCW-front-face conventions.
void append_tapered_box(CPUMesh& m, const glm::vec3& color, float half_w_bottom, float half_w_top, float y0,
                         float y1) {
    const glm::vec3 bot[4] = {
        {-half_w_bottom, y0, -half_w_bottom},
        {half_w_bottom, y0, -half_w_bottom},
        {half_w_bottom, y0, half_w_bottom},
        {-half_w_bottom, y0, half_w_bottom},
    };
    const glm::vec3 top[4] = {
        {-half_w_top, y1, -half_w_top},
        {half_w_top, y1, -half_w_top},
        {half_w_top, y1, half_w_top},
        {-half_w_top, y1, half_w_top},
    };
    for (int i = 0; i < 4; ++i) {
        const int j = (i + 1) % 4;
        push_quad(m, bot[j], bot[i], top[i], top[j], color);
    }
    push_quad(m, top[3], top[2], top[1], top[0], color);
}

// Hexagonal pyramid: base ring at y=0 (radius), apex at (apex_xz.x, height, apex_xz.y).
void append_hex_pyramid(CPUMesh& m, const glm::vec3& color, float radius, float height, const glm::vec2& apex_xz) {
    glm::vec3 base[6];
    for (int i = 0; i < 6; ++i) {
        const glm::vec2 c = hex_corner_xz(i, radius);
        base[i] = glm::vec3(c.x, 0.f, c.y);
    }
    const glm::vec3 apex{apex_xz.x, height, apex_xz.y};
    for (int i = 0; i < 6; ++i) {
        const int j = (i + 1) % 6;
        push_tri(m, base[j], base[i], apex, color);
    }
}

} // namespace

CPUMesh generate_hex_prism(const glm::vec3& color, float height, float size) {
    CPUMesh m;
    glm::vec3 top[6];
    glm::vec3 bot[6];
    for (int i = 0; i < 6; ++i) {
        const glm::vec2 c = hex_corner_xz(i, size);
        top[i] = glm::vec3(c.x, height, c.y);
        bot[i] = glm::vec3(c.x, 0.f, c.y);
    }
    const glm::vec3 top_center{0.f, height, 0.f};
    const glm::vec3 side_color = color * 0.78f;

    for (int i = 0; i < 6; ++i) {
        const int j = (i + 1) % 6;
        push_tri(m, top_center, top[j], top[i], color);
        push_quad(m, bot[j], bot[i], top[i], top[j], side_color);
    }
    return m;
}

CPUMesh generate_tree() {
    CPUMesh m;
    append_tapered_box(m, kPaletteTrunkBrown, 0.06f, 0.06f, 0.f, 0.30f);

    CPUMesh canopy1;
    append_hex_pyramid(canopy1, kPaletteForestGreen, 0.34f, 0.50f, glm::vec2(0.f, 0.f));
    merge_into(m, canopy1, glm::vec3(0.f, 0.16f, 0.f));

    CPUMesh canopy2;
    append_hex_pyramid(canopy2, kPaletteGrass, 0.20f, 0.40f, glm::vec2(0.f, 0.f));
    merge_into(m, canopy2, glm::vec3(0.f, 0.46f, 0.f));

    return m;
}

CPUMesh generate_rock() {
    CPUMesh m;
    append_hex_pyramid(m, kPaletteRockWarm, 0.30f, 0.34f, glm::vec2(0.04f, -0.03f));
    append_hex_pyramid(m, kPaletteRockWarm * 1.08f, 0.14f, 0.20f, glm::vec2(0.10f, 0.08f));
    return m;
}

CPUMesh generate_wheat() {
    CPUMesh m;
    const float h = 0.32f;
    const float w = 0.03f;
    for (int i = 0; i < 4; ++i) {
        const float angle = glm::radians(45.f * static_cast<float>(i));
        const glm::vec3 dir{std::cos(angle), 0.f, std::sin(angle)};
        const glm::vec3 a = dir * w;
        const glm::vec3 b = -dir * w;
        push_quad(m, glm::vec3(a.x, 0.f, a.z), glm::vec3(b.x, 0.f, b.z), glm::vec3(b.x, h, b.z),
                  glm::vec3(a.x, h, a.z), kPaletteWheatGold);
    }
    return m;
}

CPUMesh generate_meeple() {
    CPUMesh m;
    append_tapered_box(m, kPaletteMeepleBone * 0.92f, 0.16f, 0.10f, 0.f, 0.42f);
    append_tapered_box(m, kPaletteMeepleBone, 0.09f, 0.09f, 0.42f, 0.58f);
    return m;
}

CPUMesh generate_hub_banner() {
    CPUMesh m;
    append_tapered_box(m, kPaletteTrunkBrown, 0.05f, 0.05f, 0.f, 1.30f);

    const float bw = 0.32f;
    const float bh = 0.42f;
    const glm::vec3 base{0.f, 1.30f - bh, 0.02f};
    push_quad(m, base + glm::vec3(-bw, 0.f, 0.f), base + glm::vec3(bw, 0.f, 0.f), base + glm::vec3(bw, bh, 0.f),
              base + glm::vec3(-bw, bh, 0.f), kPaletteHubRust);
    push_quad(m, base + glm::vec3(bw, 0.f, 0.f), base + glm::vec3(-bw, 0.f, 0.f), base + glm::vec3(-bw, bh, 0.f),
              base + glm::vec3(bw, bh, 0.f), kPaletteHubRust);
    return m;
}

} // namespace aeq
