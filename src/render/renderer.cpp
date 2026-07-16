#include "render/renderer.h"

#include "render/meshgen.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>

namespace aeq {
namespace {

std::uint32_t hash_combine(int q, int r, int i) {
    std::uint32_t h = static_cast<std::uint32_t>(q) * 374761393u + static_cast<std::uint32_t>(r) * 668265263u +
                       static_cast<std::uint32_t>(i) * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

float hash01(std::uint32_t x) {
    x = (x ^ 61u) ^ (x >> 16);
    x *= 9u;
    x ^= x >> 4;
    x *= 0x27d4eb2du;
    x ^= x >> 15;
    return static_cast<float>(x & 0xFFFFFFu) / static_cast<float>(0xFFFFFFu);
}

constexpr float kTau = 6.2831853f;

} // namespace

float Renderer::biome_height(Biome b) { return tile_surface_height(b); }

glm::vec3 Renderer::quintile_tint(int quintile) {
    switch (std::clamp(quintile, 0, 4)) {
    case 0:
        return glm::vec3(0.62f, 0.66f, 0.80f); // poorest — cool slate
    case 1:
        return glm::vec3(0.80f, 0.82f, 0.80f);
    case 2:
        return glm::vec3(1.0f, 1.0f, 1.0f); // median — neutral
    case 3:
        return glm::vec3(1.05f, 0.92f, 0.55f);
    default:
        return glm::vec3(1.15f, 0.95f, 0.30f); // richest — gold
    }
}

void Renderer::init(VkContext& ctx) {
    pipeline_.create(ctx);
    postfx_.init(ctx);

    auto upload = [&](Mesh& mesh, const CPUMesh& cpu) { mesh.upload(ctx, cpu.verts, cpu.indices); };
    upload(tree_mesh_, generate_tree());
    upload(rock_mesh_, generate_rock());
    upload(wheat_mesh_, generate_wheat());
    upload(meeple_mesh_, generate_meeple());
    upload(hub_mesh_, generate_hub_banner());
    upload(highlight_mesh_, generate_hex_prism(glm::vec3(1.0f, 0.92f, 0.35f), 0.05f, 1.0f));
}

void Renderer::shutdown(VkContext& ctx) {
    terrain_mesh_.destroy(ctx);
    tree_mesh_.destroy(ctx);
    rock_mesh_.destroy(ctx);
    wheat_mesh_.destroy(ctx);
    meeple_mesh_.destroy(ctx);
    hub_mesh_.destroy(ctx);
    highlight_mesh_.destroy(ctx);

    terrain_instances_.destroy(ctx);
    tree_instances_.destroy(ctx);
    rock_instances_.destroy(ctx);
    wheat_instances_.destroy(ctx);
    agent_instances_.destroy(ctx);
    hub_instances_.destroy(ctx);
    highlight_instances_.destroy(ctx);

    postfx_.shutdown(ctx);
    pipeline_.destroy(ctx);
}

void Renderer::bake_terrain(VkContext& ctx, const World& world) {
    CPUMesh merged;
    for (const auto& tile : world.tiles()) {
        glm::vec3 color;
        switch (tile.biome) {
        case Biome::Plains:
            color = kPaletteGrass;
            break;
        case Biome::Forest:
            color = kPaletteForestGreen;
            break;
        case Biome::Mountain:
            color = kPaletteMountainGrey;
            break;
        case Biome::Water:
        default:
            color = kPaletteWaterDeep;
            break;
        }
        const float height = biome_height(tile.biome);
        const CPUMesh prism = generate_hex_prism(color, height, params::HEX_SIZE);
        const Vec2 w = hex_to_world(tile.hex);
        merge_into(merged, prism, glm::vec3(w.x, 0.f, w.y));
    }
    terrain_mesh_.upload(ctx, merged.verts, merged.indices);
}

void Renderer::draw(VkContext& ctx, const FrameContext& frame, const Camera& camera, const SimulationView& sim,
                     std::optional<int> selected_agent, std::optional<Hex> selected_hex, float tick_alpha,
                     const glm::vec3& sun_dir, const glm::vec4& clear_color, float ambient, float sun_intensity) {
    const float aspect = frame.extent.height > 0
                              ? static_cast<float>(frame.extent.width) / static_cast<float>(frame.extent.height)
                              : 1.f;

    postfx_.ensure_targets(ctx, frame.extent);
    postfx_.prepare_scene_target(frame.cmd);

    VkRenderingAttachmentInfo color_attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    color_attachment.imageView = postfx_.scene_color_view();
    color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.clearValue.color =
        VkClearColorValue{{clear_color.r, clear_color.g, clear_color.b, clear_color.a}};

    VkRenderingAttachmentInfo depth_attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    depth_attachment.imageView = ctx.depth_view();
    depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth_attachment.clearValue.depthStencil = VkClearDepthStencilValue{1.f, 0};

    VkRenderingInfo rendering_info{VK_STRUCTURE_TYPE_RENDERING_INFO};
    rendering_info.renderArea = VkRect2D{VkOffset2D{0, 0}, frame.extent};
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attachment;
    rendering_info.pDepthAttachment = &depth_attachment;

    vkCmdBeginRendering(frame.cmd, &rendering_info);

    const VkViewport viewport{0.f, 0.f, static_cast<float>(frame.extent.width),
                               static_cast<float>(frame.extent.height), 0.f, 1.f};
    const VkRect2D scissor{VkOffset2D{0, 0}, frame.extent};
    vkCmdSetViewport(frame.cmd, 0, 1, &viewport);
    vkCmdSetScissor(frame.cmd, 0, 1, &scissor);

    pipeline_.bind(frame.cmd);
    LitPushConstants pc;
    pc.view_proj = camera.view_proj(aspect);
    pc.sun_dir = glm::length(sun_dir) > 1e-6f ? glm::normalize(sun_dir) : glm::vec3(0.f, -1.f, 0.f);
    pc.ambient = ambient;
    pc.sun_intensity = sun_intensity;
    pipeline_.push(frame.cmd, pc);

    auto draw_props = [&](Mesh& mesh, InstanceBuffer& instances) {
        if (instances.count() == 0) {
            return;
        }
        mesh.bind(frame.cmd);
        instances.bind(frame.cmd);
        mesh.draw(frame.cmd, instances.count());
    };

    // Terrain: one baked mesh, one identity instance.
    if (terrain_mesh_.valid()) {
        const InstanceData identity{glm::vec3(0.f), 0.f, 1.f, glm::vec4(1.f)};
        terrain_instances_.update(ctx, std::span<const InstanceData>(&identity, 1));
        draw_props(terrain_mesh_, terrain_instances_);
    }

    // Props scattered by tile stock density (deterministic hash so positions stay stable).
    scratch_tree_.clear();
    scratch_rock_.clear();
    scratch_wheat_.clear();
    if (sim.world != nullptr) {
        for (const auto& tile : sim.world->tiles()) {
            if (tile.biome == Biome::Water || tile.capacity <= 0) {
                continue;
            }
            const int density = std::clamp(
                static_cast<int>(
                    std::ceil(3.0 * static_cast<double>(tile.stock) / static_cast<double>(tile.capacity))),
                0, 6);
            std::vector<InstanceData>* target = nullptr;
            if (tile.biome == Biome::Forest) {
                target = &scratch_tree_;
            } else if (tile.biome == Biome::Mountain) {
                target = &scratch_rock_;
            } else if (tile.biome == Biome::Plains) {
                target = &scratch_wheat_;
            }
            if (target == nullptr) {
                continue;
            }

            const Vec2 base = hex_to_world(tile.hex);
            const float ground_y = biome_height(tile.biome);
            for (int i = 0; i < density; ++i) {
                const float angle = hash01(hash_combine(tile.hex.q, tile.hex.r, i * 2)) * kTau;
                const float radius = 0.12f + hash01(hash_combine(tile.hex.q, tile.hex.r, i * 2 + 1)) * 0.55f;
                InstanceData inst;
                inst.pos = glm::vec3(base.x + std::cos(angle) * radius, ground_y, base.y + std::sin(angle) * radius);
                inst.y_rot = hash01(hash_combine(tile.hex.q, tile.hex.r, i * 2 + 101)) * kTau;
                inst.scale = 0.8f + hash01(hash_combine(tile.hex.q, tile.hex.r, i + 777)) * 0.35f;
                inst.color = glm::vec4(1.f, 1.f, 1.f, 0.f);
                target->push_back(inst);
            }
        }
    }
    tree_instances_.update(ctx, scratch_tree_);
    rock_instances_.update(ctx, scratch_rock_);
    wheat_instances_.update(ctx, scratch_wheat_);
    draw_props(tree_mesh_, tree_instances_);
    draw_props(rock_mesh_, rock_instances_);
    draw_props(wheat_mesh_, wheat_instances_);

    // Agents: interpolate prev_pos -> pos for smooth motion between ticks, tint by wealth quintile.
    scratch_agents_.clear();
    if (sim.agents != nullptr && sim.market != nullptr) {
        const MarketQuotes quotes = sim.market->quotes(params::HUB_ID);
        std::vector<double> wealth_values;
        wealth_values.reserve(sim.agents->size());
        for (const auto& a : *sim.agents) {
            if (!a.alive) {
                continue;
            }
            double w = static_cast<double>(a.cash);
            for (int i = 0; i < 3; ++i) {
                w += static_cast<double>(a.inventory[static_cast<std::size_t>(i)]) *
                     static_cast<double>(quotes.last_price[i]);
            }
            wealth_values.push_back(w);
        }
        std::vector<double> sorted_wealth = wealth_values;
        std::sort(sorted_wealth.begin(), sorted_wealth.end());

        auto quintile_of = [&sorted_wealth](double w) -> int {
            if (sorted_wealth.empty()) {
                return 2;
            }
            const auto it = std::lower_bound(sorted_wealth.begin(), sorted_wealth.end(), w);
            const double rank = static_cast<double>(it - sorted_wealth.begin()) /
                                 static_cast<double>(sorted_wealth.size());
            return std::clamp(static_cast<int>(rank * 5.0), 0, 4);
        };

        const float pulse =
            0.5f + 0.5f * std::sin((static_cast<float>(sim.tick) + tick_alpha) * 6.0f);
        std::size_t wealth_i = 0;
        for (const auto& a : *sim.agents) {
            if (!a.alive) {
                continue;
            }
            const Vec2 prev_w = hex_to_world(a.prev_pos);
            const Vec2 cur_w = hex_to_world(a.pos);
            const float ix = prev_w.x + (cur_w.x - prev_w.x) * tick_alpha;
            const float iz = prev_w.y + (cur_w.y - prev_w.y) * tick_alpha;

            const double w = wealth_i < wealth_values.size() ? wealth_values[wealth_i] : 0.0;
            ++wealth_i;
            const glm::vec3 tint = quintile_tint(quintile_of(w));

            InstanceData inst;
            inst.pos = glm::vec3(ix, 0.f, iz);
            inst.y_rot = 0.f;
            inst.scale = 1.f;
            const float glow = (selected_agent.has_value() && *selected_agent == a.id) ? pulse : 0.f;
            inst.color = glm::vec4(tint, glow);
            scratch_agents_.push_back(inst);
        }
    }
    agent_instances_.update(ctx, scratch_agents_);
    draw_props(meeple_mesh_, agent_instances_);

    // Hub banner, fixed at the map center.
    {
        const InstanceData hub_inst{glm::vec3(0.f), 0.f, 1.f, glm::vec4(1.f, 1.f, 1.f, 0.f)};
        hub_instances_.update(ctx, std::span<const InstanceData>(&hub_inst, 1));
        draw_props(hub_mesh_, hub_instances_);
    }

    // Selected-tile highlight marker (pulsing glow, sits just above the tile surface).
    if (selected_hex.has_value()) {
        const Tile* t = sim.world != nullptr ? sim.world->try_get(*selected_hex) : nullptr;
        const float y = t != nullptr ? biome_height(t->biome) : 0.f;
        const float pulse = 0.5f + 0.5f * std::sin((static_cast<float>(sim.tick) + tick_alpha) * 8.0f);
        const Vec2 w = hex_to_world(*selected_hex);
        const InstanceData inst{glm::vec3(w.x, y, w.y), 0.f, 1.02f, glm::vec4(1.f, 1.f, 1.f, pulse)};
        highlight_instances_.update(ctx, std::span<const InstanceData>(&inst, 1));
        draw_props(highlight_mesh_, highlight_instances_);
    } else {
        highlight_instances_.update(ctx, std::span<const InstanceData>{});
    }

    vkCmdEndRendering(frame.cmd);

    const float aspect_for_post = frame.extent.height > 0
                                       ? static_cast<float>(frame.extent.width) / static_cast<float>(frame.extent.height)
                                       : 1.f;
    postfx_.apply(ctx, frame, camera.view_proj(aspect_for_post), 0.52f);
}

} // namespace aeq
