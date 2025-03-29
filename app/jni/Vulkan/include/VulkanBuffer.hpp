#pragma once

#include <cstddef>
#include "VulkanRenderingContext.hpp"

namespace vulkan {
    class VulkanBuffer {
    public:
        VulkanBuffer() = delete;
        VulkanBuffer(const VulkanBuffer &) = delete;
        VulkanBuffer(const std::shared_ptr<VulkanRenderingContext> &context, const size_t &length,
                     VkBufferUsageFlags usage,
                     VkMemoryPropertyFlags properties);
        void Update(const void *data);
        void UpdateFromCameraFeed(const void* data, size_t data_size);
        void CopyFrom(std::shared_ptr<VulkanBuffer> src_buffer,
                      size_t size,
                      size_t src_offset,
                      size_t dst_offset);
        [[nodiscard]] VkBuffer GetBuffer() const;
        [[nodiscard]] size_t GetSizeInBytes() const;
        virtual ~VulkanBuffer();
    protected:
        std::shared_ptr<VulkanRenderingContext> context_;
        VkDevice device_;
        size_t size_in_bytes_;
        VkBuffer buffer_ = 0;
        VkDeviceMemory memory_ = 0;
    private:
        bool host_visible_;
    };
}
