cmake_minimum_required(VERSION 3.22.1)

find_library(log log)
find_library(android android)

add_library(${CMAKE_PROJECT_NAME} SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)


target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
        OpenXR
        native_app_glue
        spdlog
        ${log}
        ${android}
)