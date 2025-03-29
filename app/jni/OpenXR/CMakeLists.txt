cmake_minimum_required(VERSION 3.22.1)

file(GLOB ROOT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(openxr_wrapper SHARED
        ${ROOT_SOURCES}
)

target_include_directories(openxr_wrapper
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/export
        ${CMAKE_CURRENT_BINARY_DIR}/generated
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/OpenXR
)

target_compile_definitions(openxr_wrapper PRIVATE
        XR_USE_PLATFORM_ANDROID
        XR_USE_GRAPHICS_API_VULKAN
        VK_USE_PLATFORM_ANDROID_KHR
)

target_link_libraries(openxr_wrapper PUBLIC
        openxr_loader
        vulkan_wrapper
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