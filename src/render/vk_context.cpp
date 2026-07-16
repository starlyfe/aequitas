#include "render/vk_context.h"

// volk must be initialized before anything else touches Vulkan; VmaVulkanFunctions
// are then filled directly from volk's global function pointer table.
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <VkBootstrap.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <stdexcept>

namespace aeq {
namespace {

VkSurfaceKHR create_surface(VkInstance instance, GLFWwindow* window) {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("glfwCreateWindowSurface failed");
    }
    return surface;
}

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

} // namespace

void VkContext::init(GLFWwindow* window, bool enable_validation) {
    window_ = window;

    if (volkInitialize() != VK_SUCCESS) {
        throw std::runtime_error("volkInitialize failed - is a Vulkan driver installed?");
    }

    vkb::InstanceBuilder instance_builder;
    instance_builder.set_app_name("Aequitas")
        .set_engine_name("AequitasEngine")
        .require_api_version(1, 3, 0);

#ifndef NDEBUG
    if (enable_validation) {
        instance_builder.request_validation_layers(true);
        instance_builder.use_default_debug_messenger();
    }
#else
    (void)enable_validation;
#endif
    // vk-bootstrap >= 1.3 automatically enables VK_KHR_portability_enumeration (and sets
    // VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR) plus VK_KHR_portability_subset on the
    // device when available, which is exactly what MoltenVK needs.

    auto inst_ret = instance_builder.build();
    if (!inst_ret) {
        throw std::runtime_error("vkb instance build failed: " + inst_ret.error().message());
    }
    vkb::Instance vkb_instance = inst_ret.value();
    instance_ = vkb_instance.instance;
    debug_messenger_ = vkb_instance.debug_messenger;

    volkLoadInstance(instance_);

    surface_ = create_surface(instance_, window_);

    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    vkb::PhysicalDeviceSelector selector{vkb_instance};
    selector.set_surface(surface_).set_minimum_version(1, 3).set_required_features_13(features13);

    auto phys_ret = selector.select();
    if (!phys_ret) {
        throw std::runtime_error("vkb physical device selection failed: " + phys_ret.error().message());
    }
    vkb::PhysicalDevice vkb_phys = phys_ret.value();
    physical_device_ = vkb_phys.physical_device;

    vkb::DeviceBuilder device_builder{vkb_phys};
    auto dev_ret = device_builder.build();
    if (!dev_ret) {
        throw std::runtime_error("vkb device build failed: " + dev_ret.error().message());
    }
    vkb::Device vkb_device = dev_ret.value();
    device_ = vkb_device.device;

    volkLoadDevice(device_);

    auto queue_ret = vkb_device.get_queue(vkb::QueueType::graphics);
    if (!queue_ret) {
        throw std::runtime_error("failed to get graphics queue: " + queue_ret.error().message());
    }
    graphics_queue_ = queue_ret.value();
    graphics_queue_family_ = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

    VmaAllocatorCreateInfo allocator_ci{};
    allocator_ci.physicalDevice = physical_device_;
    allocator_ci.device = device_;
    allocator_ci.instance = instance_;
    allocator_ci.vulkanApiVersion = VK_API_VERSION_1_3;

    VmaVulkanFunctions vma_funcs{};
    if (vmaImportVulkanFunctionsFromVolk(&allocator_ci, &vma_funcs) != VK_SUCCESS) {
        throw std::runtime_error("vmaImportVulkanFunctionsFromVolk failed");
    }
    allocator_ci.pVulkanFunctions = &vma_funcs;

    if (vmaCreateAllocator(&allocator_ci, &allocator_) != VK_SUCCESS) {
        throw std::runtime_error("vmaCreateAllocator failed");
    }

    create_swapchain();
    create_depth();

    for (auto& f : frames_) {
        VkCommandPoolCreateInfo pool_ci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_ci.queueFamilyIndex = graphics_queue_family_;
        if (vkCreateCommandPool(device_, &pool_ci, nullptr, &f.pool) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateCommandPool failed");
        }

        VkCommandBufferAllocateInfo cmd_ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cmd_ai.commandPool = f.pool;
        cmd_ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd_ai.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(device_, &cmd_ai, &f.cmd) != VK_SUCCESS) {
            throw std::runtime_error("vkAllocateCommandBuffers failed");
        }

        VkSemaphoreCreateInfo sem_ci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        vkCreateSemaphore(device_, &sem_ci, nullptr, &f.image_available);
        vkCreateSemaphore(device_, &sem_ci, nullptr, &f.render_finished);

        VkFenceCreateInfo fence_ci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkCreateFence(device_, &fence_ci, nullptr, &f.in_flight);
    }
}

void VkContext::create_swapchain() {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    width = std::max(width, 1);
    height = std::max(height, 1);

    vkb::SwapchainBuilder builder{physical_device_, device_, surface_, graphics_queue_family_,
                                   graphics_queue_family_};
    builder.set_desired_extent(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height))
        .set_desired_format(VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .add_fallback_format(VkSurfaceFormatKHR{VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
        .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_old_swapchain(swapchain_);

    auto ret = builder.build();
    if (!ret) {
        throw std::runtime_error("vkb swapchain build failed: " + ret.error().message());
    }
    vkb::Swapchain vkb_swap = ret.value();

    // Old swapchain/views are still referenced by `swapchain_`/`swapchain_views_` at this point.
    if (swapchain_ != VK_NULL_HANDLE) {
        for (auto view : swapchain_views_) {
            vkDestroyImageView(device_, view, nullptr);
        }
        swapchain_views_.clear();
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    }

    swapchain_ = vkb_swap.swapchain;
    swapchain_format_ = vkb_swap.image_format;
    extent_ = vkb_swap.extent;

    auto images_ret = vkb_swap.get_images();
    if (!images_ret) {
        throw std::runtime_error("failed to get swapchain images: " + images_ret.error().message());
    }
    swapchain_images_ = images_ret.value();

    auto views_ret = vkb_swap.get_image_views();
    if (!views_ret) {
        throw std::runtime_error("failed to get swapchain image views: " + views_ret.error().message());
    }
    swapchain_views_ = views_ret.value();
}

void VkContext::destroy_swapchain() {
    for (auto view : swapchain_views_) {
        vkDestroyImageView(device_, view, nullptr);
    }
    swapchain_views_.clear();
    swapchain_images_.clear();
    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

void VkContext::create_depth() {
    VkImageCreateInfo image_ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image_ci.imageType = VK_IMAGE_TYPE_2D;
    image_ci.format = depth_format_;
    image_ci.extent = {extent_.width, extent_.height, 1};
    image_ci.mipLevels = 1;
    image_ci.arrayLayers = 1;
    image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
    image_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_ci.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo alloc_ci{};
    alloc_ci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    alloc_ci.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    if (vmaCreateImage(allocator_, &image_ci, &alloc_ci, &depth_image_, &depth_alloc_, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("vmaCreateImage (depth) failed");
    }

    VkImageViewCreateInfo view_ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view_ci.image = depth_image_;
    view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_ci.format = depth_format_;
    view_ci.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(device_, &view_ci, nullptr, &depth_view_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateImageView (depth) failed");
    }
}

void VkContext::destroy_depth() {
    if (depth_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, depth_view_, nullptr);
        depth_view_ = VK_NULL_HANDLE;
    }
    if (depth_image_ != VK_NULL_HANDLE) {
        vmaDestroyImage(allocator_, depth_image_, depth_alloc_);
        depth_image_ = VK_NULL_HANDLE;
        depth_alloc_ = VK_NULL_HANDLE;
    }
}

void VkContext::on_resize() {
    if (!window_ || device_ == VK_NULL_HANDLE) {
        return;
    }
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window_, &width, &height);
        glfwWaitEvents();
    }

    wait_idle();
    destroy_depth();
    create_swapchain();
    create_depth();
}

bool VkContext::begin_frame(FrameContext& out) {
    FrameSync& f = frames_[frame_index_];
    vkWaitForFences(device_, 1, &f.in_flight, VK_TRUE, UINT64_MAX);

    std::uint32_t image_index = 0;
    const VkResult acquire_res =
        vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, f.image_available, VK_NULL_HANDLE, &image_index);
    if (acquire_res == VK_ERROR_OUT_OF_DATE_KHR) {
        on_resize();
        return false;
    }
    if (acquire_res != VK_SUCCESS && acquire_res != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("vkAcquireNextImageKHR failed");
    }

    vkResetFences(device_, 1, &f.in_flight);
    vkResetCommandPool(device_, f.pool, 0);

    VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(f.cmd, &begin_info);

    image_barrier(f.cmd, swapchain_images_[image_index], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    image_barrier(f.cmd, depth_image_, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                  VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, 0,
                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

    out.cmd = f.cmd;
    out.swapchain_image = swapchain_images_[image_index];
    out.swapchain_view = swapchain_views_[image_index];
    out.swapchain_format = swapchain_format_;
    out.extent = extent_;
    out.image_index = image_index;
    return true;
}

void VkContext::end_frame(const FrameContext& frame) {
    FrameSync& f = frames_[frame_index_];

    image_barrier(frame.cmd, frame.swapchain_image, VK_IMAGE_ASPECT_COLOR_BIT,
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    vkEndCommandBuffer(frame.cmd);

    const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &f.image_available;
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &frame.cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &f.render_finished;
    if (vkQueueSubmit(graphics_queue_, 1, &submit, f.in_flight) != VK_SUCCESS) {
        throw std::runtime_error("vkQueueSubmit failed");
    }

    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &f.render_finished;
    present.swapchainCount = 1;
    present.pSwapchains = &swapchain_;
    present.pImageIndices = &frame.image_index;
    const VkResult present_res = vkQueuePresentKHR(graphics_queue_, &present);
    if (present_res == VK_ERROR_OUT_OF_DATE_KHR || present_res == VK_SUBOPTIMAL_KHR) {
        on_resize();
    } else if (present_res != VK_SUCCESS) {
        throw std::runtime_error("vkQueuePresentKHR failed");
    }

    frame_index_ = (frame_index_ + 1) % kFramesInFlight;
}

void VkContext::wait_idle() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }
}

void VkContext::shutdown() {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    wait_idle();

    for (auto& f : frames_) {
        if (f.in_flight != VK_NULL_HANDLE) {
            vkDestroyFence(device_, f.in_flight, nullptr);
        }
        if (f.render_finished != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, f.render_finished, nullptr);
        }
        if (f.image_available != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, f.image_available, nullptr);
        }
        if (f.pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device_, f.pool, nullptr);
        }
        f = FrameSync{};
    }

    destroy_depth();
    destroy_swapchain();

    if (allocator_ != VK_NULL_HANDLE) {
        vmaDestroyAllocator(allocator_);
        allocator_ = VK_NULL_HANDLE;
    }
    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
    if (debug_messenger_ != VK_NULL_HANDLE) {
        vkb::destroy_debug_utils_messenger(instance_, debug_messenger_);
        debug_messenger_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }

    physical_device_ = VK_NULL_HANDLE;
    window_ = nullptr;
}

} // namespace aeq
