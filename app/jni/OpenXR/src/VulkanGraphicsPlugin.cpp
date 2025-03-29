#include "VulkanGraphicsPlugin.hpp"
#include "OpenXRUtils.hpp"

#include <spdlog/spdlog.h>

// Graphics plugin factory function
std::shared_ptr<GraphicsPlugin> CreateGraphicsPlugin() {
    return std::make_shared<VulkanGraphicsPlugin>();
}

VulkanGraphicsPlugin::VulkanGraphicsPlugin() {
    // Constructor empty - initialization happens in InitializeDevice
}

VulkanGraphicsPlugin::~VulkanGraphicsPlugin() {
    DeinitDevice();
}

std::vector<std::string> VulkanGraphicsPlugin::GetOpenXrInstanceExtensions() const {
    return {
            XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME
    };
}

void VulkanGraphicsPlugin::InitializeDevice(XrInstance instance, XrSystemId system_id) {
    // 1. Get Vulkan instance extensions required by OpenXR
    PFN_xrGetVulkanInstanceExtensionsKHR xrGetVulkanInstanceExtensionsKHR = nullptr;
    CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanInstanceExtensionsKHR",
                                      reinterpret_cast<PFN_xrVoidFunction*>(&xrGetVulkanInstanceExtensionsKHR)));

    uint32_t extension_count = 0;
    CHECK_XRCMD(xrGetVulkanInstanceExtensionsKHR(instance, system_id, 0, &extension_count, nullptr));
    std::vector<char> extensions_raw(extension_count);
    CHECK_XRCMD(xrGetVulkanInstanceExtensionsKHR(instance, system_id, extension_count,
                                                 &extension_count, extensions_raw.data()));

    // Parse extension string
    std::vector<const char*> required_extensions;
    char* start = extensions_raw.data();
    while (start < extensions_raw.data() + extension_count) {
        required_extensions.push_back(start);
        start += strlen(start) + 1;
    }

    // 2. Create Vulkan instance with VkBootstrap
    vkb::InstanceBuilder instance_builder;
    auto instance_ret = instance_builder
            .set_app_name("Anito VoyagAR")
            .request_validation_layers(true)  // Enable for debug, disable for release
            .require_api_version(1, 1, 0)
            .enable_extensions(required_extensions)
            .build();

    if (!instance_ret) {
        spdlog::error("Failed to create Vulkan instance: {}", instance_ret.error().message());
        throw std::runtime_error("Failed to create Vulkan instance");
    }
    vkb_instance = instance_ret.value();

    // 3. Get physical device from OpenXR
    PFN_xrGetVulkanGraphicsDeviceKHR xrGetVulkanGraphicsDeviceKHR = nullptr;
    CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsDeviceKHR",
                                      reinterpret_cast<PFN_xrVoidFunction*>(&xrGetVulkanGraphicsDeviceKHR)));

    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    CHECK_XRCMD(xrGetVulkanGraphicsDeviceKHR(instance, system_id, vkb_instance.instance, &physical_device));

    // 4. Get device extensions required by OpenXR
    PFN_xrGetVulkanDeviceExtensionsKHR xrGetVulkanDeviceExtensionsKHR = nullptr;
    CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanDeviceExtensionsKHR",
                                      reinterpret_cast<PFN_xrVoidFunction*>(&xrGetVulkanDeviceExtensionsKHR)));

    extension_count = 0;
    CHECK_XRCMD(xrGetVulkanDeviceExtensionsKHR(instance, system_id, 0, &extension_count, nullptr));
    extensions_raw.resize(extension_count);
    CHECK_XRCMD(xrGetVulkanDeviceExtensionsKHR(instance, system_id, extension_count,
                                               &extension_count, extensions_raw.data()));

    // Parse device extension string
    std::vector<const char*> device_extensions;
    start = extensions_raw.data();
    while (start < extensions_raw.data() + extension_count) {
        device_extensions.push_back(start);
        start += strlen(start) + 1;
    }

    // 5. Create device with VkBootstrap
    vkb::PhysicalDeviceSelector selector{vkb_instance};
    selector.set_minimum_version(1, 3)
            .set_required_features_13({ .dynamicRendering = true })
            .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
            .add_required_extensions(device_extensions)
            .select();

    // Add optional extensions if available
    std::vector<const char*> optional_extensions = {
            VK_KHR_MAINTENANCE2_EXTENSION_NAME,
            VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME
    };
    for (auto& ext : optional_extensions) {
        selector.add_required_extension(ext);
    }

    // For Vulkan 1.2+ devices, add multiview if available
    VkPhysicalDeviceFeatures2 features2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    VkPhysicalDeviceMultiviewFeatures multiview_features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES};
    features2.pNext = &multiview_features;

    vkGetPhysicalDeviceFeatures2(physical_device, &features2);
    if (multiview_features.multiview) {
        // Device supports multiview
        selector.add_required_extension(VK_KHR_MULTIVIEW_EXTENSION_NAME);
    }


    auto selector_ret = selector.select();
    if (!selector_ret) {
        spdlog::error("Failed to select physical device: {}", selector_ret.error().message());
        throw std::runtime_error("Failed to select physical device");
    }

    vkb::DeviceBuilder device_builder{selector_ret.value()};
    auto device_ret = device_builder.build();
    if (!device_ret) {
        spdlog::error("Failed to create device: {}", device_ret.error().message());
        throw std::runtime_error("Failed to create device");
    }
    vkb_device = device_ret.value();

    // 6. Get queue
    auto queue_ret = vkb_device.get_queue(vkb::QueueType::graphics);
    if (!queue_ret) {
        spdlog::error("Failed to get graphics queue: {}", queue_ret.error().message());
        throw std::runtime_error("Failed to get graphics queue");
    }
    graphics_queue = queue_ret.value();

    auto queue_index_ret = vkb_device.get_queue_index(vkb::QueueType::graphics);
    if (!queue_index_ret) {
        spdlog::error("Failed to get graphics queue index: {}", queue_index_ret.error().message());
        throw std::runtime_error("Failed to get graphics queue index");
    }
    graphics_queue_family = queue_index_ret.value();

    // 7. Create command pool
    VkCommandPoolCreateInfo cmd_pool_info = {};
    cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmd_pool_info.queueFamilyIndex = graphics_queue_family;

    if (vkCreateCommandPool(vkb_device.device, &cmd_pool_info, nullptr, &command_pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }

    // 8. Create rendering resources
    CreateRenderResources();

    // 9. Initialize AR-specific resources
    InitARResources();
}

const XrBaseInStructure* VulkanGraphicsPlugin::GetGraphicsBinding() const {
    // Fill in the graphics binding structure for OpenXR
    graphics_binding = {XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};  // Updated type
    graphics_binding.instance = vkb_instance.instance;
    graphics_binding.physicalDevice = vkb_device.physical_device;
    graphics_binding.device = vkb_device.device;
    graphics_binding.queueFamilyIndex = graphics_queue_family;
    graphics_binding.queueIndex = 0;  // Assume first queue in family

    return reinterpret_cast<const XrBaseInStructure*>(&graphics_binding);
}

int64_t VulkanGraphicsPlugin::SelectSwapchainFormat(const std::vector<int64_t>& runtime_formats) {
    // Preferred formats for AR (typically SRGB or UNORM with alpha)
    std::vector<VkFormat> preferred_formats = {
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_FORMAT_B8G8R8A8_SRGB,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_B8G8R8A8_UNORM
    };

    // Find the first format that matches our preferences
    for (auto preferred_format : preferred_formats) {
        auto it = std::find(runtime_formats.begin(), runtime_formats.end(), preferred_format);
        if (it != runtime_formats.end()) {
            return *it;
        }
    }

    // If none of our preferred formats are available, use the first one
    if (!runtime_formats.empty()) {
        return runtime_formats[0];
    }

    // Fallback format if the list is empty (shouldn't happen)
    return VK_FORMAT_R8G8B8A8_SRGB;
}

XrSwapchainImageBaseHeader* VulkanGraphicsPlugin::AllocateSwapchainImageStructs(
        uint32_t capacity, const XrSwapchainCreateInfo& swapchain_create_info) {

    // Create a context for this swapchain if it doesn't exist
    auto& context = swapchain_image_contexts[swapchain_create_info.width];
    if (!context) {
        context = std::make_shared<SwapchainImageContext>();
    }

    // Allocate and set up the image structs for OpenXR to fill
    context->vulkan_images.resize(capacity, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});

    // Pre-allocate space for views and framebuffers
    context->image_views.resize(capacity, VK_NULL_HANDLE);
    context->framebuffers.resize(capacity, VK_NULL_HANDLE);

    return reinterpret_cast<XrSwapchainImageBaseHeader*>(context->vulkan_images.data());
}

void VulkanGraphicsPlugin::SwapchainImageStructsReady(XrSwapchainImageBaseHeader* images) {
    // Nothing specific to do here, the images are now filled with valid VkImage handles
}

void VulkanGraphicsPlugin::CreateRenderResources() {
    // Create render pass (you already have this part)
    // Create descriptor set layout for camera texture
    VkDescriptorSetLayoutBinding camera_texture_binding{};
    camera_texture_binding.binding = 0;
    camera_texture_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    camera_texture_binding.descriptorCount = 1;
    camera_texture_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &camera_texture_binding;

    if (vkCreateDescriptorSetLayout(vkb_device.device, &layout_info, nullptr, &descriptor_set_layout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout");
    }

    // Create descriptor pool
    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = 10; // Allocate enough for multiple textures

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    pool_info.maxSets = 10;

    if (vkCreateDescriptorPool(vkb_device.device, &pool_info, nullptr, &descriptor_pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool");
    }

    // Create pipeline layout - just use the descriptor set layout for now
    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &descriptor_set_layout;

    if (vkCreatePipelineLayout(vkb_device.device, &pipeline_layout_info, nullptr, &pipeline_layout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    // TODO: We'll create the actual pipeline when we have the shaders loaded
    // This would happen in a production app, but for now we're not creating the full pipeline
}

void VulkanGraphicsPlugin::InitARResources() {
    // For now, we'll just set up a placeholder camera texture
    // In a real implementation, this would be filled by ARCore data

    // Create camera texture sampler
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = 16.0f;

    if (vkCreateSampler(vkb_device.device, &sampler_info, nullptr, &camera_texture.sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create camera texture sampler");
    }

    // TODO: The actual camera texture and its descriptor set will be created
    // when we get the first camera frame from ARCore
}

void VulkanGraphicsPlugin::RenderCameraBackground(VkCommandBuffer cmd_buffer, VkImage target_image) {
    // TODO: If we don't have a camera texture yet, skip rendering the background
    if (camera_texture.image == VK_NULL_HANDLE) {
        return;
    }

    // In a real implementation, this would render a fullscreen quad with the camera texture
    // For now, we'll just add a comment as a placeholder

    // If we had a pipeline for the camera background:
    // 1. Bind the camera background pipeline
    // 2. Bind the camera texture descriptor set
    // 3. Draw a fullscreen quad

    // Example code (commented out since we don't have the actual pipeline yet):
    /*
    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, camera_background_pipeline);
    vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           camera_background_pipeline_layout, 0, 1, &camera_texture.descriptor_set, 0, nullptr);
    vkCmdDraw(cmd_buffer, 6, 1, 0, 0); // Fullscreen quad as two triangles
    */
}
void VulkanGraphicsPlugin::RenderView(
        const XrCompositionLayerProjectionView& layer_view,
        XrSwapchainImageBaseHeader* swapchain_images,
        const uint32_t image_index,
        const std::vector<math::Transform>& cube_transforms) {

    // Get the appropriate swapchain context based on view width
    auto& context = swapchain_image_contexts[layer_view.subImage.imageRect.extent.width];
    if (!context) {
        spdlog::error("No swapchain context found for width {}",
                      layer_view.subImage.imageRect.extent.width);
        return;
    }

    // Get the Vulkan image from the swapchain
    auto* vulkan_images = reinterpret_cast<XrSwapchainImageVulkan2KHR*>(swapchain_images);
    VkImage target_image = vulkan_images[image_index].image;

    // Create or get image view
    if (context->image_views[image_index] == VK_NULL_HANDLE) {
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = target_image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = VK_FORMAT_R8G8B8A8_SRGB; // Must match swapchain format
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        if (vkCreateImageView(vkb_device.device, &view_info, nullptr,
                              &context->image_views[image_index]) != VK_SUCCESS) {
            spdlog::error("Failed to create image view");
            return;
        }
    }

    // Create or get framebuffer
    if (context->framebuffers[image_index] == VK_NULL_HANDLE) {
        VkFramebufferCreateInfo fb_info = {};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.renderPass = render_pass;
        fb_info.attachmentCount = 1;
        fb_info.pAttachments = &context->image_views[image_index];
        fb_info.width = layer_view.subImage.imageRect.extent.width;
        fb_info.height = layer_view.subImage.imageRect.extent.height;
        fb_info.layers = 1;

        if (vkCreateFramebuffer(vkb_device.device, &fb_info, nullptr,
                                &context->framebuffers[image_index]) != VK_SUCCESS) {
            spdlog::error("Failed to create framebuffer");
            return;
        }
    }

    // Allocate command buffer
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer cmd_buffer;
    if (vkAllocateCommandBuffers(vkb_device.device, &alloc_info, &cmd_buffer) != VK_SUCCESS) {
        spdlog::error("Failed to allocate command buffer");
        return;
    }

    // Begin command buffer
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd_buffer, &begin_info);

    // Transition image layout for rendering
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = target_image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(cmd_buffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Begin render pass
    VkRenderPassBeginInfo render_pass_begin = {};
    render_pass_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin.renderPass = render_pass;
    render_pass_begin.framebuffer = context->framebuffers[image_index];
    render_pass_begin.renderArea.offset = {0, 0};
    render_pass_begin.renderArea.extent = {
            static_cast<uint32_t>(layer_view.subImage.imageRect.extent.width),
            static_cast<uint32_t>(layer_view.subImage.imageRect.extent.height)
    };

    // For AR, use a transparent clear color so we see the camera feed
    VkClearValue clear_value = {0.0f, 0.0f, 0.0f, 0.0f};
    render_pass_begin.clearValueCount = 1;
    render_pass_begin.pClearValues = &clear_value;

    vkCmdBeginRenderPass(cmd_buffer, &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE);

    // AR-specific: Render camera background
    RenderCameraBackground(cmd_buffer, target_image);

    // TODO: Render your AR content here
    // This would include 3D models positioned based on AR tracking

    // End render pass
    vkCmdEndRenderPass(cmd_buffer);

    // End and submit command buffer
    vkEndCommandBuffer(cmd_buffer);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buffer;

    vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue);  // Simple synchronization for now

    // Free command buffer
    vkFreeCommandBuffers(vkb_device.device, command_pool, 1, &cmd_buffer);
}

void VulkanGraphicsPlugin::CleanupRenderResources() {
    if (render_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(vkb_device.device, render_pass, nullptr);
        render_pass = VK_NULL_HANDLE;
    }

    if (pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(vkb_device.device, pipeline_layout, nullptr);
        pipeline_layout = VK_NULL_HANDLE;
    }

    if (graphics_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(vkb_device.device, graphics_pipeline, nullptr);
        graphics_pipeline = VK_NULL_HANDLE;
    }

    if (descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(vkb_device.device, descriptor_set_layout, nullptr);
        descriptor_set_layout = VK_NULL_HANDLE;
    }

    if (descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(vkb_device.device, descriptor_pool, nullptr);
        descriptor_pool = VK_NULL_HANDLE;
    }

    // Clean up swapchain resources
    for (auto& [width, context] : swapchain_image_contexts) {
        for (auto framebuffer : context->framebuffers) {
            if (framebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(vkb_device.device, framebuffer, nullptr);
            }
        }

        for (auto image_view : context->image_views) {
            if (image_view != VK_NULL_HANDLE) {
                vkDestroyImageView(vkb_device.device, image_view, nullptr);
            }
        }
    }
    swapchain_image_contexts.clear();
}

void VulkanGraphicsPlugin::DeinitDevice() {
    // Clean up Vulkan resources
    CleanupRenderResources();

    if (command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(vkb_device.device, command_pool, nullptr);
        command_pool = VK_NULL_HANDLE;
    }

    // Clean up device and instance
    if (vkb_device.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(vkb_device.device);
        destroy_device(vkb_device);
    }

    if (vkb_instance.instance != VK_NULL_HANDLE) {
        destroy_instance(vkb_instance);
    }
}