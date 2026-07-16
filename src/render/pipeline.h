#pragma once

#include "render/mesh.h"
#include "render/vk_context.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <string>

namespace aeq {

struct LitPushConstants {
    glm::mat4 view_proj{1.f};
    glm::vec3 sun_dir{0.f, -1.f, 0.f};
    float ambient = 0.25f;
};

// Single graphics pipeline for all flat-shaded, per-instance-tinted low-poly props,
// rendered via dynamic rendering (no VkRenderPass / VkFramebuffer).
class LitPipeline {
public:
    void create(VkContext& ctx, const std::string& vert_spv_relpath = "shaders/lit.vert.spv",
                const std::string& frag_spv_relpath = "shaders/lit.frag.spv");
    void destroy(VkContext& ctx);

    void bind(VkCommandBuffer cmd) const;
    void push(VkCommandBuffer cmd, const LitPushConstants& pc) const;

    VkPipelineLayout layout() const { return layout_; }

private:
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
};

} // namespace aeq
