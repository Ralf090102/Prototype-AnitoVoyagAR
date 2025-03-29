#include "VulkanSwapchainContext.hpp"
#include <algorithm>
#include <stdexcept>
#include <spdlog/spdlog.h>

VulkanSwapchainContext::VulkanSwapchainContext(
        std::shared_ptr<vulkan::VulkanRenderingContext> vulkan_rendering_context,
        uint32_t capacity,
        const XrSwapchainCreateInfo &swapchain_create_info)
        : rendering_context_(std::move(vulkan_rendering_context)),
          swapchain_image_format_(static_cast<VkFormat>(swapchain_create_info.format)),
          swapchain_extent_{swapchain_create_info.width, swapchain_create_info.height} {

    spdlog::info("Creating VulkanSwapchainContext for format {}, size {}x{}",
         swapchain_create_info.format, swapchain_create_info.width, swapchain_create_info.height);

    // Allocate swapchain images for OpenXR to fill in
    swapchain_images_.resize(capacity, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});

    // No image views or framebuffers yet - these will be created after OpenXR provides the images

    // Create color and depth resources for MSAA (if using)
    CreateColorResources();
    CreateDepthResources();

    // Create command buffers for rendering
    CreateCommandBuffers();

    // Create synchronization objects
    CreateSyncObjects();

    inited_ = true;

    spdlog::info("VulkanSwapchainContext creation complete");
}

XrSwapchainImageBaseHeader *VulkanSwapchainContext::GetFirstImagePointer() {
    return reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchain_images_.data());
}

void VulkanSwapchainContext::InitSwapchainImageViews() {
    spdlog::info("Initializing swapchain image views for {} images", swapchain_images_.size());

    // Create image views for each swapchain image
    swapchain_image_views_.resize(swapchain_images_.size());

    for (size_t i = 0; i < swapchain_images_.size(); i++) {
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = swapchain_images_[i].image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = swapchain_image_format_;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        if (vkCreateImageView(rendering_context_->GetDevice(), &view_info, nullptr, &swapchain_image_views_[i]) != VK_SUCCESS) {
            spdlog::error("Failed to create image views for swapchain image {}", i);
            throw std::runtime_error("Failed to create image views for swapchain");
        }
    }

    // Create framebuffers
    CreateFrameBuffers();

    spdlog::info("Swapchain image views and framebuffers initialized");
}

void VulkanSwapchainContext::Draw(
        uint32_t image_index,
        std::shared_ptr<vulkan::VulkanRenderingPipeline> pipeline,
        uint32_t index_count,
        std::vector<glm::mat4> transforms) {

    // Ensure we have a valid image index
    if (image_index >= swapchain_images_.size()) {
        spdlog::error("Invalid image index: {} (max: {})", image_index, swapchain_images_.size() - 1);
        return;
    }

    current_fame_ = (current_fame_ + 1) % max_frames_in_flight_;

    // Wait until previous frame is done with this fence
    vkWaitForFences(rendering_context_->GetDevice(), 1, &in_flight_fences_[current_fame_], VK_TRUE, UINT64_MAX);

    // Check if this image is already in use by another frame
    if (images_in_flight_[image_index] != VK_NULL_HANDLE) {
        vkWaitForFences(rendering_context_->GetDevice(), 1, &images_in_flight_[image_index], VK_TRUE, UINT64_MAX);
    }
    // Mark this image as now being in use by this frame
    images_in_flight_[image_index] = in_flight_fences_[current_fame_];

    // Configure command buffer
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    begin_info.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(graphics_command_buffers_[current_fame_], &begin_info) != VK_SUCCESS) {
        spdlog::error("Failed to begin recording command buffer");
        return;
    }

    // Begin renderpass
    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = rendering_context_->GetRenderPass();
    render_pass_info.framebuffer = swapchain_frame_buffers_[image_index];
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = swapchain_extent_;

    // Clear values for color and depth
    std::array<VkClearValue, 2> clear_values{};
    clear_values[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}}; // Transparent for AR
    clear_values[1].depthStencil = {1.0f, 0};

    render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
    render_pass_info.pClearValues = clear_values.data();

    vkCmdBeginRenderPass(graphics_command_buffers_[current_fame_], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    // Set dynamic state
    viewport_ = {
            0.0f, 0.0f,
            static_cast<float>(swapchain_extent_.width), static_cast<float>(swapchain_extent_.height),
            0.0f, 1.0f
    };
    vkCmdSetViewport(graphics_command_buffers_[current_fame_], 0, 1, &viewport_);

    scissor_ = {{0, 0}, swapchain_extent_};
    vkCmdSetScissor(graphics_command_buffers_[current_fame_], 0, 1, &scissor_);

    // Draw using the provided pipeline
    if (pipeline) {
        pipeline->Bind(graphics_command_buffers_[current_fame_]);
        pipeline->Draw(graphics_command_buffers_[current_fame_], index_count, transforms);
    }

    // End renderpass and command buffer
    vkCmdEndRenderPass(graphics_command_buffers_[current_fame_]);

    if (vkEndCommandBuffer(graphics_command_buffers_[current_fame_]) != VK_SUCCESS) {
        spdlog::error("Failed to record command buffer");
        return;
    }

    // Submit command buffer
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &graphics_command_buffers_[current_fame_];

    // Reset the fence for this frame
    vkResetFences(rendering_context_->GetDevice(), 1, &in_flight_fences_[current_fame_]);

    // Submit to queue
    if (vkQueueSubmit(rendering_context_->GetGraphicsQueue(), 1, &submit_info, in_flight_fences_[current_fame_]) != VK_SUCCESS) {
        spdlog::error("Failed to submit draw command buffer");
        throw std::runtime_error("Failed to submit draw command buffer");
    }
}

bool VulkanSwapchainContext::IsInited() const {
    return inited_;
}

VulkanSwapchainContext::~VulkanSwapchainContext() {
    // Wait for device to be idle before cleanup
    rendering_context_->WaitForGpuIdle();

    // Clean up synchronization objects
    for (size_t i = 0; i < max_frames_in_flight_; i++) {
        vkDestroyFence(rendering_context_->GetDevice(), in_flight_fences_[i], nullptr);
    }

    // Clean up framebuffers
    for (auto framebuffer : swapchain_frame_buffers_) {
        vkDestroyFramebuffer(rendering_context_->GetDevice(), framebuffer, nullptr);
    }

    // Clean up image views
    for (auto image_view : swapchain_image_views_) {
        vkDestroyImageView(rendering_context_->GetDevice(), image_view, nullptr);
    }

    // Clean up color and depth resources
    if (color_image_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(rendering_context_->GetDevice(), color_image_view_, nullptr);
    }
    if (color_image_ != VK_NULL_HANDLE) {
        vkDestroyImage(rendering_context_->GetDevice(), color_image_, nullptr);
    }
    if (color_image_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(rendering_context_->GetDevice(), color_image_memory_, nullptr);
    }

    if (depth_image_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(rendering_context_->GetDevice(), depth_image_view_, nullptr);
    }
    if (depth_image_ != VK_NULL_HANDLE) {
        vkDestroyImage(rendering_context_->GetDevice(), depth_image_, nullptr);
    }
    if (depth_image_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(rendering_context_->GetDevice(), depth_image_memory_, nullptr);
    }
}

void VulkanSwapchainContext::CreateColorResources() {
    // Create a multisampled color buffer if MSAA is enabled
    VkSampleCountFlagBits msaa_samples = rendering_context_->GetRecommendedMsaaSamples();

    // If no MSAA, we don't need color resources
    if (msaa_samples == VK_SAMPLE_COUNT_1_BIT) {
        return;
    }

    // Create multisampled color image
    rendering_context_->CreateImage(
            swapchain_extent_.width,
            swapchain_extent_.height,
            msaa_samples,
            swapchain_image_format_,
            VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &color_image_,
            &color_image_memory_
    );

    // Create color image view
    rendering_context_->CreateImageView(
            color_image_,
            swapchain_image_format_,
            VK_IMAGE_ASPECT_COLOR_BIT,
            &color_image_view_
    );

    spdlog::info("Created multisampled color resources for MSAA");
}

void VulkanSwapchainContext::CreateDepthResources() {
    // Find a suitable depth format
    VkFormat depth_format = rendering_context_->GetDepthAttachmentFormat();

    // Create depth image
    VkSampleCountFlagBits msaa_samples = rendering_context_->GetRecommendedMsaaSamples();
    rendering_context_->CreateImage(
            swapchain_extent_.width,
            swapchain_extent_.height,
            msaa_samples,
            depth_format,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &depth_image_,
            &depth_image_memory_
    );

    // Create depth image view
    rendering_context_->CreateImageView(
            depth_image_,
            depth_format,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            &depth_image_view_
    );

    spdlog::info("Created depth resources with format {}", static_cast<int>(depth_format));
}

void VulkanSwapchainContext::CreateFrameBuffers() {
    swapchain_frame_buffers_.resize(swapchain_image_views_.size());

    for (size_t i = 0; i < swapchain_image_views_.size(); i++) {
        // Set up attachments based on whether we're using MSAA
        VkSampleCountFlagBits msaa_samples = rendering_context_->GetRecommendedMsaaSamples();
        std::vector<VkImageView> attachments;

        if (msaa_samples != VK_SAMPLE_COUNT_1_BIT) {
            // With MSAA: color, depth, resolve
            attachments = {color_image_view_, depth_image_view_, swapchain_image_views_[i]};
        } else {
            // Without MSAA: color, depth
            attachments = {swapchain_image_views_[i], depth_image_view_};
        }

        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = rendering_context_->GetRenderPass();
        framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebuffer_info.pAttachments = attachments.data();
        framebuffer_info.width = swapchain_extent_.width;
        framebuffer_info.height = swapchain_extent_.height;
        framebuffer_info.layers = 1;

        if (vkCreateFramebuffer(rendering_context_->GetDevice(), &framebuffer_info, nullptr, &swapchain_frame_buffers_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }

    spdlog::info("Created {} framebuffers", swapchain_frame_buffers_.size());
}

void VulkanSwapchainContext::CreateCommandBuffers() {
    graphics_command_buffers_.resize(max_frames_in_flight_);

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = rendering_context_->GetGraphicsPool();
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = static_cast<uint32_t>(graphics_command_buffers_.size());

    if (vkAllocateCommandBuffers(rendering_context_->GetDevice(), &alloc_info, graphics_command_buffers_.data()) != VK_SUCCESS) {
        spdlog::error("Failed to allocate command buffers");
        throw std::runtime_error("Failed to allocate command buffers");
    }

    spdlog::info("Created {} command buffers", graphics_command_buffers_.size());
}

void VulkanSwapchainContext::CreateSyncObjects() {
    // Create synchronization objects
    in_flight_fences_.resize(max_frames_in_flight_);

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled so first frame doesn't wait

    for (size_t i = 0; i < max_frames_in_flight_; i++) {
        if (vkCreateFence(rendering_context_->GetDevice(), &fence_info, nullptr, &in_flight_fences_[i]) != VK_SUCCESS) {
            spdlog::error("Failed to create synchronization objects for frame {}", i);
            throw std::runtime_error("Failed to create synchronization objects");
        }
    }

    // Initialize images_in_flight with null handles
    images_in_flight_.resize(swapchain_images_.size(), VK_NULL_HANDLE);

    spdlog::info("Created synchronization objects for {} frames in flight", max_frames_in_flight_);
}
