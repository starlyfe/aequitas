#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <volk.h>

#include <vk_mem_alloc.h>

struct GLFWwindow;

namespace aeq {

struct FrameContext {
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkImage swapchain_image = VK_NULL_HANDLE;
    VkImageView swapchain_view = VK_NULL_HANDLE;
    VkFormat swapchain_format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent{};
    uint32_t image_index = 0;
};

class VkContext {
public:
    void init(GLFWwindow* window, bool enable_validation);
    void shutdown();

    // Returns false if swapchain is out of date (caller should recreate & skip frame).
    bool begin_frame(FrameContext& out);
    void end_frame(const FrameContext& frame);

    void wait_idle();
    void on_resize();

    VkDevice device() const { return device_; }
    VkPhysicalDevice physical_device() const { return physical_device_; }
    VkQueue graphics_queue() const { return graphics_queue_; }
    uint32_t graphics_queue_family() const { return graphics_queue_family_; }
    VmaAllocator allocator() const { return allocator_; }
    VkFormat swapchain_format() const { return swapchain_format_; }
    VkFormat depth_format() const { return depth_format_; }
    VkExtent2D extent() const { return extent_; }
    VkImageView depth_view() const { return depth_view_; }
    VkImage depth_image() const { return depth_image_; }
    uint32_t frames_in_flight() const { return kFramesInFlight; }
    uint32_t frame_index() const { return frame_index_; }
    uint32_t swapchain_image_count() const { return static_cast<uint32_t>(swapchain_images_.size()); }

    // For ImGui init.
    VkInstance instance() const { return instance_; }
    VkRenderPass unused() const { return VK_NULL_HANDLE; } // dynamic rendering — no RP

private:
    static constexpr uint32_t kFramesInFlight = 2;

    void create_swapchain();
    void destroy_swapchain();
    void create_depth();
    void destroy_depth();

    GLFWwindow* window_ = nullptr;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    uint32_t graphics_queue_family_ = 0;
    VmaAllocator allocator_ = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchain_format_ = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D extent_{};
    std::vector<VkImage> swapchain_images_;
    std::vector<VkImageView> swapchain_views_;

    VkImage depth_image_ = VK_NULL_HANDLE;
    VmaAllocation depth_alloc_ = VK_NULL_HANDLE;
    VkImageView depth_view_ = VK_NULL_HANDLE;
    VkFormat depth_format_ = VK_FORMAT_D32_SFLOAT;

    struct FrameSync {
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkSemaphore image_available = VK_NULL_HANDLE;
        VkSemaphore render_finished = VK_NULL_HANDLE;
        VkFence in_flight = VK_NULL_HANDLE;
    };
    FrameSync frames_[kFramesInFlight]{};
    uint32_t frame_index_ = 0;
};

} // namespace aeq
