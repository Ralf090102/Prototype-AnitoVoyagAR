cmake_minimum_required(VERSION 3.22.1)

include(FetchContent)
include(CheckIPOSupported)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/android.cmake)

project("Anito-VoyagAR")

# Version Management
set(ANITO_VERSION_MAJOR 1)
set(ANITO_VERSION_MINOR 0)
set(ANITO_VERSION_PATCH 0)
set(ANITO_VERSION "${ANITO_VERSION_MAJOR}.${ANITO_VERSION_MINOR}.${ANITO_VERSION_PATCH}")

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/generated/version.h
)

option(ANITO_BUILD_TESTS "Build tests" OFF)
option(ANITO_USE_BUNDLED_DEPS "Use bundled dependencies" ON)

set(CMAKE_DISABLE_IN_SOURCE_BUILDS ON)

# C settings
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# C++ settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -ggdb")

set(CMAKE_BUILD_TYPE_INIT "Debug")

# Compiler Settings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic)

    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")

    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/W4)

    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /Od /Zi")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /Zi")

    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /O2")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
endif()

# Enable Interprocedural Optimization on Release
check_ipo_supported(RESULT IPO_SUPPORTED)
if(IPO_SUPPORTED AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()


# Dependency Management
# EXCLUDE_FROM_ALL => Prevents library from being built unless explicitly needed.
# SYSTEM => Suppresses compiler warnings from library headers.
FetchContent_Declare(magic_enum
        GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
        GIT_TAG v0.9.7
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(magic_enum)
FetchContent_GetProperties(magic_enum)
if(magic_enum_ADDED)
    set_target_properties(magic_enum PROPERTIES
            EXCLUDE_FROM_ALL TRUE
            SYSTEM TRUE
    )
endif()

FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.15.1
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(spdlog)
FetchContent_GetProperties(spdlog)
if(spdlog_ADDED)
    set_target_properties(spdlog PROPERTIES
            EXCLUDE_FROM_ALL TRUE
            SYSTEM TRUE
    )
endif()

FetchContent_Declare(glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(glm)
FetchContent_GetProperties(glm)
if(glm_ADDED)
    set_target_properties(glm PROPERTIES
            EXCLUDE_FROM_ALL TRUE
            SYSTEM TRUE
    )
endif()

FetchContent_Declare(SPIRV_Reflect
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect.git
        GIT_TAG vulkan-sdk-1.4.309.0
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
set(SPIRV_REFLECT_EXAMPLES OFF)
set(SPIRV_REFLECT_EXECUTABLE OFF)
set(SPIRV_REFLECT_STATIC_LIB ON)
FetchContent_MakeAvailable(SPIRV_Reflect)
FetchContent_GetProperties(SPIRV_Reflect)
if(SPIRV_Reflect_ADDED)
    set_target_properties(spirv-reflect-static PROPERTIES
            EXCLUDE_FROM_ALL TRUE
            SYSTEM TRUE
    )
endif()

FetchContent_Declare(OpenXR_SDK
        GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
        GIT_TAG release-1.1.46
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(OpenXR_SDK)
FetchContent_GetProperties(OpenXR_SDK)
if(OpenXR_SDK_ADDED)
    set_target_properties(openxr_loader PROPERTIES
            EXCLUDE_FROM_ALL TRUE
            SYSTEM TRUE
    )
endif()

FetchContent_Declare(vk_bootstrap
        GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
        GIT_TAG v1.4.311
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(vk_bootstrap)
FetchContent_GetProperties(vk_bootstrap)
if(vk_bootstrap_ADDED)
    set_target_properties(vk-bootstrap PROPERTIES
            EXCLUDE_FROM_ALL TRUE
            SYSTEM TRUE
    )
endif()

# Interface Libraries
add_library(magic_enum_headers INTERFACE)
target_include_directories(magic_enum_headers INTERFACE
        ${magic_enum_SOURCE_DIR}/include
)

add_library(spdlog_headers INTERFACE)
target_include_directories(spdlog_headers INTERFACE
        ${spdlog_SOURCE_DIR}/include
)

add_library(glm_headers INTERFACE)
target_include_directories(glm_headers INTERFACE
        ${glm_SOURCE_DIR}
)

add_library(SPIRV_Reflect_headers INTERFACE)
target_include_directories(SPIRV_Reflect_headers INTERFACE
        ${SPIRV_Reflect_SOURCE_DIR}/include
)

add_library(OpenXR_SDK_headers INTERFACE)
target_include_directories(OpenXR_SDK_headers INTERFACE
        ${OpenXR_SDK_SOURCE_DIR}/include
)

add_library(vk_bootstrap_headers INTERFACE)
target_include_directories(vk_bootstrap_headers INTERFACE
        ${vk_bootstrap_SOURCE_DIR}/src
)

find_package(Vulkan REQUIRED)

add_subdirectory(jni)