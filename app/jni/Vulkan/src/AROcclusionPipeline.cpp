#include "AROcclusionPipeline.hpp"
#include "vulkan_utils.hpp"
#include <array>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

// Simple vertex with position and texture coordinates
struct OcclusionVertex {
    glm::vec3 pos;
    glm::vec2 texCoord;
};

// Uniform buffer object
struct OcclusionUBO {
    glm::mat4 viewProjection;
    float depthThreshold;
    float padding[3]; // Align to 16 bytes
};

// Shader code (will be replaced with proper SPIR-V loading)
// This is just placeholder - you'll need actual compiled SPIR-V
const uint32_t occlusion_vert_spv[] = { /* your compiled SPIR-V here */ };
const uint32_t occlusion_frag_spv[] = { /* your compiled SPIR-V here */ };

AROcclusionPipeline::AROcclusionPipeline(
        std::shared_ptr<vulkan::VulkanRenderingContext> context,
        std::shared_ptr<ARDepthTextureManager> depth_texture_manager)
        : context_(context), depth_texture_manager_(depth_texture_manager) {

    // Create the pipeline components
    CreateShaderModules();
    CreatePipeline();
    CreateGeometry();

    is_initialized_ = true;
}

AROcclusionPipeline::~AROcclusionPipeline() {
    CleanupResources();
}

void AROcclusionPipeline::CreateShaderModules() {
    VkDevice device = context_->GetDevice();

    // Create vertex shader module
    VkShaderModuleCreateInfo vert_create_info{};
    vert_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vert_create_info.codeSize = sizeof(occlusion_vert_spv);
    vert_create_info.pCode = occlusion_vert_spv;

    if (vkCreateShaderModule(device, &vert_create_info, nullptr, &vert_shader_module_) != VK_SUCCESS) {
        spdlog::error("Failed to create occlusion vertex shader module");
        throw std::runtime_error("Failed to create occlusion vertex shader module");
    }

    // Create fragment shader module
    VkShaderModuleCreateInfo frag_create_info{};
    frag_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    frag_create_info.codeSize = sizeof(occlusion_frag_spv);
    frag_create_info.pCode = occlusion_frag_spv;

    if (vkCreateShaderModule(device, &frag_create_info, nullptr, &frag_shader_module_) != VK_SUCCESS) {
        spdlog::error("Failed to create occlusion fragment shader module");
        throw std::runtime_error("Failed to create occlusion fragment shader module");
    }

    // Create uniform buffer
    VkDeviceSize buffer_size = sizeof(OcclusionUBO);
    context_->CreateBuffer(
            buffer_size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &uniform_buffer_,
            &uniform_buffer_memory_
    );

    // Create descriptor set layout
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    // Binding 0: Uniform buffer
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[0].pImmutableSamplers = nullptr;

    // Binding 1: Depth texture
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_info.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &descriptor_set_layout_) != VK_SUCCESS) {
        spdlog::error("Failed to create descriptor set layout");
        throw std::runtime_error("Failed to create descriptor set layout");
    }

    // Create descriptor pool
    std::array<VkDescriptorPoolSize, 2> pool_sizes{};
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[0].descriptorCount = 1;
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = 1;

    if (vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptor_pool_) != VK_SUCCESS) {
        spdlog::error("Failed to create descriptor pool");
        throw std::runtime_error("Failed to create descriptor pool");
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool_;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &descriptor_set_layout_;

    if (vkAllocateDescriptorSets(device, &alloc_info, &descriptor_set_) != VK_SUCCESS) {
        spdlog::error("Failed to allocate descriptor set");
        throw std::runtime_error("Failed to allocate descriptor set");
    }

    // Update descriptor set - uniform buffer
    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = uniform_buffer_;
    buffer_info.offset = 0;
    buffer_info.range = sizeof(OcclusionUBO);

    VkWriteDescriptorSet write_descriptor{};
    write_descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_descriptor.dstSet = descriptor_set_;
    write_descriptor.dstBinding = 0;
    write_descriptor.dstArrayElement = 0;
    write_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write_descriptor.descriptorCount = 1;
    write_descriptor.pBufferInfo = &buffer_info;

    vkUpdateDescriptorSets(device, 1, &write_descriptor, 0, nullptr);

    spdlog::info("Occlusion shader modules and descriptors created");
}

void AROcclusionPipeline::CreatePipeline() {
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
    binding_description.stride = sizeof(OcclusionVertex);
    binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions{};
    // Position attribute
    attribute_descriptions[0].binding = 0;
    attribute_descriptions[0].location = 0;
    attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[0].offset = offsetof(OcclusionVertex, pos);
    // Texture coordinate attribute
    attribute_descriptions[1].binding = 0;
    attribute_descriptions[1].location = 1;
    attribute_descriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_descriptions[1].offset = offsetof(OcclusionVertex, texCoord);

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
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
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
    depth_stencil.depthTestEnable = VK_TRUE;
    depth_stencil.depthWriteEnable = VK_TRUE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.stencilTestEnable = VK_FALSE;

    // Color blend state
    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    // Dynamic state
    std::array<VkDynamicState, 2> dynamic_states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &descriptor_set_layout_;

    if (vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout_) != VK_SUCCESS) {
        spdlog::error("Failed to create occlusion pipeline layout");
        throw std::runtime_error("Failed to create occlusion pipeline layout");
    }

    // Create the graphics pipeline
    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
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
        spdlog::error("Failed to create occlusion graphics pipeline");
        throw std::runtime_error("Failed to create occlusion graphics pipeline");
    }

    spdlog::info("Occlusion pipeline created");
}

void AROcclusionPipeline::CreateGeometry() {
    // Create a simple full-screen quad
    std::array<OcclusionVertex, 6> vertices = {
            // First triangle
            OcclusionVertex{{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}}, // bottom left
            OcclusionVertex{{ 1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}}, // bottom right
            OcclusionVertex{{ 1.0f,  1.0f, 0.0f}, {1.0f, 1.0f}}, // top right
            // Second triangle
            OcclusionVertex{{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}}, // bottom left
            OcclusionVertex{{ 1.0f,  1.0f, 0.0f}, {1.0f, 1.0f}}, // top right
            OcclusionVertex{{-1.0f,  1.0f, 0.0f}, {0.0f, 1.0f}}  // top left
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

    spdlog::info("Occlusion geometry created");
}

void AROcclusionPipeline::Render(VkCommandBuffer cmd_buffer, const glm::mat4& projection, const glm::mat4& view) {
    if (!IsReady() || !depth_test_enabled_) {
        return;
    }

    // Update uniform buffer with view projection matrix and depth threshold
    OcclusionUBO ubo{};
    ubo.viewProjection = projection * view;
    ubo.depthThreshold = depth_threshold_;

    void* data;
    vkMapMemory(context_->GetDevice(), uniform_buffer_memory_, 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(context_->GetDevice(), uniform_buffer_memory_);

    // Update depth texture descriptor if needed
    if (depth_texture_manager_ && depth_texture_manager_->IsTextureReady()) {
        VkDescriptorImageInfo image_info{};
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info.imageView = depth_texture_manager_->GetDepthTextureView();
        image_info.sampler = depth_texture_manager_->GetDepthSampler();

        VkWriteDescriptorSet write_descriptor{};
        write_descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write_descriptor.dstSet = descriptor_set_;
        write_descriptor.dstBinding = 1;
        write_descriptor.dstArrayElement = 0;
        write_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write_descriptor.descriptorCount = 1;
        write_descriptor.pImageInfo = &image_info;

        vkUpdateDescriptorSets(context_->GetDevice(), 1, &write_descriptor, 0, nullptr);
    }

    // Bind the pipeline
    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    // Set viewport and scissor (dynamic state)
    // These should match the current render target dimensions
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = 1.0f;  // Will be set dynamically
    viewport.height = 1.0f; // Will be set dynamically
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {1, 1}; // Will be set dynamically
    vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

    // Bind descriptor set
    vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_,
                            0, 1, &descriptor_set_, 0, nullptr);

    // Bind vertex buffer
    VkBuffer vertex_buffers[] = {vertex_buffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buffer, 0, 1, vertex_buffers, offsets);

    // Draw the occlusion quad
    vkCmdDraw(cmd_buffer, vertex_count_, 1, 0, 0);

    spdlog::info("Occlusion rendered");
}

void AROcclusionPipeline::CleanupResources() {
    VkDevice device = context_->GetDevice();

    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }

    if (pipeline_layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipeline_layout_, nullptr);
        pipeline_layout_ = VK_NULL_HANDLE;
    }

    if (descriptor_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptor_pool_, nullptr);
        descriptor_pool_ = VK_NULL_HANDLE;
    }

    if (descriptor_set_layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, descriptor_set_layout_, nullptr);
        descriptor_set_layout_ = VK_NULL_HANDLE;
    }

    if (uniform_buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, uniform_buffer_, nullptr);
        uniform_buffer_ = VK_NULL_HANDLE;
    }

    if (uniform_buffer_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device, uniform_buffer_memory_, nullptr);
        uniform_buffer_memory_ = VK_NULL_HANDLE;
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

    spdlog::info("Occlusion resources cleaned up");
}