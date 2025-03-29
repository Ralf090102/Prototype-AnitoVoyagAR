#include "AR/ARBackgroundPipeline.hpp"
#include "../../include/lib/VulkanUtils.hpp"

#include <array>

// Simple vertex with position and texture coordinates
struct BackgroundVertex {
    glm::vec3 pos;
    glm::vec2 texCoord;
};

// Shader code (will be replaced with proper SPIR-V loading)
// This is just placeholder - you'll need actual compiled SPIR-V
const uint32_t background_vert_spv[] = { /* your compiled SPIR-V here */ };
const uint32_t background_frag_spv[] = { /* your compiled SPIR-V here */ };

ARBackgroundPipeline::ARBackgroundPipeline(std::shared_ptr<vulkan::VulkanRenderingContext> context,
                                           std::shared_ptr<ARCameraTextureManager> camera_texture_manager)
        : context_(context), camera_texture_manager_(camera_texture_manager) {

    // Create the pipeline components
    CreateShaderModules();
    CreatePipeline();
    CreateGeometry();

    is_initialized_ = true;
}

ARBackgroundPipeline::~ARBackgroundPipeline() {
    CleanupResources();
}

void ARBackgroundPipeline::CreateShaderModules() {
    VkDevice device = context_->GetDevice();

    // Create vertex shader module
    VkShaderModuleCreateInfo vert_create_info{};
    vert_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vert_create_info.codeSize = sizeof(background_vert_spv);
    vert_create_info.pCode = background_vert_spv;

    if (vkCreateShaderModule(device, &vert_create_info, nullptr, &vert_shader_module_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create background vertex shader module");
    }

    // Create fragment shader module
    VkShaderModuleCreateInfo frag_create_info{};
    frag_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    frag_create_info.codeSize = sizeof(background_frag_spv);
    frag_create_info.pCode = background_frag_spv;

    if (vkCreateShaderModule(device, &frag_create_info, nullptr, &frag_shader_module_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create background fragment shader module");
    }
}

void ARBackgroundPipeline::CreatePipeline() {
    VkDevice device = context_->GetDevice();

    // Shader stage creation info
    VkPipelineShaderStageCreateInfo vert_stage_info{};
    vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage_info.module = vert_shader_module_;
    vert_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_stage_info{};
    frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage_info.module = frag_shader_module_;
    frag_stage_info.pName = "main";

    std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {vert_stage_info, frag_stage_info};

    // Vertex input state
    VkVertexInputBindingDescription binding_description{};
    binding_description.binding = 0;
    binding_description.stride = sizeof(BackgroundVertex);
    binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions{};
    // Position attribute
    attribute_descriptions[0].binding = 0;
    attribute_descriptions[0].location = 0;
    attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[0].offset = offsetof(BackgroundVertex, pos);
    // Texture coordinate attribute
    attribute_descriptions[1].binding = 0;
    attribute_descriptions[1].location = 1;
    attribute_descriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_descriptions[1].offset = offsetof(BackgroundVertex, texCoord);

    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &binding_description;
    vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
    vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

    // Input assembly state
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    // Viewport state
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    // Rasterization state
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE; // Don't cull for background
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisample state
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth stencil state
    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = VK_FALSE; // No depth testing for background
    depth_stencil.depthWriteEnable = VK_FALSE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.stencilTestEnable = VK_FALSE;

    // Color blend state
    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    // Dynamic state
    std::array<VkDynamicState, 2> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();

    // Pipeline layout
    VkDescriptorSetLayout descriptor_layouts[] = {camera_texture_manager_->GetDescriptorSetLayout()};

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = descriptor_layouts;

    if (vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create background pipeline layout");
    }

    // Create the graphics pipeline
    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages.data();
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = pipeline_layout_;
    pipeline_info.renderPass = context_->GetRenderPass();
    pipeline_info.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create background graphics pipeline");
    }
}

void ARBackgroundPipeline::CreateGeometry() {
    // Create a simple full-screen quad for the background
    std::array<BackgroundVertex, 6> vertices = {
            // First triangle
            BackgroundVertex{{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}}, // bottom left
            BackgroundVertex{{ 1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}}, // bottom right
            BackgroundVertex{{ 1.0f,  1.0f, 0.0f}, {1.0f, 1.0f}}, // top right
            // Second triangle
            BackgroundVertex{{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}}, // bottom left
            BackgroundVertex{{ 1.0f,  1.0f, 0.0f}, {1.0f, 1.0f}}, // top right
            BackgroundVertex{{-1.0f,  1.0f, 0.0f}, {0.0f, 1.0f}}  // top left
    };

    vertex_count_ = static_cast<uint32_t>(vertices.size());
    VkDeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();

    // Create staging buffer
    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;

    context_->CreateBuffer(
            buffer_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &staging_buffer,
            &staging_buffer_memory
    );

    // Copy vertex data to staging buffer
    void* data;
    vkMapMemory(context_->GetDevice(), staging_buffer_memory, 0, buffer_size, 0, &data);
    memcpy(data, vertices.data(), buffer_size);
    vkUnmapMemory(context_->GetDevice(), staging_buffer_memory);

    // Create vertex buffer
    context_->CreateBuffer(
            buffer_size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &vertex_buffer_,
            &vertex_buffer_memory_
    );

    // Copy from staging to vertex buffer
    context_->CopyBuffer(staging_buffer, vertex_buffer_, buffer_size);

    // Clean up staging buffer
    vkDestroyBuffer(context_->GetDevice(), staging_buffer, nullptr);
    vkFreeMemory(context_->GetDevice(), staging_buffer_memory, nullptr);
}

void ARBackgroundPipeline::Render(VkCommandBuffer cmd_buffer, const glm::mat4& projection, const glm::mat4& view) {
    if (!IsReady()) {
        return;
    }

    // Bind the pipeline
    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    // Set viewport and scissor
    // These should be set dynamically based on the actual render target dimensions
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = 1.0f;  // Will be set by dynamic state
    viewport.height = 1.0f; // Will be set by dynamic state
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {1, 1}; // Will be set by dynamic state
    vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

    // Bind descriptor sets for camera texture
    VkDescriptorSet descriptor_set = camera_texture_manager_->GetCameraTextureDescriptorSet();
    vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_,
                            0, 1, &descriptor_set, 0, nullptr);

    // Bind vertex buffer
    VkBuffer vertex_buffers[] = {vertex_buffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buffer, 0, 1, vertex_buffers, offsets);

    // Draw the full-screen quad
    vkCmdDraw(cmd_buffer, vertex_count_, 1, 0, 0);
}

void ARBackgroundPipeline::CleanupResources() {
    VkDevice device = context_->GetDevice();

    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }

    if (pipeline_layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipeline_layout_, nullptr);
        pipeline_layout_ = VK_NULL_HANDLE;
    }

    if (vert_shader_module_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, vert_shader_module_, nullptr);
        vert_shader_module_ = VK_NULL_HANDLE;
    }

    if (frag_shader_module_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, frag_shader_module_, nullptr);
        frag_shader_module_ = VK_NULL_HANDLE;
    }

    if (vertex_buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, vertex_buffer_, nullptr);
        vertex_buffer_ = VK_NULL_HANDLE;
    }

    if (vertex_buffer_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device, vertex_buffer_memory_, nullptr);
        vertex_buffer_memory_ = VK_NULL_HANDLE;
    }

    is_initialized_ = false;
}
