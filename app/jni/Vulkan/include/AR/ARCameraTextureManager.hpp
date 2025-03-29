#pragma once

#include <vulkan/vulkan.h>
#include "VulkanRenderingContext.hpp"
#include <memory>
#include <vector>

class ARCameraTextureManager {
public:
    ARCameraTextureManager(std::shared_ptr<vulkan::VulkanRenderingContext> context);
    ~ARCameraTextureManager();

    // Update camera texture with new image data from ARCore
    bool UpdateCameraTexture(const uint8_t* image_data, int width, int height, int format);

    // Get the descriptor set for binding in shaders
    VkDescriptorSet GetCameraTextureDescriptorSet() const;

    // Initialize descriptor resources
    void CreateDescriptorResources();

    // Returns if the camera texture is ready for use
    bool IsTextureReady() const { return texture_ready_; }

    // Get camera texture details
    VkImageView GetCameraTextureView() const { return camera_texture_view_; }
    VkImage GetCameraTexture() const { return camera_texture_; }
    VkSampler GetCameraSampler() const { return camera_sampler_; }
    VkDescriptorSetLayout GetDescriptorSetLayout() const { return descriptor_set_layout_; }

    // Get camera intrinsics (from ARCore)
    float GetFocalLengthX() const { return focal_length_x_; }
    float GetFocalLengthY() const { return focal_length_y_; }
    float GetPrincipalPointX() const { return principal_point_x_; }
    float GetPrincipalPointY() const { return principal_point_y_; }

    // Set camera intrinsics (from ARCore)
    void SetCameraIntrinsics(float focal_length_x, float focal_length_y,
                             float principal_point_x, float principal_point_y);

private:
    std::shared_ptr<vulkan::VulkanRenderingContext> context_;

    // Vulkan resources for camera texture
    VkImage camera_texture_ = VK_NULL_HANDLE;
    VkDeviceMemory camera_texture_memory_ = VK_NULL_HANDLE;
    VkImageView camera_texture_view_ = VK_NULL_HANDLE;
    VkSampler camera_sampler_ = VK_NULL_HANDLE;

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

    // Camera intrinsics
    float focal_length_x_ = 0.0f;
    float focal_length_y_ = 0.0f;
    float principal_point_x_ = 0.0f;
    float principal_point_y_ = 0.0f;

    bool texture_ready_ = false;

    // Helper methods
    void CreateTextureResources(int width, int height);
    void CleanupTextureResources();
    void ResizeStagingBufferIfNeeded(size_t required_size);
};
