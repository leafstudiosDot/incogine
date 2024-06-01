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
    find_package(D3D11 REQUIRED)
    find_package(D3D12 REQUIRED)
    target_link_libraries(Incogine PRIVATE d3d11 d3dcompiler)
    target_link_libraries(Incogine PRIVATE d3d12 d3dcompiler)
endif()


# Read the project.xml file directly in CMakeLists.txt
file(READ "${CMAKE_SOURCE_DIR}/src/project.xml" PROJECT_XML_CONTENT)

# Extract values from project.xml
string(REGEX MATCH "<name>(.+)</name>" PROJECT_NAME_MATCH "${PROJECT_XML_CONTENT}")
set(PROJECT_NAME "${CMAKE_MATCH_1}")
string(REGEX MATCH "<version>(.+)</version>" PROJECT_VERSION_MATCH "${PROJECT_XML_CONTENT}")
set(PROJECT_VERSION "${CMAKE_MATCH_1}")
string(REGEX MATCH "<description>(.+)</description>" PROJECT_DESCRIPTION_MATCH "${PROJECT_XML_CONTENT}")
set(PROJECT_DESCRIPTION "${CMAKE_MATCH_1}")
string(REGEX MATCH "<incogine_version>(.+)</incogine_version>" INCOGINE_VERSION_MATCH "${PROJECT_XML_CONTENT}")
set(INCOGINE_VERSION "${CMAKE_MATCH_1}")
string(REGEX MATCH "<author>(.+)</author>" PROJECT_AUTHOR_MATCH "${PROJECT_XML_CONTENT}")
set(PROJECT_AUTHOR "${CMAKE_MATCH_1}")

# Add double quotes to each value
set(PROJECT_NAME "\"${PROJECT_NAME}\"")
set(PROJECT_VERSION "\"${PROJECT_VERSION}\"")
set(PROJECT_AUTHOR "\"${PROJECT_AUTHOR}\"")
set(INCOGINE_VERSION "\"${INCOGINE_VERSION}\"")
set(PROJECT_DESCRIPTION "\"${PROJECT_DESCRIPTION}\"")

# Add the definitions to the compile options
add_definitions("-DPROJECT_NAME=${PROJECT_NAME}")
add_definitions("-DPROJECT_VERSION=${PROJECT_VERSION}")
add_definitions("-DPROJECT_AUTHOR=${PROJECT_AUTHOR}")
add_definitions("-DINCOGINE_VERSION=${INCOGINE_VERSION}")
add_definitions("-DPROJECT_DESCRIPTION=${PROJECT_DESCRIPTION}")


include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_executable(Incogine ${SOURCES})
target_link_libraries(Incogine PRIVATE SDL2::SDL2)
target_link_libraries(Incogine PRIVATE ${OPENGL_LIBRARIES})

if (APPLE)
    target_link_libraries(Incogine PRIVATE ${METAL_LIBRARY})
endif()