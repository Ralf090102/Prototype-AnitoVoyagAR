cmake_minimum_required(VERSION 3.22.1)

add_library(native_app_glue STATIC ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC ${ANDROID_NDK}/sources/android/native_app_glue)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

add_subdirectory(Vulkan)
add_subdirectory(OpenXR)

add_subdirectory(Anito-VoyagAR)

include(${CMAKE_SOURCE_DIR}/CMake/properties.cmake)
set_anito_properties()