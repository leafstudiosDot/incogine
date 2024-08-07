cmake_minimum_required(VERSION 3.26.3)

if(APPLE)
    set(CMAKE_C_COMPILER "/usr/bin/gcc")
    set(CMAKE_CXX_COMPILER "/usr/bin/g++")
endif()

project(Incogine)

if(EMSCRIPTEN)
    set(PLATFORM "Web")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")
elseif(WIN32)
    set(PLATFORM "Windows")
elseif(APPLE)
    set(PLATFORM "macOS")
    #set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
elseif(MIRA)
    set(PLATFORM "mira")
else()
    set(PLATFORM "Unknown")
endif()
message(STATUS "Building for platform: ${PLATFORM}")

if(PLATFORM STREQUAL "Web")
    set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/emsdk/upstream/emscripten/system")

    find_package(SDL2 REQUIRED)
    find_package(SDL2_ttf REQUIRED)
    find_package(SDL2_image REQUIRED)

    include_directories(${SDL2_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS})

    set_target_properties(${PROJECT_NAME} PROPERTIES
        SUFFIX ".html"
        LINK_FLAGS "-s USE_SDL=2 --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/src/web/init.html"
    )
endif()

if (WIN32)
    # SDL2
    set(SDL2_PATH "${CMAKE_CURRENT_SOURCE_DIR}/reqs/SDL2")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(SDL2_LIB_DIR ${SDL2_PATH}/lib/x64)
        set(SDL2_BIN_DIR ${SDL2_PATH}/bin/x64)
    else()
        set(SDL2_LIB_DIR ${SDL2_PATH}/lib/x86)
        set(SDL2_BIN_DIR ${SDL2_PATH}/bin/x86)
    endif()

    include_directories(${SDL2_PATH}/include)
    link_directories(${SDL2_LIB_DIR})

    # SDL2_ttf
    set(SDL2_TTF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/reqs/SDL2_ttf")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(SDL2_TTF_LIB_DIR ${SDL2_TTF_PATH}/lib/x64)
        set(SDL2_TTF_BIN_DIR ${SDL2_TTF_PATH}/bin/x64)
    else()
        set(SDL2_TTF_LIB_DIR ${SDL2_TTF_PATH}/lib/x86)
        set(SDL2_TTF_BIN_DIR ${SDL2_TTF_PATH}/bin/x86)
    endif()

    include_directories(${SDL2_TTF_PATH}/include)
    link_directories(${SDL2_TTF_LIB_DIR})

    # SDL2_image
    set(SDL2_IMAGE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/reqs/SDL2_image")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(SDL2_IMAGE_LIB_DIR ${SDL2_IMAGE_PATH}/lib/x64)
        set(SDL2_IMAGE_BIN_DIR ${SDL2_IMAGE_PATH}/bin/x64)
    else()
        set(SDL2_IMAGE_LIB_DIR ${SDL2_IMAGE_PATH}/lib/x86)
        set(SDL2_IMAGE_BIN_DIR ${SDL2_IMAGE_PATH}/bin/x86)
    endif()
        
    include_directories(${SDL2_IMAGE_PATH}/include)
    link_directories(${SDL2_IMAGE_LIB_DIR})
endif()

if (APPLE)
    # SDL2
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/reqs/lib/SDL2.framework")
    find_package(SDL2 REQUIRED COMPONENTS SDL2)
    include_directories(${SDL2_INCLUDE_DIRS})

    # SDL2_ttf
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/reqs/lib/SDL2_ttf.framework")
    find_package(SDL2_ttf REQUIRED COMPONENTS SDL2_ttf)
    include_directories(${SDL2_TTF_INCLUDE_DIRS})

    # SDL2_image
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/reqs/lib/SDL2_image.framework")
    find_package(SDL2_image REQUIRED COMPONENTS SDL2_image)
    include_directories(${SDL2_IMAGE_INCLUDE_DIRS})
endif()

# OpenGL
find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})

# FBX
if (APPLE)
    set(FBX_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/reqs/include/fbx")
    set(FBX_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/reqs/lib/fbx/debug/libfbxsdk.a")
    include_directories(${FBX_INCLUDE_DIR})
    link_directories(${CMAKE_CURRENT_SOURCE_DIR}/reqs/lib/fbx/debug)
endif()

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
    set(WindowsSDKDir "C:/Program Files (x86)/Windows Kits/10")
    set(CMAKE_PREFIX_PATH ${WindowsSDKDir})
    set(D3D12_DIR ${WindowsSDKDir})
    set(D3D11_DIR ${WindowsSDKDir})
endif()


# Fonts
set(MAIN_FONT_FILE "${CMAKE_SOURCE_DIR}/src/fonts/main_font.ttf")
set(MAIN_FONTPARSER_SCRIPT "${CMAKE_SOURCE_DIR}/src/parser/ttfparse_main.py")

add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/src/fonts/main_font.h
    COMMAND python3 ${MAIN_FONTPARSER_SCRIPT} ${MAIN_FONT_FILE} ${CMAKE_SOURCE_DIR}/src/fonts/main_font.h
    DEPENDS ${MAIN_FONT_FILE} ${MAIN_FONTPARSER_SCRIPT}
    COMMENT "Generating main_font.h from main_font.ttf"
)


# SVG
set(SVG_PARSER_SCRIPT "${CMAKE_SOURCE_DIR}/src/parser/svgparse.py")
file(GLOB SVG_FILES "${CMAKE_SOURCE_DIR}/*.svg")

foreach(SVG_FILE ${SVG_FILES})
    get_filename_component(SVG_FILENAME ${SVG_FILE} NAME_WE)
    add_custom_command(
        OUTPUT "${SVG_FILENAME}_svgdata.h"
        COMMAND python3 ${SVG_PARSER_SCRIPT} ${CMAKE_SOURCE_DIR}/src/parser/svgparse.py ${SVG_FILE}
        DEPENDS ${SVG_FILE}
        COMMENT "Generating header for ${SVG_FILE}"
    )
    list(APPEND SVG_HEADERS "${CMAKE_BINARY_DIR}/${SVG_FILENAME}_svgdata.h")
endforeach()
add_custom_target(generate_headers ALL DEPENDS ${SVG_HEADERS})


# Project Settings (project.xml)
file(READ "${CMAKE_SOURCE_DIR}/src/project.xml" PROJECT_XML_CONTENT)

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

set(PROJECT_NAME_RAW ${PROJECT_NAME})
set(PROJECT_NAME "\"${PROJECT_NAME}\"")
set(PROJECT_VERSION "\"${PROJECT_VERSION}\"")
set(PROJECT_AUTHOR "\"${PROJECT_AUTHOR}\"")
set(INCOGINE_VERSION "\"${INCOGINE_VERSION}\"")
set(PROJECT_DESCRIPTION "\"${PROJECT_DESCRIPTION}\"")

add_definitions("-DPROJECT_NAME=${PROJECT_NAME}")
add_definitions("-DPROJECT_VERSION=${PROJECT_VERSION}")
add_definitions("-DPROJECT_AUTHOR=${PROJECT_AUTHOR}")
add_definitions("-DINCOGINE_VERSION=${INCOGINE_VERSION}")
add_definitions("-DPROJECT_DESCRIPTION=${PROJECT_DESCRIPTION}")


include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/fonts)

file(GLOB_RECURSE CPP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE H_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)
set(SOURCES ${CPP_SOURCES} ${H_SOURCES})

foreach(_source IN ITEMS ${SOURCES})
    get_filename_component(_source_path "${_source}" PATH)
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" _group_path "${_source_path}")
    string(REPLACE "/" "\\" _group_path "${_group_path}")
    source_group("${_group_path}" FILES "${_source}")
endforeach()

add_executable(${PROJECT_NAME_RAW} ${SOURCES} ${CMAKE_SOURCE_DIR}/src/fonts/main_font.h)

if (APPLE)
    target_link_libraries(${PROJECT_NAME_RAW} PRIVATE SDL2::SDL2 SDL2_ttf::SDL2_ttf SDL2_image::SDL2_image)
    target_link_libraries(${PROJECT_NAME_RAW} PRIVATE ${OPENGL_LIBRARIES})
    target_link_libraries(${PROJECT_NAME_RAW} PRIVATE ${FBX_LIBRARY})

    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    target_link_libraries(${PROJECT_NAME_RAW} PRIVATE ${COREFOUNDATION_LIBRARY})
    target_link_libraries(${PROJECT_NAME_RAW} PRIVATE ${METAL_LIBRARY})
endif()

if(WIN32)
    target_include_directories(${PROJECT_NAME_RAW}
        PRIVATE
        "${SDL2_PATH}/include"
        "${SDL2_IMAGE_PATH}/include"
        "${SDL2_TTF_PATH}/include"
    )

    target_link_libraries(${PROJECT_NAME_RAW} PRIVATE SDL2 SDL2main SDL2_ttf SDL2_image)
    target_link_libraries(${PROJECT_NAME_RAW} PRIVATE ${OPENGL_LIBRARIES})
    target_link_libraries(${PROJECT_NAME_RAW} PRIVATE ${FBX_LIBRARY})

    target_link_libraries(${PROJECT_NAME_RAW} PRIVATE d3d11 d3dcompiler)
    target_link_libraries(${PROJECT_NAME_RAW} PRIVATE d3d12 d3dcompiler)

    add_custom_command(TARGET ${PROJECT_NAME_RAW} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SDL2_LIB_DIR}/SDL2.dll $<TARGET_FILE_DIR:Incogine>)
    add_custom_command(TARGET ${PROJECT_NAME_RAW} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2_IMAGE_LIB_DIR}/SDL2_image.dll" $<TARGET_FILE_DIR:${PROJECT_NAME_RAW}>)
    add_custom_command(TARGET ${PROJECT_NAME_RAW} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2_TTF_LIB_DIR}/SDL2_ttf.dll" $<TARGET_FILE_DIR:${PROJECT_NAME_RAW}>)
endif()


# macOS Bundle properties
if(APPLE)
    set_target_properties(${PROJECT_NAME_RAW} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME_RAW}
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${INCOGINE_VERSION}
        MACOSX_BUNDLE_LONG_VERSION_STRING ${INCOGINE_VERSION}
        MACOSX_BUNDLE_COPYRIGHT "© 2024 leafstudiosDot"
    )
    set(MACOSX_BUNDLE_ICON_FILE "AppIcon.icns")
    set_source_files_properties(${CMAKE_SOURCE_DIR}/bundle/macos/AppIcon.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(${PROJECT_NAME_RAW} PRIVATE ${CMAKE_SOURCE_DIR}/bundle/macos/AppIcon.icns)
endif()

# Windows Bundle properties
if(WIN32)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_custom_target(run_debug
            COMMAND ${CMAKE_COMMAND} ${CMAKE_BINARY_DIR}/${PROJECT_NAME_RAW}.exe -dev
            COMMENT "Running application with -dev argument"
        )
    endif()
endif()