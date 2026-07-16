#include "render/postfx.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace aeq {
namespace {

void image_barrier(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect, VkImageLayout old_layout,
                    VkImageLayout new_layout, VkAccessFlags src_access, VkAccessFlags dst_access,
                    VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = src_access;
    barrier.dstAccessMask = dst_access;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {aspect, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

std::filesystem::path exe_dir() {
#if defined(_WIN32)
    char buf[MAX_PATH];
    const DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        return std::filesystem::path(buf).parent_path();
    }
#endif
    std::error_code ec;
    return std::filesystem::current_path(ec);
}

std::vector<std::uint32_t> read_spirv(const std::string& relpath) {
    const std::filesystem::path candidates[] = {
        exe_dir() / relpath,
        std::filesystem::current_path() / relpath,
        std::filesystem::path(relpath),
    };
    for (const auto& p : candidates) {
        std::ifstream f(p, std::ios::binary | std::ios::ate);
        if (!f.is_open()) {
            continue;
        }
        const std::streamsize size = f.tellg();
        if (size <= 0 || (size % 4) != 0) {
            continue;
        }
        std::vector<std::uint32_t> buf(static_cast<std::size_t>(size) / sizeof(std::uint32_t));
        f.seekg(0);
        f.read(reinterpret_cast<char*>(buf.data()), size);
        return buf;
    }
    throw std::runtime_error("shader SPIR-V not found: " + relpath);
}

VkShaderModule make_module(VkDevice device, const std::vector<std::uint32_t>& code) {
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = code.size() * sizeof(std::uint32_t);
    ci.pCode = code.data();
    VkShaderModule mod = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &ci, nullptr, &mod) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateShaderModule failed");
    }
    return mod;
}

struct SsaoPush {
    glm::mat4 view_proj{1.f};
    glm::mat4 inv_view_proj{1.f};
    glm::vec4 params{1.5f, 0.025f, 1.15f, 0.f}; // radius, bias, intensity
};

struct BlurPush {
    glm::vec4 dir_texel{0.f};
};

struct CompositePush {
    glm::vec4 params{0.55f, 0.f, 0.f, 0.f};
};

VkPipeline make_fullscreen_pipeline(VkDevice device, VkPipelineLayout layout, VkShaderModule vert,
                                    VkShaderModule frag, VkFormat color_format, bool depth_disabled = true) {
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vertex_input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo input_assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport_state{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.f;

    VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depth_stencil{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depth_stencil.depthTestEnable = depth_disabled ? VK_FALSE : VK_TRUE;
    depth_stencil.depthWriteEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState blend_attachment{};
    blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                       VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo blend_state{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend_state.attachmentCount = 1;
    blend_state.pAttachments = &blend_attachment;

    const std::array<VkDynamicState, 2> dynamics = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamics.data();

    VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachmentFormats = &color_format;

    VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    ci.pNext = &rendering;
    ci.stageCount = 2;
    ci.pStages = stages;
    ci.pVertexInputState = &vertex_input;
    ci.pInputAssemblyState = &input_assembly;
    ci.pViewportState = &viewport_state;
    ci.pRasterizationState = &raster;
    ci.pMultisampleState = &multisample;
    ci.pDepthStencilState = &depth_stencil;
    ci.pColorBlendState = &blend_state;
    ci.pDynamicState = &dynamic_state;
    ci.layout = layout;

    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateGraphicsPipelines (postfx) failed");
    }
    return pipeline;
}

void create_color_image(VkContext& ctx, VkFormat format, VkExtent2D extent, VkImageUsageFlags usage, VkImage& image,
                        VmaAllocation& alloc, VkImageView& view) {
    VkImageCreateInfo image_ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image_ci.imageType = VK_IMAGE_TYPE_2D;
    image_ci.format = format;
    image_ci.extent = {extent.width, extent.height, 1};
    image_ci.mipLevels = 1;
    image_ci.arrayLayers = 1;
    image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
    image_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_ci.usage = usage;
    image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo alloc_ci{};
    alloc_ci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    alloc_ci.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    if (vmaCreateImage(ctx.allocator(), &image_ci, &alloc_ci, &image, &alloc, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("vmaCreateImage (postfx) failed");
    }

    VkImageViewCreateInfo view_ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view_ci.image = image;
    view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_ci.format = format;
    view_ci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(ctx.device(), &view_ci, nullptr, &view) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateImageView (postfx) failed");
    }
}

} // namespace

void PostFx::init(VkContext& ctx) {
    scene_format_ = ctx.swapchain_format();

    VkSamplerCreateInfo sampler_ci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler_ci.magFilter = VK_FILTER_LINEAR;
    sampler_ci.minFilter = VK_FILTER_LINEAR;
    sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_ci.maxLod = 0.f;
    if (vkCreateSampler(ctx.device(), &sampler_ci, nullptr, &sampler_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateSampler (postfx) failed");
    }

    auto make_layout = [&](std::uint32_t count) {
        std::vector<VkDescriptorSetLayoutBinding> bindings(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            bindings[i].binding = i;
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        ci.bindingCount = count;
        ci.pBindings = bindings.data();
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        if (vkCreateDescriptorSetLayout(ctx.device(), &ci, nullptr, &layout) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateDescriptorSetLayout failed");
        }
        return layout;
    };

    depth_set_layout_ = make_layout(1);
    ao_set_layout_ = make_layout(1);
    composite_set_layout_ = make_layout(2);

    VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8};
    VkDescriptorPoolCreateInfo pool_ci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_ci.maxSets = 8;
    pool_ci.poolSizeCount = 1;
    pool_ci.pPoolSizes = &pool_size;
    if (vkCreateDescriptorPool(ctx.device(), &pool_ci, nullptr, &desc_pool_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateDescriptorPool failed");
    }

    auto alloc_set = [&](VkDescriptorSetLayout layout) {
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = desc_pool_;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &layout;
        VkDescriptorSet set = VK_NULL_HANDLE;
        if (vkAllocateDescriptorSets(ctx.device(), &ai, &set) != VK_SUCCESS) {
            throw std::runtime_error("vkAllocateDescriptorSets failed");
        }
        return set;
    };
    depth_set_ = alloc_set(depth_set_layout_);
    blur_set_a_ = alloc_set(ao_set_layout_);
    blur_set_b_ = alloc_set(ao_set_layout_);
    composite_set_ = alloc_set(composite_set_layout_);

    create_pipelines(ctx);
    ensure_targets(ctx, ctx.extent());
}

void PostFx::shutdown(VkContext& ctx) {
    destroy_targets(ctx);
    destroy_pipelines(ctx);
    if (desc_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(ctx.device(), desc_pool_, nullptr);
        desc_pool_ = VK_NULL_HANDLE;
    }
    if (depth_set_layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(ctx.device(), depth_set_layout_, nullptr);
        depth_set_layout_ = VK_NULL_HANDLE;
    }
    if (ao_set_layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(ctx.device(), ao_set_layout_, nullptr);
        ao_set_layout_ = VK_NULL_HANDLE;
    }
    if (composite_set_layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(ctx.device(), composite_set_layout_, nullptr);
        composite_set_layout_ = VK_NULL_HANDLE;
    }
    if (sampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(ctx.device(), sampler_, nullptr);
        sampler_ = VK_NULL_HANDLE;
    }
}

void PostFx::ensure_targets(VkContext& ctx, VkExtent2D extent) {
    if (extent.width == 0 || extent.height == 0) {
        return;
    }
    if (scene_image_ != VK_NULL_HANDLE && extent_.width == extent.width && extent_.height == extent.height) {
        return;
    }
    destroy_targets(ctx);
    create_targets(ctx, extent);
}

void PostFx::destroy_targets(VkContext& ctx) {
    auto destroy_img = [&](VkImage& image, VmaAllocation& alloc, VkImageView& view) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(ctx.device(), view, nullptr);
            view = VK_NULL_HANDLE;
        }
        if (image != VK_NULL_HANDLE) {
            vmaDestroyImage(ctx.allocator(), image, alloc);
            image = VK_NULL_HANDLE;
            alloc = VK_NULL_HANDLE;
        }
    };
    destroy_img(scene_image_, scene_alloc_, scene_view_);
    destroy_img(ao_image_, ao_alloc_, ao_view_);
    destroy_img(ao_temp_image_, ao_temp_alloc_, ao_temp_view_);
    extent_ = {};
}

void PostFx::create_targets(VkContext& ctx, VkExtent2D extent) {
    extent_ = extent;
    scene_format_ = ctx.swapchain_format();

    create_color_image(ctx, scene_format_, extent,
                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, scene_image_, scene_alloc_,
                       scene_view_);
    create_color_image(ctx, VK_FORMAT_R8_UNORM, extent,
                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, ao_image_, ao_alloc_,
                       ao_view_);
    create_color_image(ctx, VK_FORMAT_R8_UNORM, extent,
                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, ao_temp_image_,
                       ao_temp_alloc_, ao_temp_view_);

    auto write_combined = [&](VkDescriptorSet set, std::uint32_t binding, VkImageView view) {
        VkDescriptorImageInfo info{};
        info.sampler = sampler_;
        info.imageView = view;
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = set;
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &info;
        vkUpdateDescriptorSets(ctx.device(), 1, &write, 0, nullptr);
    };

    write_combined(depth_set_, 0, ctx.depth_view());
    write_combined(blur_set_a_, 0, ao_view_);
    write_combined(blur_set_b_, 0, ao_temp_view_);

    VkDescriptorImageInfo scene_info{};
    scene_info.sampler = sampler_;
    scene_info.imageView = scene_view_;
    scene_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkDescriptorImageInfo ao_info{};
    ao_info.sampler = sampler_;
    ao_info.imageView = ao_view_;
    ao_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet writes[2]{};
    writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    writes[0].dstSet = composite_set_;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo = &scene_info;
    writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    writes[1].dstSet = composite_set_;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = &ao_info;
    vkUpdateDescriptorSets(ctx.device(), 2, writes, 0, nullptr);
}

void PostFx::create_pipelines(VkContext& ctx) {
    if (pipelines_ready_) {
        return;
    }
    const VkDevice device = ctx.device();
    const VkShaderModule vert = make_module(device, read_spirv("shaders/fullscreen.vert.spv"));
    const VkShaderModule ssao_frag = make_module(device, read_spirv("shaders/ssao.frag.spv"));
    const VkShaderModule blur_frag = make_module(device, read_spirv("shaders/ssao_blur.frag.spv"));
    const VkShaderModule comp_frag = make_module(device, read_spirv("shaders/ssao_composite.frag.spv"));

    auto make_layout = [&](VkDescriptorSetLayout set_layout, std::uint32_t push_size) {
        VkPushConstantRange push{};
        push.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        push.offset = 0;
        push.size = push_size;
        VkPipelineLayoutCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        ci.setLayoutCount = 1;
        ci.pSetLayouts = &set_layout;
        ci.pushConstantRangeCount = 1;
        ci.pPushConstantRanges = &push;
        VkPipelineLayout layout = VK_NULL_HANDLE;
        if (vkCreatePipelineLayout(device, &ci, nullptr, &layout) != VK_SUCCESS) {
            throw std::runtime_error("vkCreatePipelineLayout (postfx) failed");
        }
        return layout;
    };

    ssao_layout_ = make_layout(depth_set_layout_, sizeof(SsaoPush));
    blur_layout_ = make_layout(ao_set_layout_, sizeof(BlurPush));
    composite_layout_ = make_layout(composite_set_layout_, sizeof(CompositePush));

    ssao_pipeline_ = make_fullscreen_pipeline(device, ssao_layout_, vert, ssao_frag, VK_FORMAT_R8_UNORM);
    blur_pipeline_ = make_fullscreen_pipeline(device, blur_layout_, vert, blur_frag, VK_FORMAT_R8_UNORM);
    composite_pipeline_ =
        make_fullscreen_pipeline(device, composite_layout_, vert, comp_frag, ctx.swapchain_format());

    vkDestroyShaderModule(device, vert, nullptr);
    vkDestroyShaderModule(device, ssao_frag, nullptr);
    vkDestroyShaderModule(device, blur_frag, nullptr);
    vkDestroyShaderModule(device, comp_frag, nullptr);
    pipelines_ready_ = true;
}

void PostFx::destroy_pipelines(VkContext& ctx) {
    auto destroy_pipe = [&](VkPipeline& p) {
        if (p != VK_NULL_HANDLE) {
            vkDestroyPipeline(ctx.device(), p, nullptr);
            p = VK_NULL_HANDLE;
        }
    };
    auto destroy_layout = [&](VkPipelineLayout& l) {
        if (l != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(ctx.device(), l, nullptr);
            l = VK_NULL_HANDLE;
        }
    };
    destroy_pipe(ssao_pipeline_);
    destroy_pipe(blur_pipeline_);
    destroy_pipe(composite_pipeline_);
    destroy_layout(ssao_layout_);
    destroy_layout(blur_layout_);
    destroy_layout(composite_layout_);
    pipelines_ready_ = false;
}

void PostFx::prepare_scene_target(VkCommandBuffer cmd) {
    if (scene_image_ == VK_NULL_HANDLE) {
        return;
    }
    image_barrier(cmd, scene_image_, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
}

void PostFx::apply(VkContext& ctx, const FrameContext& frame, const glm::mat4& view_proj, float ao_strength) {
    ensure_targets(ctx, frame.extent);
    // Depth descriptor may be stale after swapchain resize (new depth view).
    {
        VkDescriptorImageInfo info{};
        info.sampler = sampler_;
        info.imageView = ctx.depth_view();
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = depth_set_;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &info;
        vkUpdateDescriptorSets(ctx.device(), 1, &write, 0, nullptr);
    }

    const VkCommandBuffer cmd = frame.cmd;
    const VkViewport viewport{0.f, 0.f, static_cast<float>(frame.extent.width),
                               static_cast<float>(frame.extent.height), 0.f, 1.f};
    const VkRect2D scissor{VkOffset2D{0, 0}, frame.extent};

    auto begin_color_pass = [&](VkImageView view, VkAttachmentLoadOp load) {
        VkRenderingAttachmentInfo color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        color.imageView = view;
        color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color.loadOp = load;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.clearValue.color = VkClearColorValue{{1.f, 1.f, 1.f, 1.f}};

        VkRenderingInfo info{VK_STRUCTURE_TYPE_RENDERING_INFO};
        info.renderArea = scissor;
        info.layerCount = 1;
        info.colorAttachmentCount = 1;
        info.pColorAttachments = &color;
        vkCmdBeginRendering(cmd, &info);
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);
    };

    // Scene color: attachment → sampled. Depth: attachment → sampled.
    image_barrier(cmd, scene_image_, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                  VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    image_barrier(cmd, ctx.depth_image(), VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    // AO target undefined → color attachment.
    image_barrier(cmd, ao_image_, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    SsaoPush ssao_pc;
    ssao_pc.view_proj = view_proj;
    ssao_pc.inv_view_proj = glm::inverse(view_proj);
    ssao_pc.params = glm::vec4(1.65f, 0.03f, 1.2f, 0.f);

    begin_color_pass(ao_view_, VK_ATTACHMENT_LOAD_OP_CLEAR);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ssao_pipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ssao_layout_, 0, 1, &depth_set_, 0, nullptr);
    vkCmdPushConstants(cmd, ssao_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ssao_pc), &ssao_pc);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRendering(cmd);

    // Blur H: ao → temp
    image_barrier(cmd, ao_image_, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                  VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    image_barrier(cmd, ao_temp_image_, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    BlurPush blur_pc;
    blur_pc.dir_texel = glm::vec4(1.f / static_cast<float>(frame.extent.width), 0.f, 0.f, 0.f);
    begin_color_pass(ao_temp_view_, VK_ATTACHMENT_LOAD_OP_CLEAR);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blur_pipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blur_layout_, 0, 1, &blur_set_a_, 0, nullptr);
    vkCmdPushConstants(cmd, blur_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(blur_pc), &blur_pc);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRendering(cmd);

    // Blur V: temp → ao
    image_barrier(cmd, ao_temp_image_, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                  VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    image_barrier(cmd, ao_image_, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    blur_pc.dir_texel = glm::vec4(0.f, 1.f / static_cast<float>(frame.extent.height), 0.f, 0.f);
    begin_color_pass(ao_view_, VK_ATTACHMENT_LOAD_OP_CLEAR);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blur_pipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blur_layout_, 0, 1, &blur_set_b_, 0, nullptr);
    vkCmdPushConstants(cmd, blur_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(blur_pc), &blur_pc);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRendering(cmd);

    image_barrier(cmd, ao_image_, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                  VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    // Composite → swapchain (already COLOR_ATTACHMENT from begin_frame).
    CompositePush comp_pc;
    comp_pc.params = glm::vec4(ao_strength, 0.f, 0.f, 0.f);
    begin_color_pass(frame.swapchain_view, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, composite_pipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, composite_layout_, 0, 1, &composite_set_, 0,
                            nullptr);
    vkCmdPushConstants(cmd, composite_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(comp_pc), &comp_pc);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRendering(cmd);

    // Return depth to attachment layout for the ImGui pass.
    image_barrier(cmd, ctx.depth_image(), VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                  VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
}

} // namespace aeq
