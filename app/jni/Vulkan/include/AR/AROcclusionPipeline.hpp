#pragma once

#include "VulkanRenderingContext.hpp"
#include "ARDepthTextureManager.hpp"
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>
#include "glm/glm.hpp"

class AROcclusionPipeline {
public:
    AROcclusionPipeline(std::shared_ptr<vulkan::VulkanRenderingContext> context,
                        std::shared_ptr<ARDepthTextureManager> depth_texture_manager);
    ~AROcclusionPipeline();

    // Render the depth mask for occlusion
    void Render(VkCommandBuffer cmd_buffer,
                const glm::mat4& projection,
                const glm::mat4& view);

    // Return if the pipeline is ready to use
    bool IsReady() const {
        return is_initialized_ && depth_texture_manager_ && depth_texture_manager_->IsTextureReady();
    }

    // Set depth test parameters
    void SetDepthTestThreshold(float threshold) { depth_threshold_ = threshold; }
    void EnableDepthTest(bool enable) { depth_test_enabled_ = enable; }

private:
    // Create shader modules and pipeline
    void CreateShaderModules();
    void CreatePipeline();
    void CreateGeometry();
    void CleanupResources();

    // Context and dependencies
    std::shared_ptr<vulkan::VulkanRenderingContext> context_;
    std::shared_ptr<ARDepthTextureManager> depth_texture_manager_;

    // Pipeline resources
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkShaderModule vert_shader_module_ = VK_NULL_HANDLE;
    VkShaderModule frag_shader_module_ = VK_NULL_HANDLE;

    // Fullscreen quad geometry
    VkBuffer vertex_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertex_buffer_memory_ = VK_NULL_HANDLE;
    uint32_t vertex_count_ = 0;

    // Depth test parameters
    float depth_threshold_ = 0.1f;  // Threshold for depth testing (in meters)
    bool depth_test_enabled_ = true;

    // Uniform buffer for parameters
    VkBuffer uniform_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory uniform_buffer_memory_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set_ = VK_NULL_HANDLE;

    bool is_initialized_ = false;
};
