#include "ARCameraTextureManager.hpp"
#include "vulkan_utils.hpp"
#include <stdexcept>

ARCameraTextureManager::ARCameraTextureManager(std::shared_ptr<vulkan::VulkanRenderingContext> context)
        : context_(context) {
    // Nothing to initialize yet - we'll create texture when we get the first frame
}

ARCameraTextureManager::~ARCameraTextureManager() {
    CleanupTextureResources();
}

bool ARCameraTextureManager::UpdateCameraTexture(const uint8_t* image_data, int width, int height, int format) {
    if (!image_data || width <= 0 || height <= 0) {
        return false;
    }

    // Create or recreate texture if dimensions changed
    if (texture_width_ != width || texture_height_ != height || camera_texture_ == VK_NULL_HANDLE) {
        CleanupTextureResources();
        CreateTextureResources(width, height);
    }

    // Calculate required buffer size based on format
    size_t bytes_per_pixel = 4; // Assume RGBA8 format by default
    switch (format) {
        case 0: // RGBA8
            bytes_per_pixel = 4;
            break;
        case 1: // RGB8
            bytes_per_pixel = 3;
            break;
            // Add more formats as needed
    }

    size_t buffer_size = width * height * bytes_per_pixel;

    // Resize staging buffer if needed
    ResizeStagingBufferIfNeeded(buffer_size);

    // Copy data to staging buffer
    void* data;
    vkMapMemory(context_->GetDevice(), staging_buffer_memory_, 0, buffer_size, 0, &data);
    memcpy(data, image_data, buffer_size);
    vkUnmapMemory(context_->GetDevice(), staging_buffer_memory_);

    // Transition image layout for copy
    context_->TransitionImageLayout(camera_texture_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy from staging buffer to texture
    VkCommandBuffer cmd_buffer = context_->BeginSingleTimeCommands(context_->GetGraphicsPool());

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height),
            1
    };

    vkCmdCopyBufferToImage(cmd_buffer, staging_buffer_, camera_texture_,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    context_->EndSingleTimeCommands(context_->GetGraphicsQueue(), context_->GetGraphicsPool(), cmd_buffer);

    // Transition image layout for shader access
    context_->TransitionImageLayout(camera_texture_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    texture_ready_ = true;
    return true;
}

void ARCameraTextureManager::CreateTextureResources(int width, int height) {
    texture_width_ = width;
    texture_height_ = height;

    // Create the image
    context_->CreateImage(
            width,
            height,
            VK_SAMPLE_COUNT_1_BIT,
            VK_FORMAT_R8G8B8A8_UNORM, // Common format for camera textures
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &camera_texture_,
            &camera_texture_memory_
    );

    // Create the image view
    context_->CreateImageView(
            camera_texture_,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_ASPECT_COLOR_BIT,
            &camera_texture_view_
    );

    // Create the sampler
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = 16;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 0.0f;

    if (vkCreateSampler(context_->GetDevice(), &sampler_info, nullptr, &camera_sampler_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create camera texture sampler");
    }

    // Create descriptor resources if they don't exist yet
    if (descriptor_set_ == VK_NULL_HANDLE) {
        CreateDescriptorResources();
    }
}

void ARCameraTextureManager::CreateDescriptorResources() {
    // Create descriptor set layout
    VkDescriptorSetLayoutBinding sampler_binding{};
    sampler_binding.binding = 0;
    sampler_binding.descriptorCount = 1;
    sampler_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampler_binding.pImmutableSamplers = nullptr;
    sampler_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &sampler_binding;

    if (vkCreateDescriptorSetLayout(context_->GetDevice(), &layout_info, nullptr, &descriptor_set_layout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout");
    }

    // Create descriptor pool
    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = 1;

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    pool_info.maxSets = 1;

    if (vkCreateDescriptorPool(context_->GetDevice(), &pool_info, nullptr, &descriptor_pool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool");
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool_;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &descriptor_set_layout_;

    if (vkAllocateDescriptorSets(context_->GetDevice(), &alloc_info, &descriptor_set_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set");
    }

    // Update the descriptor set
    if (camera_texture_view_ != VK_NULL_HANDLE) {
        VkDescriptorImageInfo image_info{};
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info.imageView = camera_texture_view_;
        image_info.sampler = camera_sampler_;

        VkWriteDescriptorSet descriptor_write{};
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.dstSet = descriptor_set_;
        descriptor_write.dstBinding = 0;
        descriptor_write.dstArrayElement = 0;
        descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_write.descriptorCount = 1;
        descriptor_write.pImageInfo = &image_info;

        vkUpdateDescriptorSets(context_->GetDevice(), 1, &descriptor_write, 0, nullptr);
    }
}

void ARCameraTextureManager::ResizeStagingBufferIfNeeded(size_t required_size) {
    if (required_size <= current_staging_buffer_size_ && staging_buffer_ != VK_NULL_HANDLE) {
        return; // Current buffer is big enough
    }

    // Clean up old buffer if it exists
    if (staging_buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_->GetDevice(), staging_buffer_, nullptr);
        vkFreeMemory(context_->GetDevice(), staging_buffer_memory_, nullptr);
    }

    // Create new buffer
    context_->CreateBuffer(
            required_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &staging_buffer_,
            &staging_buffer_memory_
    );

    current_staging_buffer_size_ = required_size;
}

void ARCameraTextureManager::CleanupTextureResources() {
    VkDevice device = context_->GetDevice();

    // Clean up descriptor resources
    if (descriptor_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptor_pool_, nullptr);
        descriptor_pool_ = VK_NULL_HANDLE;
        descriptor_set_ = VK_NULL_HANDLE; // Destroyed with the pool
    }

    if (descriptor_set_layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, descriptor_set_layout_, nullptr);
        descriptor_set_layout_ = VK_NULL_HANDLE;
    }

    // Clean up texture resources
    if (camera_sampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device, camera_sampler_, nullptr);
        camera_sampler_ = VK_NULL_HANDLE;
    }

    if (camera_texture_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device, camera_texture_view_, nullptr);
        camera_texture_view_ = VK_NULL_HANDLE;
    }

    if (camera_texture_ != VK_NULL_HANDLE) {
        vkDestroyImage(device, camera_texture_, nullptr);
        camera_texture_ = VK_NULL_HANDLE;
    }

    if (camera_texture_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device, camera_texture_memory_, nullptr);
        camera_texture_memory_ = VK_NULL_HANDLE;
    }

    // Clean up staging buffer
    if (staging_buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, staging_buffer_, nullptr);
        staging_buffer_ = VK_NULL_HANDLE;
    }

    if (staging_buffer_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device, staging_buffer_memory_, nullptr);
        staging_buffer_memory_ = VK_NULL_HANDLE;
    }

    current_staging_buffer_size_ = 0;
    texture_ready_ = false;
}

VkDescriptorSet ARCameraTextureManager::GetCameraTextureDescriptorSet() const {
    return descriptor_set_;
}

void ARCameraTextureManager::SetCameraIntrinsics(float focal_length_x, float focal_length_y,
                                                 float principal_point_x, float principal_point_y) {
    focal_length_x_ = focal_length_x;
    focal_length_y_ = focal_length_y;
    principal_point_x_ = principal_point_x;
    principal_point_y_ = principal_point_y;
}
