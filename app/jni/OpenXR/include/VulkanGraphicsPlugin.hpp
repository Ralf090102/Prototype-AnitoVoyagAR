#pragma once
#include "graphics_plugin.hpp"
#include "openxr_utils.hpp"
#include "ARTrackingManager.hpp"

#include <VkBootstrap.h>

#include <array>
#include <map>
#include <memory>

struct CameraTextureInfo {
    VkImage image;
    VkImageView view;
    VkSampler sampler;
    VkDescriptorSet descriptor_set;
};

class VulkanGraphicsPlugin : public GraphicsPlugin {
public:
    VulkanGraphicsPlugin();
    ~VulkanGraphicsPlugin() override;

    // GraphicsPlugin interface implementation
    std::vector<std::string> GetOpenXrInstanceExtensions() const override;
    void InitializeDevice(XrInstance instance, XrSystemId system_id) override;
    const XrBaseInStructure* GetGraphicsBinding() const override;
    int64_t SelectSwapchainFormat(const std::vector<int64_t>& runtime_formats) override;
    XrSwapchainImageBaseHeader* AllocateSwapchainImageStructs(uint32_t capacity,
                                                              const XrSwapchainCreateInfo& swapchain_create_info) override;
    void SwapchainImageStructsReady(XrSwapchainImageBaseHeader* images) override;
    void RenderView(const XrCompositionLayerProjectionView& layer_view,
                    XrSwapchainImageBaseHeader* swapchain_images,
                    const uint32_t image_index,
                    const std::vector<math::Transform>& cube_transforms) override;
    void DeinitDevice() override;

private:
    // Helper methods
    void CreateRenderResources();
    void CleanupRenderResources();
    void InitARResources();
    void RenderCameraBackground(VkCommandBuffer cmd, VkImage target_image);

    // AR Improvements
    bool UpdateCameraTexture(const uint8_t* image_data, int width, int height);
    void UpdateARPose(const math::Transform& camera_pose);
    void UpdateARPlanes(const std::vector<ARPlane>& planes);
    void EnableARDepthTesting(bool enable);
    void UpdateARDepthTexture(const uint8_t* depth_data, int width, int height);

    // Vulkan instance and device from VkBootstrap
    vkb::Instance vkb_instance;
    vkb::Device vkb_device;

    // Basic Vulkan handles
    VkQueue graphics_queue = VK_NULL_HANDLE;
    uint32_t graphics_queue_family = 0;
    VkCommandPool command_pool = VK_NULL_HANDLE;

    // Rendering resources
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline graphics_pipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

    // Image management
    struct SwapchainImageContext {
        std::vector<XrSwapchainImageVulkan2KHR> vulkan_images;
        std::vector<VkImageView> image_views;
        std::vector<VkFramebuffer> framebuffers;
    };
    std::map<uint32_t, std::shared_ptr<SwapchainImageContext>> swapchain_image_contexts;

    // AR-specific resources
    CameraTextureInfo camera_texture;

    // OpenXR binding structure
    mutable XrGraphicsBindingVulkan2KHR graphics_binding{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
};
