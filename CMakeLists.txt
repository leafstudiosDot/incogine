cmake_minimum_required(VERSION 3.26.3)
project(Incogine)

# SDL2
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/reqs/lib/SDL2.framework")
find_package(SDL2 REQUIRED COMPONENTS SDL2)
include_directories(${SDL2_INCLUDE_DIRS})

# SDL2_ttf
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/reqs/lib/SDL2_ttf.framework")
find_package(SDL2_ttf REQUIRED COMPONENTS SDL2_ttf)
include_directories(${SDL2_TTF_INCLUDE_DIRS})

# OpenGL
find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})

# FBX
set(FBX_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/reqs/include/fbx")
set(FBX_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/reqs/lib/fbx/debug/libfbxsdk.a")

include_directories(${FBX_INCLUDE_DIR})
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/reqs/lib/fbx/debug)

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

set(MAIN_FONT_FILE "${CMAKE_SOURCE_DIR}/src/fonts/main_font.ttf")
set(MAIN_FONTPARSER_SCRIPT "${CMAKE_SOURCE_DIR}/src/parser/ttfparse_main.py")

add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/src/fonts/main_font.h
    COMMAND python3 ${MAIN_FONTPARSER_SCRIPT} ${MAIN_FONT_FILE} ${CMAKE_SOURCE_DIR}/src/fonts/main_font.h
    DEPENDS ${MAIN_FONT_FILE} ${MAIN_FONTPARSER_SCRIPT}
    COMMENT "Generating main_font.h from main_font.ttf"
)


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
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/fonts)
file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_executable(Incogine ${SOURCES} ${CMAKE_SOURCE_DIR}/src/fonts/main_font.h)
target_link_libraries(Incogine PRIVATE SDL2::SDL2 SDL2_ttf::SDL2_ttf)
target_link_libraries(Incogine PRIVATE ${OPENGL_LIBRARIES})
target_link_libraries(Incogine PRIVATE ${FBX_LIBRARY})

if (APPLE)
    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    target_link_libraries(Incogine PRIVATE ${COREFOUNDATION_LIBRARY})
    target_link_libraries(Incogine PRIVATE ${METAL_LIBRARY})
endif()