cmake_minimum_required(VERSION 3.26.3)

if(APPLE)
    set(CMAKE_C_COMPILER "/usr/bin/gcc")
    set(CMAKE_CXX_COMPILER "/usr/bin/g++")
endif()

project(Incogine)

# Python3
find_package(Python3 REQUIRED)

if(EMSCRIPTEN)
    set(PLATFORM "Web")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")
elseif(WIN32)
    set(PLATFORM "Windows")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "WindowsStore")
    set(PLATFORM "UWP")
elseif(APPLE)
    if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(PLATFORM "iOS")
    else()
        set(PLATFORM "macOS")
    endif()
elseif(ANDROID)
    set(PLATFORM "Android")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/reqs/android_ndk/build/cmake/android.toolchain.cmake")
    set(ANDROID_ABI "arm64-v8a")
    set(ANDROID_PLATFORM_LEVEL "android-26")
    set(CMAKE_ANDROID_NDK_TOOLCHAIN_VERSION clang)
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
    if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
        # SDL2
        set(SDL2_DIR ${CMAKE_CURRENT_SOURCE_DIR}/reqs/lib/SDL2.framework/Resources/CMake)
        find_package(SDL2 REQUIRED)
        include_directories(${SDL2_INCLUDE_DIRS})

        # SDL2_ttf
        set(SDL2_ttf_DIR ${CMAKE_CURRENT_SOURCE_DIR}/reqs/lib/SDL2_ttf.framework/Resources/CMake)
        find_package(SDL2_ttf REQUIRED)
        include_directories(${SDL2_TTF_INCLUDE_DIRS})

        # SDL2_image
        set(SDL2_image_DIR ${CMAKE_CURRENT_SOURCE_DIR}/reqs/lib/SDL2_image.framework/Resources/CMake)
        find_package(SDL2_image REQUIRED)
        include_directories(${SDL2_IMAGE_INCLUDE_DIRS})
    else()
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
endif()

# OpenGL
if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # Do nothing
else()
    find_package(OpenGL REQUIRED)
    include_directories(${OPENGL_INCLUDE_DIR})
    link_directories(${OPENGL_gl_LIBRARY})
endif()

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

# Metal (iOS/macOS)
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

# UWP
if(${CMAKE_SYSTEM_NAME} STREQUAL "WindowsStore")
    add_definitions(-DWINAPI_FAMILY=WINAPI_FAMILY_APP)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /ZW")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /APPCONTAINER")
    set(CMAKE_TARGET_PLATFORM "UWP")
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

# Android
if(ANDROID)
    find_library(ANDROID_LOG_LIB log)
    find_library(ANDROID_NATIVE_APP_GLUE_LIB android_native_app_glue)
    find_library(OPENGL_LIB GLESv3)
    find_library(EGL_LIB EGL)

    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${ANDROID_LOG_LIB}
        ${ANDROID_NATIVE_APP_GLUE_LIB}
        ${OPENGL_LIB}
        ${EGL_LIB}
    )

    add_definitions("-DANDROID")
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
string(REGEX MATCH "<window_name>(.+)</window_name>" WINDOW_NAME_MATCH "${PROJECT_XML_CONTENT}")
set(WINDOW_NAME "${CMAKE_MATCH_1}")
string(REGEX MATCH "<identifier>(.+)</identifier>" IDENTIFIER_MATCH "${PROJECT_XML_CONTENT}")
set(BUNDLE_IDENTIFIER "${CMAKE_MATCH_1}")
string(REGEX MATCH "<version>(.+)</version>" PROJECT_VERSION_MATCH "${PROJECT_XML_CONTENT}")
set(PROJECT_VERSION "${CMAKE_MATCH_1}")
string(REGEX MATCH "<description>(.+)</description>" PROJECT_DESCRIPTION_MATCH "${PROJECT_XML_CONTENT}")
set(PROJECT_DESCRIPTION "${CMAKE_MATCH_1}")
string(REGEX MATCH "<incogine_version>(.+)</incogine_version>" INCOGINE_VERSION_MATCH "${PROJECT_XML_CONTENT}")
set(INCOGINE_VERSION "${CMAKE_MATCH_1}")
string(REGEX MATCH "<author>(.+)</author>" PROJECT_AUTHOR_MATCH "${PROJECT_XML_CONTENT}")
set(PROJECT_AUTHOR "${CMAKE_MATCH_1}")
string(REGEX MATCH "<copyright>(.+)</copyright>" COPYRIGHT_MATCH "${PROJECT_XML_CONTENT}")
set(PROJECT_COPYRIGHT "${CMAKE_MATCH_1}")

set(PROJECT_NAME ${PROJECT_NAME})
set(WINDOW_NAME ${WINDOW_NAME})
set(BUNDLE_IDENTIFIER ${BUNDLE_IDENTIFIER})
set(PROJECT_VERSION ${PROJECT_VERSION})
set(PROJECT_AUTHOR ${PROJECT_AUTHOR})
set(INCOGINE_VERSION ${INCOGINE_VERSION})
set(PROJECT_DESCRIPTION ${PROJECT_DESCRIPTION})
set(PROJECT_COPYRIGHT ${PROJECT_COPYRIGHT})

add_definitions("-DPROJECT_NAME=\"${PROJECT_NAME}\"")
add_definitions("-DWINDOW_NAME=\"${WINDOW_NAME}\"")
add_definitions("-DBUNDLE_IDENTIFIER=\"${BUNDLE_IDENTIFIER}\"")
add_definitions("-DPROJECT_VERSION=\"${PROJECT_VERSION}\"")
add_definitions("-DPROJECT_AUTHOR=\"${PROJECT_AUTHOR}\"")
add_definitions("-DINCOGINE_VERSION=\"${INCOGINE_VERSION}\"")
add_definitions("-DPROJECT_DESCRIPTION=\"${PROJECT_DESCRIPTION}\"")
add_definitions("-DPROJECT_COPYRIGHT=\"${PROJECT_COPYRIGHT}\"")


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

add_executable(${PROJECT_NAME} ${SOURCES} ${CMAKE_SOURCE_DIR}/src/fonts/main_font.h)

if (APPLE)
    if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
        # Find OpenGL ES for iOS
        find_library(OpenGLES_LIBRARIES OpenGLES)
        include_directories(${OPENGL_INCLUDE_DIR})

        target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2 SDL2_ttf::SDL2_ttf SDL2_image::SDL2_image ${FBX_LIBRARY} ${OpenGLES_LIBRARIES})
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2 SDL2_ttf::SDL2_ttf SDL2_image::SDL2_image)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENGL_LIBRARIES})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${FBX_LIBRARY})
    endif()

    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${COREFOUNDATION_LIBRARY})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${METAL_LIBRARY})
endif()

if(WIN32)
    target_include_directories(${PROJECT_NAME}
        PRIVATE
        "${SDL2_PATH}/include"
        "${SDL2_IMAGE_PATH}/include"
        "${SDL2_TTF_PATH}/include"
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE SDL2 SDL2main SDL2_ttf SDL2_image)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENGL_LIBRARIES})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${FBX_LIBRARY})

    target_link_libraries(${PROJECT_NAME} PRIVATE d3d11 d3dcompiler)
    target_link_libraries(${PROJECT_NAME} PRIVATE d3d12 d3dcompiler)

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SDL2_LIB_DIR}/SDL2.dll $<TARGET_FILE_DIR:Incogine>)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2_IMAGE_LIB_DIR}/SDL2_image.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2_TTF_LIB_DIR}/SDL2_ttf.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
endif()


# Apple macOS/iOS Bundle properties
if(APPLE)
    if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0")

        set(APP_ASSETS 
            ${CMAKE_SOURCE_DIR}/bundle/ios/Assets.xcassets
            ${CMAKE_SOURCE_DIR}/bundle/ios/LaunchScreen.storyboard
        )

        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${APP_ASSETS} $<TARGET_FILE_DIR:${PROJECT_NAME}>/Assets.xcassets
        )

        add_custom_command(
            OUTPUT ${CMAKE_BINARY_DIR}/Info.plist
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/src/parser/ios_infoplist_gen.py
                "${WINDOW_NAME}" "${PROJECT_NAME}" "${BUNDLE_IDENTIFIER}" "${PROJECT_VERSION}" "${PROJECT_NAME}"
            DEPENDS ${CMAKE_SOURCE_DIR}/src/parser/ios_infoplist_gen.py
            COMMENT "Generating Info.plist"
        )
        add_custom_target(InfoPlist ALL DEPENDS ${CMAKE_BINARY_DIR}/Info.plist)
        add_dependencies(${PROJECT_NAME} InfoPlist)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_BINARY_DIR}/Info.plist"
        )
    else()
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")
        set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME ${WINDOW_NAME}
            MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
            MACOSX_BUNDLE_SHORT_VERSION_STRING ${INCOGINE_VERSION}
            MACOSX_BUNDLE_LONG_VERSION_STRING ${INCOGINE_VERSION}
            MACOSX_BUNDLE_COPYRIGHT "${PROJECT_COPYRIGHT}\nIncogine © 2024 leafstudiosDot"
        )
        set(MACOSX_BUNDLE_ICON_FILE "AppIcon.icns")
        set_source_files_properties(${CMAKE_SOURCE_DIR}/bundle/macos/AppIcon.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/bundle/macos/AppIcon.icns)
    endif()
endif()

# Windows Bundle properties
if(WIN32)
    find_program(CMAKE_RC_COMPILER NAMES rc.exe)

    if(NOT CMAKE_RC_COMPILER)
        message(FATAL_ERROR "Resource compiler (rc.exe) not found. Please ensure Visual Studio is installed and configured correctly.")
    endif()

    add_custom_command(
        TARGET ${PROJECT_NAME}
        PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/bundle/windows/icon.ico ${CMAKE_BINARY_DIR}
        COMMENT "Copying icon file"
    )

    add_custom_command(
        TARGET ${PROJECT_NAME}
        PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/bundle/windows/resource.rc ${CMAKE_BINARY_DIR}
        COMMENT "Copying resource script file"
    )

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/resource.res
        COMMAND ${CMAKE_RC_COMPILER} /fo ${CMAKE_BINARY_DIR}/resource.res ${CMAKE_BINARY_DIR}/resource.rc
        DEPENDS ${CMAKE_BINARY_DIR}/resource.rc
        COMMENT "Compiling resource file"
    )

    add_custom_target(
        compile_resource ALL
        DEPENDS ${CMAKE_BINARY_DIR}/resource.res
    )

    add_dependencies(${PROJECT_NAME} compile_resource)
    target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR}/resource.res)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_custom_target(run_debug
            COMMAND ${CMAKE_COMMAND} ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.exe -dev
            COMMENT "Running application with -dev argument"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    endif()

    add_custom_target(run ALL
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} --target run_debug
        COMMENT "Build and run the application"
    )
endif()