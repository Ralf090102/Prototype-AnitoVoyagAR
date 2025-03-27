cmake_minimum_required(VERSION 3.22.1)

include(GenerateExportHeader)

add_library(vulkan_wrapper SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan.cpp
)

target_include_directories(vulkan_wrapper
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/generated
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/Vulkan
)

target_compile_definitions(vulkan_wrapper PRIVATE
        #TODO
)

target_link_libraries(vulkan_wrapper PUBLIC
        vk_bootstrap_headers
        Vulkan::Vulkan
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