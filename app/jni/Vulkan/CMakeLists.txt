cmake_minimum_required(VERSION 3.22.1)

include(GenerateExportHeader)

add_definitions(-DGLM_ENABLE_EXPERIMENTAL)


file(GLOB AR_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/AR/*.cpp)
file(GLOB LIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/*.cpp)
file(GLOB ROOT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)


add_library(vulkan_wrapper SHARED
    ${AR_SOURCES}
    ${LIB_SOURCES}
    ${ROOT_SOURCES}
)

target_include_directories(vulkan_wrapper
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/AR
        ${CMAKE_CURRENT_SOURCE_DIR}/include/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/include/export
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
        EXPORT_FILE_NAME ${CMAKE_CURRENT_SOURCE_DIR}/include/export/vulkan_wrapper_export.h
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