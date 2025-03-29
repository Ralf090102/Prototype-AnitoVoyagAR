#pragma once

#include "VulkanRenderingContext.hpp"
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

class ARDepthTextureManager {
public:
    ARDepthTextureManager(std::shared_ptr<vulkan::VulkanRenderingContext> context);
    ~ARDepthTextureManager();

    // Update depth texture with new data from ARCore
    bool UpdateDepthTexture(const uint8_t* depth_data, int width, int height);

    // Get the descriptor set for binding in shaders
    VkDescriptorSet GetDepthTextureDescriptorSet() const { return descriptor_set_; }

    // Get descriptor set layout for pipeline creation
    VkDescriptorSetLayout GetDescriptorSetLayout() const { return descriptor_set_layout_; }

    // Initialize descriptor resources
    void CreateDescriptorResources();

    // Is the depth texture ready?
    bool IsTextureReady() const { return texture_ready_; }

    // Get depth texture details
    VkImageView GetDepthTextureView() const { return depth_texture_view_; }
    VkImage GetDepthTexture() const { return depth_texture_; }
    VkSampler GetDepthSampler() const { return depth_sampler_; }

private:
    std::shared_ptr<vulkan::VulkanRenderingContext> context_;

    // Vulkan resources for depth texture
    VkImage depth_texture_ = VK_NULL_HANDLE;
    VkDeviceMemory depth_texture_memory_ = VK_NULL_HANDLE;
    VkImageView depth_texture_view_ = VK_NULL_HANDLE;
    VkSampler depth_sampler_ = VK_NULL_HANDLE;

    // Staging resources for texture upload
    VkBuffer staging_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory staging_buffer_memory_ = VK_NULL_HANDLE;
    size_t current_staging_buffer_size_ = 0;

    // Descriptor resources
    VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set_ = VK_NULL_HANDLE;

    // Texture dimensions
    int texture_width_ = 0;
    int texture_height_ = 0;

    bool texture_ready_ = false;

    // Helper methods
    void CreateTextureResources(int width, int height);
    void CleanupTextureResources();
    void ResizeStagingBufferIfNeeded(size_t required_size);
};
