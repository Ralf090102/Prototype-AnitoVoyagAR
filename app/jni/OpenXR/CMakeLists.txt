cmake_minimum_required(VERSION 3.22.1)

add_library(OpenXR SHARED

)

target_include_directories(OpenXR PRIVATE

)

target_link_libraries(OpenXR PUBLIC
    openxr_loader
)