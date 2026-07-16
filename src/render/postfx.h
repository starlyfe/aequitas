#pragma once

#include "render/vk_context.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace aeq {

// Depth-based SSAO + soft composite. Scene is rendered into an offscreen color target;
// this class produces AO from the depth buffer and composites onto the swapchain.
class PostFx {
public:
    void init(VkContext& ctx);
    void shutdown(VkContext& ctx);

    // Recreate size-dependent images when the swapchain extent changes.
    void ensure_targets(VkContext& ctx, VkExtent2D extent);

    VkImageView scene_color_view() const { return scene_view_; }
    VkFormat scene_color_format() const { return scene_format_; }

    // Transition scene color to COLOR_ATTACHMENT for the lit pass (safe every frame with CLEAR).
    void prepare_scene_target(VkCommandBuffer cmd);

    // After the lit pass has written scene_color + depth (depth STORE), apply SSAO and
    // composite the result into the swapchain image.
    void apply(VkContext& ctx, const FrameContext& frame, const glm::mat4& view_proj, float ao_strength = 0.55f);

private:
    void destroy_targets(VkContext& ctx);
    void create_targets(VkContext& ctx, VkExtent2D extent);
    void create_pipelines(VkContext& ctx);
    void destroy_pipelines(VkContext& ctx);

    VkExtent2D extent_{};

    VkFormat scene_format_ = VK_FORMAT_R8G8B8A8_UNORM;
    VkImage scene_image_ = VK_NULL_HANDLE;
    VmaAllocation scene_alloc_ = VK_NULL_HANDLE;
    VkImageView scene_view_ = VK_NULL_HANDLE;

    VkImage ao_image_ = VK_NULL_HANDLE;
    VmaAllocation ao_alloc_ = VK_NULL_HANDLE;
    VkImageView ao_view_ = VK_NULL_HANDLE;

    VkImage ao_temp_image_ = VK_NULL_HANDLE;
    VmaAllocation ao_temp_alloc_ = VK_NULL_HANDLE;
    VkImageView ao_temp_view_ = VK_NULL_HANDLE;

    VkSampler sampler_ = VK_NULL_HANDLE;

    VkDescriptorSetLayout depth_set_layout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout ao_set_layout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout composite_set_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool desc_pool_ = VK_NULL_HANDLE;
    VkDescriptorSet depth_set_ = VK_NULL_HANDLE;
    VkDescriptorSet blur_set_a_ = VK_NULL_HANDLE; // samples ao → writes temp
    VkDescriptorSet blur_set_b_ = VK_NULL_HANDLE; // samples temp → writes ao
    VkDescriptorSet composite_set_ = VK_NULL_HANDLE;

    VkPipelineLayout ssao_layout_ = VK_NULL_HANDLE;
    VkPipelineLayout blur_layout_ = VK_NULL_HANDLE;
    VkPipelineLayout composite_layout_ = VK_NULL_HANDLE;
    VkPipeline ssao_pipeline_ = VK_NULL_HANDLE;
    VkPipeline blur_pipeline_ = VK_NULL_HANDLE;
    VkPipeline composite_pipeline_ = VK_NULL_HANDLE;

    bool pipelines_ready_ = false;
};

} // namespace aeq
