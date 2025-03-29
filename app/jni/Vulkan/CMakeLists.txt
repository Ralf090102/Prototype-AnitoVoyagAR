cmake_minimum_required(VERSION 3.22.1)

include(GenerateExportHeader)

add_definitions(-DGLM_ENABLE_EXPERIMENTAL)

add_library(vulkan_wrapper SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/src/data_type.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan_shader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan_utils.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan_buffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan_buffer_layout.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan_rendering_context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan_rendering_pipeline.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ARCameraTextureManager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ARTrackingManager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ARBackgroundPipeline.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AROcclusionPipeline.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ARDepthTextureManager.cpp
)

target_include_directories(vulkan_wrapper
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/generated
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/Vulkan
)

target_compile_definitions(vulkan_wrapper PRIVATE
        GLM_ENABLE_EXPERIMENTAL
)

target_link_libraries(vulkan_wrapper PUBLIC
        Vulkan::Vulkan
        vk-bootstrap
        magic_enum_headers
        spdlog_headers
        spirv-reflect-static
        glm_headers
)

# Generates Macros
generate_export_header(vulkan_wrapper
        BASE_NAME vulkan_wrapper
        EXPORT_FILE_NAME ${CMAKE_CURRENT_SOURCE_DIR}/include/vulkan_wrapper_export.h
)

install(TARGETS vulkan_wrapper
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../libs/Vulkan
        DESTINATION include/vulkan_wrapper
        FILES_MATCHING PATTERN "*.h*"
)