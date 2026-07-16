#pragma once

#include "render/camera.h"
#include "render/mesh.h"
#include "render/pipeline.h"
#include "render/postfx.h"
#include "render/vk_context.h"

#include "sim/hex.h"
#include "sim/simulation.h"
#include "sim/world.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <optional>

namespace aeq {

// Owns the lit pipeline, SSAO post stack, static prop meshes, and per-category instance buffers.
class Renderer {
public:
    void init(VkContext& ctx);
    void shutdown(VkContext& ctx);

    void bake_terrain(VkContext& ctx, const World& world);

    void draw(VkContext& ctx, const FrameContext& frame, const Camera& camera, const SimulationView& sim,
              std::optional<int> selected_agent, std::optional<Hex> selected_hex, float tick_alpha,
              const glm::vec3& sun_dir, const glm::vec4& clear_color, float ambient, float sun_intensity);

private:
    static float biome_height(Biome b);
    static glm::vec3 quintile_tint(int quintile);

    LitPipeline pipeline_;
    PostFx postfx_;

    Mesh terrain_mesh_;
    Mesh tree_mesh_;
    Mesh rock_mesh_;
    Mesh wheat_mesh_;
    Mesh meeple_mesh_;
    Mesh hub_mesh_;
    Mesh highlight_mesh_;

    InstanceBuffer terrain_instances_;
    InstanceBuffer tree_instances_;
    InstanceBuffer rock_instances_;
    InstanceBuffer wheat_instances_;
    InstanceBuffer agent_instances_;
    InstanceBuffer hub_instances_;
    InstanceBuffer highlight_instances_;

    std::vector<InstanceData> scratch_tree_;
    std::vector<InstanceData> scratch_rock_;
    std::vector<InstanceData> scratch_wheat_;
    std::vector<InstanceData> scratch_agents_;
};

} // namespace aeq
