cmake_minimum_required(VERSION 3.22.1)

add_library(openxr_wrapper SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/src/openxr_utils.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform_android.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/openxr_program.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/TempGraphicsPlugin.cpp
)

target_include_directories(openxr_wrapper
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/generated
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/OpenXR
)

target_compile_definitions(openxr_wrapper PRIVATE XR_USE_PLATFORM_ANDROID
        XR_USE_GRAPHICS_API_VULKAN
        VK_USE_PLATFORM_ANDROID_KHR
)

target_link_libraries(openxr_wrapper PUBLIC
        openxr_loader
        spdlog_headers
        glm_headers
        vulkan_wrapper
        magic_enum_headers
        OpenXR_SDK_headers
)

# For Cross-Platform
install(TARGETS openxr_wrapper
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../libs/OpenXR
        DESTINATION include/openxr_wrapper
        FILES_MATCHING PATTERN "*.h*"
)
