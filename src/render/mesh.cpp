#include "render/mesh.h"

#include <cstring>
#include <stdexcept>

namespace aeq {
namespace {

struct StagingBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation alloc = VK_NULL_HANDLE;
};

StagingBuffer create_staging(VkContext& ctx, const void* data, VkDeviceSize size) {
    StagingBuffer sb;
    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = size;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_AUTO;
    aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    aci.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    VmaAllocationInfo info{};
    if (vmaCreateBuffer(ctx.allocator(), &bci, &aci, &sb.buffer, &sb.alloc, &info) != VK_SUCCESS) {
        throw std::runtime_error("failed to create staging buffer");
    }
    std::memcpy(info.pMappedData, data, static_cast<std::size_t>(size));
    return sb;
}

VkBuffer create_device_local(VkContext& ctx, VkDeviceSize size, VkBufferUsageFlags usage, VmaAllocation& out_alloc) {
    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = size;
    bci.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VkBuffer buf = VK_NULL_HANDLE;
    if (vmaCreateBuffer(ctx.allocator(), &bci, &aci, &buf, &out_alloc, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("failed to create device-local buffer");
    }
    return buf;
}

void immediate_copy(VkContext& ctx, VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pci.queueFamilyIndex = ctx.graphics_queue_family();
    VkCommandPool pool = VK_NULL_HANDLE;
    vkCreateCommandPool(ctx.device(), &pci, nullptr, &pool);

    VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = pool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(ctx.device(), &cbai, &cmd);

    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    VkBufferCopy region{};
    region.size = size;
    vkCmdCopyBuffer(cmd, src, dst, 1, &region);
    vkEndCommandBuffer(cmd);

    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence = VK_NULL_HANDLE;
    vkCreateFence(ctx.device(), &fci, nullptr, &fence);

    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(ctx.graphics_queue(), 1, &si, fence);
    vkWaitForFences(ctx.device(), 1, &fence, VK_TRUE, UINT64_MAX);

    vkDestroyFence(ctx.device(), fence, nullptr);
    vkDestroyCommandPool(ctx.device(), pool, nullptr);
}

} // namespace

void Mesh::upload(VkContext& ctx, const std::vector<Vertex>& verts, const std::vector<std::uint32_t>& indices) {
    destroy(ctx);
    if (verts.empty() || indices.empty()) {
        return;
    }

    const VkDeviceSize vsize = static_cast<VkDeviceSize>(verts.size() * sizeof(Vertex));
    const VkDeviceSize isize = static_cast<VkDeviceSize>(indices.size() * sizeof(std::uint32_t));

    StagingBuffer vstage = create_staging(ctx, verts.data(), vsize);
    StagingBuffer istage = create_staging(ctx, indices.data(), isize);

    vertex_buffer_ = create_device_local(ctx, vsize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertex_alloc_);
    index_buffer_ = create_device_local(ctx, isize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, index_alloc_);

    immediate_copy(ctx, vstage.buffer, vertex_buffer_, vsize);
    immediate_copy(ctx, istage.buffer, index_buffer_, isize);

    vmaDestroyBuffer(ctx.allocator(), vstage.buffer, vstage.alloc);
    vmaDestroyBuffer(ctx.allocator(), istage.buffer, istage.alloc);

    index_count_ = static_cast<std::uint32_t>(indices.size());
}

void Mesh::destroy(VkContext& ctx) {
    if (vertex_buffer_ != VK_NULL_HANDLE) {
        vmaDestroyBuffer(ctx.allocator(), vertex_buffer_, vertex_alloc_);
        vertex_buffer_ = VK_NULL_HANDLE;
        vertex_alloc_ = VK_NULL_HANDLE;
    }
    if (index_buffer_ != VK_NULL_HANDLE) {
        vmaDestroyBuffer(ctx.allocator(), index_buffer_, index_alloc_);
        index_buffer_ = VK_NULL_HANDLE;
        index_alloc_ = VK_NULL_HANDLE;
    }
    index_count_ = 0;
}

void Mesh::bind(VkCommandBuffer cmd) const {
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer_, &offset);
    vkCmdBindIndexBuffer(cmd, index_buffer_, 0, VK_INDEX_TYPE_UINT32);
}

void Mesh::draw(VkCommandBuffer cmd, std::uint32_t instance_count) const {
    if (index_count_ == 0 || instance_count == 0) {
        return;
    }
    vkCmdDrawIndexed(cmd, index_count_, instance_count, 0, 0, 0);
}

void InstanceBuffer::ensure_capacity(VkContext& ctx, std::size_t bytes_needed) {
    if (bytes_needed <= capacity_bytes_) {
        return;
    }
    std::size_t new_capacity = capacity_bytes_ == 0 ? 4096 : capacity_bytes_;
    while (new_capacity < bytes_needed) {
        new_capacity *= 2;
    }

    if (buffer_ != VK_NULL_HANDLE) {
        vmaDestroyBuffer(ctx.allocator(), buffer_, alloc_);
        buffer_ = VK_NULL_HANDLE;
        alloc_ = VK_NULL_HANDLE;
        mapped_ = nullptr;
    }

    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = static_cast<VkDeviceSize>(new_capacity);
    bci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_AUTO;
    aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    aci.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    VmaAllocationInfo info{};
    if (vmaCreateBuffer(ctx.allocator(), &bci, &aci, &buffer_, &alloc_, &info) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance buffer");
    }
    mapped_ = info.pMappedData;
    capacity_bytes_ = new_capacity;
}

void InstanceBuffer::update(VkContext& ctx, std::span<const InstanceData> instances) {
    count_ = static_cast<std::uint32_t>(instances.size());
    if (instances.empty()) {
        return;
    }
    const std::size_t bytes = instances.size() * sizeof(InstanceData);
    ensure_capacity(ctx, bytes);
    std::memcpy(mapped_, instances.data(), bytes);
}

void InstanceBuffer::destroy(VkContext& ctx) {
    if (buffer_ != VK_NULL_HANDLE) {
        vmaDestroyBuffer(ctx.allocator(), buffer_, alloc_);
        buffer_ = VK_NULL_HANDLE;
        alloc_ = VK_NULL_HANDLE;
        mapped_ = nullptr;
    }
    capacity_bytes_ = 0;
    count_ = 0;
}

void InstanceBuffer::bind(VkCommandBuffer cmd) const {
    if (buffer_ == VK_NULL_HANDLE) {
        return;
    }
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 1, 1, &buffer_, &offset);
}

} // namespace aeq
