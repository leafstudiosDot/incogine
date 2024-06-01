cmake_minimum_required(VERSION 3.26.3)
project(Incogine)

# SDL2
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/reqs/lib/SDL2.framework")
find_package(SDL2 REQUIRED COMPONENTS SDL2)
include_directories(${SDL2_INCLUDE_DIRS})

# OpenGL
find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})

# Vulkan
#list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/reqs/lib/SDL2.framework")
#find_package(Vulkan REQUIRED)
#include_directories(${Vulkan_INCLUDE_DIRS})

# Metal (macOS)
if(APPLE)
    find_library(METAL_LIBRARY Metal)
endif()

# DirectX (Windows)
if(WIN32)
    target_link_libraries(Incogine PRIVATE d3d11 d3dcompiler)
    target_link_libraries(Incogine PRIVATE d3d12 d3dcompiler)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_executable(Incogine ${SOURCES})
target_link_libraries(Incogine PRIVATE SDL2::SDL2)