cmake_minimum_required(VERSION 3.22.1)

add_library(openxr-wrapper SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/src/openxr_utils.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform_android.cpp
)

target_include_directories(openxr-wrapper PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(openxr-wrapper PRIVATE XR_USE_PLATFORM_ANDROID
        XR_USE_GRAPHICS_API_VULKAN
        VK_USE_PLATFORM_ANDROID_KHR
)

target_link_libraries(openxr-wrapper PUBLIC
        openxr_loader
        spdlog
        magic_enum

)