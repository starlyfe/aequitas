#pragma once

#include "render/vk_context.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace aeq {

struct Vertex {
    glm::vec3 pos{0.f};
    glm::vec3 normal{0.f, 1.f, 0.f};
    glm::vec3 color{1.f};
};

struct InstanceData {
    glm::vec3 pos{0.f};
    float y_rot = 0.f;
    float scale = 1.f;
    glm::vec4 color{1.f};
};

// Static GPU mesh: interleaved vertex buffer + uint32 index buffer, both device-local,
// uploaded once via a host-visible staging buffer.
class Mesh {
public:
    void upload(VkContext& ctx, const std::vector<Vertex>& verts, const std::vector<std::uint32_t>& indices);
    void destroy(VkContext& ctx);

    void bind(VkCommandBuffer cmd) const;
    void draw(VkCommandBuffer cmd, std::uint32_t instance_count = 1) const;

    std::uint32_t index_count() const { return index_count_; }
    bool valid() const { return vertex_buffer_ != VK_NULL_HANDLE; }

private:
    VkBuffer vertex_buffer_ = VK_NULL_HANDLE;
    VmaAllocation vertex_alloc_ = VK_NULL_HANDLE;
    VkBuffer index_buffer_ = VK_NULL_HANDLE;
    VmaAllocation index_alloc_ = VK_NULL_HANDLE;
    std::uint32_t index_count_ = 0;
};

// Growable host-visible+coherent buffer of per-instance attributes, rewritten every frame.
class InstanceBuffer {
public:
    void update(VkContext& ctx, std::span<const InstanceData> instances);
    void destroy(VkContext& ctx);
    void bind(VkCommandBuffer cmd) const;

    std::uint32_t count() const { return count_; }

private:
    void ensure_capacity(VkContext& ctx, std::size_t bytes_needed);

    VkBuffer buffer_ = VK_NULL_HANDLE;
    VmaAllocation alloc_ = VK_NULL_HANDLE;
    void* mapped_ = nullptr;
    std::size_t capacity_bytes_ = 0;
    std::uint32_t count_ = 0;
};

} // namespace aeq
