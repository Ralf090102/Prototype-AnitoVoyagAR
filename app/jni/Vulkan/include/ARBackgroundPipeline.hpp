#pragma once

#include "vulkan_rendering_pipeline.hpp"
#include "ARCameraTextureManager.hpp"

#include <memory>
#include <vector>
#include <glm/glm.hpp>

class ARBackgroundPipeline {
public:
    ARBackgroundPipeline(std::shared_ptr<vulkan::VulkanRenderingContext> context,
                         std::shared_ptr<ARCameraTextureManager> camera_texture_manager);
    ~ARBackgroundPipeline();

    // Render the camera background as a fullscreen quad
    void Render(VkCommandBuffer cmd_buffer, const glm::mat4& projection, const glm::mat4& view);

    // Return if the pipeline is ready to use
    bool IsReady() const { return is_initialized_ && camera_texture_manager_->IsTextureReady(); }

private:
    // Create shader modules and pipeline
    void CreateShaderModules();
    void CreatePipeline();
    void CreateGeometry();
    void CleanupResources();

    // Context and dependencies
    std::shared_ptr<vulkan::VulkanRenderingContext> context_;
    std::shared_ptr<ARCameraTextureManager> camera_texture_manager_;

    // Pipeline resources
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkShaderModule vert_shader_module_ = VK_NULL_HANDLE;
    VkShaderModule frag_shader_module_ = VK_NULL_HANDLE;

    // Fullscreen quad geometry
    VkBuffer vertex_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertex_buffer_memory_ = VK_NULL_HANDLE;
    uint32_t vertex_count_ = 0;

    bool is_initialized_ = false;
};
