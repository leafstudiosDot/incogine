# Incogine — top-level CMake configuration
cmake_minimum_required(VERSION 3.26.3)
project(Incogine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Project settings (extracted from src/project.xml)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/project.xml" PROJECT_XML_CONTENT)

# Extracts <tag>value</tag> matches from project.xml into ${_var}
macro(icg_xml_set _tag _var)
    string(REGEX MATCH "<${_tag}>(.+)</${_tag}>" _xml_match "${PROJECT_XML_CONTENT}")
    set(${_var} "${CMAKE_MATCH_1}")
endmacro()

icg_xml_set(name                PROJECT_NAME)
icg_xml_set(window_name         WINDOW_NAME)
icg_xml_set(identifier          BUNDLE_IDENTIFIER)
icg_xml_set(version             PROJECT_VERSION)
icg_xml_set(description         PROJECT_DESCRIPTION)
icg_xml_set(incogine_version    INCOGINE_VERSION)
icg_xml_set(author              PROJECT_AUTHOR)
icg_xml_set(copyright           PROJECT_COPYRIGHT)

set(PROJECT_DEFINES
    -DPROJECT_NAME=\"${PROJECT_NAME}\"
    -DWINDOW_NAME=\"${WINDOW_NAME}\"
    -DBUNDLE_IDENTIFIER=\"${BUNDLE_IDENTIFIER}\"
    -DPROJECT_VERSION=\"${PROJECT_VERSION}\"
    -DPROJECT_AUTHOR=\"${PROJECT_AUTHOR}\"
    -DINCOGINE_VERSION=\"${INCOGINE_VERSION}\"
    -DPROJECT_DESCRIPTION=\"${PROJECT_DESCRIPTION}\"
    -DPROJECT_COPYRIGHT=\"${PROJECT_COPYRIGHT}\"
)

# Helpers
find_package(Python3 REQUIRED)

# Groups source files into folders in the IDE (no-op on non-Visual Studio generators)
function(icg_group_sources _sources)
    foreach(_source IN LISTS ${_sources})
        get_filename_component(_source_path "${_source}" PATH)
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" _group_path "${_source_path}")
        string(REPLACE "/" "\\" _group_path "${_group_path}")
        source_group("${_group_path}" FILES "${_source}")
    endforeach()
endfunction()

# Platform detection
if(EMSCRIPTEN)
    set(PLATFORM "Web")
elseif(WIN32)
    set(PLATFORM "Windows")
elseif(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
    set(PLATFORM "UWP")
elseif(APPLE)
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(PLATFORM "iOS")
    else()
        set(PLATFORM "macOS")
    endif()
elseif(ANDROID)
    set(PLATFORM "Android")
    set(ANDROID_ABI "arm64-v8a")
    set(ANDROID_PLATFORM_LEVEL "android-26")
    set(CMAKE_ANDROID_NDK_TOOLCHAIN_VERSION clang)
elseif(MIRA)
    set(PLATFORM "mira")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(EXISTS "/etc/lsb-release")
        file(READ "/etc/lsb-release" LSB_RELEASE_CONTENT)
        if(LSB_RELEASE_CONTENT MATCHES "DISTRIB_ID=Ubuntu")
            set(PLATFORM "Ubuntu")
        elseif(LSB_RELEASE_CONTENT MATCHES "DISTRIB_ID=firoma Mira")
            set(PLATFORM "mira_linux")
        else()
            set(PLATFORM "Linux - Unknown Distribution")
        endif()
    else()
        set(PLATFORM "Linux - Unknown")
    endif()
else()
    set(PLATFORM "Unknown")
endif()

message(STATUS "Building for platform: ${PLATFORM}")

# Platform dependencies
if(PLATFORM STREQUAL "Windows")
    if(MSVC)
        add_compile_options(/utf-8)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
            CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-fexec-charset=UTF-8)
    endif()

    # Prebuilt SDL3 dependencies in reqs/ (x64/x86 layout)
    function(icg_setup_sdl3 _name _include_subdir)
        set(_path "${CMAKE_CURRENT_SOURCE_DIR}/reqs/${_name}")
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(_lib_dir "${_path}/lib/x64")
            set(_bin_dir "${_path}/bin/x64")
        else()
            set(_lib_dir "${_path}/lib/x86")
            set(_bin_dir "${_path}/bin/x86")
        endif()
        set(${_name}_PATH "${_path}" PARENT_SCOPE)
        set(${_name}_LIB_DIR "${_lib_dir}" PARENT_SCOPE)
        set(${_name}_BIN_DIR "${_bin_dir}" PARENT_SCOPE)
        include_directories("${_path}/${_include_subdir}")
        link_directories("${_lib_dir}")
    endfunction()

    icg_setup_sdl3(SDL3       include)
    icg_setup_sdl3(SDL3_ttf   include)
    icg_setup_sdl3(SDL3_image include)
    icg_setup_sdl3(SDL3_mixer include)

    # Windows SDK (needed to locate d3d11/d3d12 headers and libs)
    set(WindowsSDKDir "C:/Program Files (x86)/Windows Kits/10")
    set(CMAKE_PREFIX_PATH ${WindowsSDKDir})
elseif(PLATFORM STREQUAL "UWP")
    add_definitions(-DWINAPI_FAMILY=WINAPI_FAMILY_APP)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /ZW")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /APPCONTAINER")
    set(CMAKE_TARGET_PLATFORM "UWP")
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
else()
    # SDL3 + addons built from source submodules (Linux/macOS/iOS/Android/Web).
    # Windows uses the prebuilt libraries in reqs/ instead (see above).
    # Emscripten ships no SDL3 ports, so Web also builds from source.
    if(PLATFORM STREQUAL "Android")
        # Android requires SDL3 as a shared library (the Java glue in
        # SDLActivity loads libSDL3.so at runtime), so all SDL libraries
        # build shared there.
        set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
    else()
        # Static everywhere else: no runtime DLL/.so/.dylib distribution.
        # BUILD_SHARED_LIBS must be pinned before the SDL3 addons configure,
        # otherwise they default to shared and require SDL3-shared.
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
    endif()

    if(APPLE)
        # Link SDL3 as a plain static library, not as a framework.
        set(SDL_FRAMEWORK OFF CACHE BOOL "" FORCE)
    endif()

    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

    # Use the vendored third-party sources (git submodules of the SDL3
    # addon repos) so no system dev packages are required on any platform.
    set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
    set(SDLIMAGE_VENDORED ON CACHE BOOL "" FORCE)
    set(SDLMIXER_VENDORED ON CACHE BOOL "" FORCE)

    # AVIF needs the dav1d decoder which requires the nasm assembler to
    # build; not needed by the engine, so leave it disabled.
    set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)

    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/reqs/SDL3_source ${CMAKE_BINARY_DIR}/sdl3_build EXCLUDE_FROM_ALL)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/reqs/SDL3_ttf_source ${CMAKE_BINARY_DIR}/sdl3_ttf_build EXCLUDE_FROM_ALL)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/reqs/SDL3_image_source ${CMAKE_BINARY_DIR}/sdl3_image_build EXCLUDE_FROM_ALL)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/reqs/SDL3_mixer_source ${CMAKE_BINARY_DIR}/sdl3_mixer_build EXCLUDE_FROM_ALL)

    if(PLATFORM STREQUAL "iOS")
        find_library(OpenGLES_LIBRARIES OpenGLES)
        find_library(METAL_LIBRARY Metal)
        find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    elseif(PLATFORM STREQUAL "macOS")
        find_library(METAL_LIBRARY Metal)
        find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    elseif(PLATFORM STREQUAL "Android")
        find_library(ANDROID_LOG_LIB log)
        add_definitions("-DANDROID")
    endif()
endif()

# OpenGL: no header/library lookup needed - glad (below) provides the GL
# headers and resolves all GL entry points at runtime through
# SDL_GL_GetProcAddress. Desktop Linux only needs libGL.so to exist at
# runtime (provided by the Mesa/GL driver), not at link time.

# glad: generated GL loader (desktop GL 3.3 core + OpenGL ES 3.0)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src/core/render/glad/include")

# Generated resources
set(FONT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/fonts")
set(PARSER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/parser")

# Assets are read from disk at runtime by default (src/assets/ is copied
# next to the executable; Android/Web package it through their own
# mechanisms). ICG_EMBED_ASSETS=ON compiles fonts/audio into the binary as
# C arrays for single-file distribution.
option(ICG_EMBED_ASSETS "Embed fonts and audio into the executable (single-file distribution)" OFF)

if(ICG_EMBED_ASSETS)
    # Fonts: TTF -> C source/header pairs (generated into the build dir)
    set(FONT_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_fonts")
    set(FONT_C_FILES
        "${FONT_GEN_DIR}/main_font.c"
        "${FONT_GEN_DIR}/jpsup_font.c"
    )
    set(FONT_H_FILES
        "${FONT_GEN_DIR}/main_font.h"
        "${FONT_GEN_DIR}/jpsup_font.h"
    )

    add_custom_command(
        OUTPUT "${FONT_GEN_DIR}/main_font.h" "${FONT_GEN_DIR}/main_font.c"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${FONT_GEN_DIR}"
        COMMAND ${Python3_EXECUTABLE} "${PARSER_DIR}/ttfparse_main.py"
                "${FONT_DIR}/main_font.ttf" "${FONT_GEN_DIR}/main_font"
        DEPENDS "${FONT_DIR}/main_font.ttf" "${PARSER_DIR}/ttfparse_main.py"
        COMMENT "Generating main_font.h/.c from main_font.ttf"
    )

    add_custom_command(
        OUTPUT "${FONT_GEN_DIR}/jpsup_font.h" "${FONT_GEN_DIR}/jpsup_font.c"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${FONT_GEN_DIR}"
        COMMAND ${Python3_EXECUTABLE} "${PARSER_DIR}/ttfparse.py"
                "${FONT_DIR}/jpsup_font.ttf" "${FONT_GEN_DIR}/jpsup_font" _jpsup
        DEPENDS "${FONT_DIR}/jpsup_font.ttf" "${PARSER_DIR}/ttfparse.py"
        COMMENT "Generating jpsup_font.h/.c from jpsup_font.ttf"
    )

    add_custom_target(generate_fonts ALL DEPENDS ${FONT_H_FILES} ${FONT_C_FILES})
    include_directories("${FONT_GEN_DIR}")

    # Audio: OGG -> C source/header pairs (generated into the build dir)
    set(AUDIO_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_audio")
    set(AUDIO_C_FILES
        "${AUDIO_DIR}/testbgm_audio.c"
    )
    set(AUDIO_H_FILES
        "${AUDIO_DIR}/testbgm_audio.h"
    )

    add_custom_command(
        OUTPUT "${AUDIO_DIR}/testbgm_audio.h" "${AUDIO_DIR}/testbgm_audio.c"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${AUDIO_DIR}"
        COMMAND ${Python3_EXECUTABLE} "${PARSER_DIR}/audioparse.py"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/audio/testbgm.ogg" "${AUDIO_DIR}/testbgm_audio" testbgm
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/audio/testbgm.ogg" "${PARSER_DIR}/audioparse.py"
        COMMENT "Generating testbgm_audio.h/.c from testbgm.ogg"
    )

    add_custom_target(generate_audio ALL DEPENDS ${AUDIO_H_FILES} ${AUDIO_C_FILES})
    include_directories("${AUDIO_DIR}")

    add_compile_definitions(ICG_EMBED_ASSETS)
endif()

# SVG: root-level .svg files -> _svgdata.h headers
set(SVG_PARSER_SCRIPT "${PARSER_DIR}/svgparse.py")
file(GLOB SVG_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.svg")
foreach(SVG_FILE IN LISTS SVG_FILES)
    get_filename_component(SVG_FILENAME "${SVG_FILE}" NAME_WE)
    set(SVG_HEADER "${CMAKE_BINARY_DIR}/${SVG_FILENAME}_svgdata.h")
    add_custom_command(
        OUTPUT "${SVG_HEADER}"
        COMMAND ${Python3_EXECUTABLE} "${SVG_PARSER_SCRIPT}" "${SVG_FILE}" "${SVG_HEADER}"
        DEPENDS "${SVG_FILE}" "${SVG_PARSER_SCRIPT}"
        COMMENT "Generating ${SVG_HEADER}"
    )
endforeach()

# Puroko — static library for the project-specific code (project/ + scenes/)
set(PROJ_LIB Puroko)

file(GLOB_RECURSE PROJ_LIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/project/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/project/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scenes/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scenes/*.h
)

# splash/ and settings/ are compiled into the executable instead of the library
file(GLOB_RECURSE PROJ_LIB_EXCLUDE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scenes/splash/*
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scenes/settings/*
)
list(REMOVE_ITEM PROJ_LIB_SOURCES ${PROJ_LIB_EXCLUDE})

add_library(${PROJ_LIB} STATIC ${PROJ_LIB_SOURCES})
if(ICG_EMBED_ASSETS)
    add_dependencies(${PROJ_LIB} generate_fonts generate_audio)
endif()
target_include_directories(${PROJ_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fonts
)
target_compile_definitions(${PROJ_LIB} PUBLIC ${PROJECT_DEFINES})
icg_group_sources(PROJ_LIB_SOURCES)

# Pull in SDL3 + addon include dirs (submodule builds export them via the
# SDL3::* targets; Windows prebuilts via the global include dirs above).
if(PLATFORM STREQUAL "Linux" OR PLATFORM STREQUAL "Ubuntu" OR
   PLATFORM STREQUAL "mira_linux" OR PLATFORM STREQUAL "Linux - Unknown" OR
   PLATFORM STREQUAL "Linux - Unknown Distribution" OR
   PLATFORM STREQUAL "macOS" OR PLATFORM STREQUAL "iOS" OR
   PLATFORM STREQUAL "Android" OR PLATFORM STREQUAL "Web")
    target_link_libraries(${PROJ_LIB} PUBLIC
        SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image SDL3_mixer::SDL3_mixer
    )
endif()

# Executable sources
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fonts
)

file(GLOB_RECURSE CPP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE H_SOURCES   ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)
file(GLOB_RECURSE FILE_ASSETS ${CMAKE_CURRENT_SOURCE_DIR}/src/assets/*)

# Sources owned by the Puroko library stay out of the executable
list(REMOVE_ITEM CPP_SOURCES ${PROJ_LIB_SOURCES})
list(REMOVE_ITEM H_SOURCES   ${PROJ_LIB_SOURCES})

set(SOURCES ${CPP_SOURCES} ${H_SOURCES})
icg_group_sources(SOURCES)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/project.xml")
    source_group("" FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/project.xml")
    list(APPEND SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/project.xml")
endif()

set(CREDITS_XML "${CMAKE_CURRENT_SOURCE_DIR}/src/credits.xml")
if(EXISTS "${CREDITS_XML}")
    source_group("" FILES "${CREDITS_XML}")
    list(APPEND SOURCES "${CREDITS_XML}")
endif()

foreach(_assetfile IN LISTS FILE_ASSETS)
    source_group("Assets" FILES "${_assetfile}")
endforeach()

# Executable / game target
# Android builds the game as a shared library ("main") loaded by SDLActivity.
if(PLATFORM STREQUAL "Android")
    set(ICG_GAME_TARGET main)
    add_library(${ICG_GAME_TARGET} SHARED ${SOURCES} ${FILE_ASSETS} ${FONT_C_FILES} ${AUDIO_C_FILES}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/render/glad/src/gl.c)
else()
    set(ICG_GAME_TARGET ${PROJECT_NAME})
    add_executable(${ICG_GAME_TARGET} ${SOURCES} ${FILE_ASSETS} ${FONT_C_FILES} ${AUDIO_C_FILES}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/render/glad/src/gl.c)
endif()
target_compile_definitions(${ICG_GAME_TARGET} PRIVATE ${PROJECT_DEFINES})
target_link_libraries(${ICG_GAME_TARGET} PRIVATE ${PROJ_LIB})
add_dependencies(${ICG_GAME_TARGET} ${PROJ_LIB})
if(ICG_EMBED_ASSETS)
    add_dependencies(${ICG_GAME_TARGET} generate_fonts generate_audio)
endif()

# Static library code is linked into a shared library on Android
set_target_properties(${PROJ_LIB} PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(PLATFORM STREQUAL "Web")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        SUFFIX ".html"
        LINK_FLAGS "--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/src/web/init.html"
    )
    # Package src/assets/ into the emscripten virtual filesystem (loaded
    # lazily as Incogine.data at runtime).
    target_link_options(${PROJECT_NAME} PRIVATE
        --preload-file ${CMAKE_CURRENT_SOURCE_DIR}/src/assets@assets
        -sFORCE_FILESYSTEM
    )
endif()

if(NOT PLATFORM STREQUAL "Android")
    add_custom_command(TARGET ${ICG_GAME_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:${PROJ_LIB}> $<TARGET_FILE_DIR:${ICG_GAME_TARGET}>
    )
endif()

# Copy assets next to the executable (desktop + iOS bundle). Android and
# Web package src/assets/ through their own mechanisms (gradle assets
# source set, emscripten preload).
if(NOT PLATFORM STREQUAL "Android" AND NOT PLATFORM STREQUAL "Web")
    add_custom_command(TARGET ${ICG_GAME_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/src/assets" "$<TARGET_FILE_DIR:${ICG_GAME_TARGET}>/assets"
        COMMENT "Copying assets to $<TARGET_FILE_DIR:${ICG_GAME_TARGET}>/assets"
    )
endif()

# Platform-specific linking and packaging
if(PLATFORM STREQUAL "iOS")
    target_link_libraries(${ICG_GAME_TARGET} PRIVATE
        SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image SDL3_mixer::SDL3_mixer
        ${OpenGLES_LIBRARIES} ${COREFOUNDATION_LIBRARY} ${METAL_LIBRARY}
    )
elseif(PLATFORM STREQUAL "macOS")
    target_link_libraries(${ICG_GAME_TARGET} PRIVATE
        SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image SDL3_mixer::SDL3_mixer
        ${COREFOUNDATION_LIBRARY} ${METAL_LIBRARY}
    )
elseif(PLATFORM STREQUAL "Android")
    target_link_libraries(${ICG_GAME_TARGET} PRIVATE
        SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image SDL3_mixer::SDL3_mixer
        ${ANDROID_LOG_LIB}
    )
    set_target_properties(${ICG_GAME_TARGET} PROPERTIES
        LINK_FLAGS "-u ANativeActivity_onCreate"
    )
    target_compile_definitions(${ICG_GAME_TARGET} PRIVATE -DANDROID)
elseif(PLATFORM STREQUAL "Linux" OR PLATFORM STREQUAL "Ubuntu" OR
       PLATFORM STREQUAL "mira_linux" OR PLATFORM STREQUAL "Linux - Unknown" OR
       PLATFORM STREQUAL "Linux - Unknown Distribution")
    target_link_libraries(${ICG_GAME_TARGET} PRIVATE
        SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image SDL3_mixer::SDL3_mixer
    )
elseif(PLATFORM STREQUAL "Windows")
    target_link_libraries(${ICG_GAME_TARGET} PRIVATE
        SDL3 SDL3_ttf SDL3_image SDL3_mixer
        d3d11 d3d12 d3dcompiler
    )

    # Copy runtime DLLs next to the executable
    foreach(_sdl IN ITEMS SDL3 SDL3_ttf SDL3_image SDL3_mixer)
        add_custom_command(TARGET ${ICG_GAME_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${${_sdl}_LIB_DIR}/${_sdl}.dll" "$<TARGET_FILE_DIR:${ICG_GAME_TARGET}>"
        )
    endforeach()
elseif(PLATFORM STREQUAL "Web")
    target_link_libraries(${ICG_GAME_TARGET} PRIVATE
        SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image SDL3_mixer::SDL3_mixer
    )
endif()

if(PLATFORM STREQUAL "iOS" OR PLATFORM STREQUAL "macOS")
    if(PLATFORM STREQUAL "iOS")
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0")

        add_custom_command(TARGET ${ICG_GAME_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/bundle/ios/Assets.xcassets $<TARGET_FILE_DIR:${ICG_GAME_TARGET}>/Assets.xcassets
        )

        add_custom_command(TARGET ${ICG_GAME_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/bundle/ios/LaunchScreen.storyboard $<TARGET_FILE_DIR:${ICG_GAME_TARGET}>
        )

        add_custom_command(
            OUTPUT ${CMAKE_BINARY_DIR}/Info.plist
            COMMAND ${Python3_EXECUTABLE} ${PARSER_DIR}/ios_infoplist_gen.py
                "${WINDOW_NAME}" "${PROJECT_NAME}" "${BUNDLE_IDENTIFIER}" "${PROJECT_VERSION}" "${PROJECT_NAME}"
            DEPENDS ${PARSER_DIR}/ios_infoplist_gen.py
            COMMENT "Generating Info.plist"
        )
        add_custom_target(InfoPlist ALL DEPENDS ${CMAKE_BINARY_DIR}/Info.plist)
        add_dependencies(${ICG_GAME_TARGET} InfoPlist)
        set_target_properties(${ICG_GAME_TARGET} PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_BINARY_DIR}/Info.plist"
        )
    else()
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")
        set_target_properties(${ICG_GAME_TARGET} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME ${WINDOW_NAME}
            MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
            MACOSX_BUNDLE_SHORT_VERSION_STRING ${INCOGINE_VERSION}
            MACOSX_BUNDLE_LONG_VERSION_STRING ${INCOGINE_VERSION}
            MACOSX_BUNDLE_COPYRIGHT "${PROJECT_COPYRIGHT}\nIncogine © 2026 leafstudiosDot"
        )
        set(MACOSX_BUNDLE_ICON_FILE "AppIcon.icns")
        set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/bundle/macos/AppIcon.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        target_sources(${ICG_GAME_TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/bundle/macos/AppIcon.icns)
    endif()
elseif(PLATFORM STREQUAL "Windows")
    find_program(CMAKE_RC_COMPILER NAMES rc.exe)
    if(NOT CMAKE_RC_COMPILER)
        message(FATAL_ERROR "Resource compiler (rc.exe) not found. Please ensure Visual Studio is installed and configured correctly.")
    endif()

    add_custom_command(
        TARGET ${ICG_GAME_TARGET}
        PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/bundle/windows/icon.ico ${CMAKE_BINARY_DIR}
        COMMENT "Copying icon file"
    )

    add_custom_command(
        TARGET ${ICG_GAME_TARGET}
        PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/bundle/windows/resource.rc ${CMAKE_BINARY_DIR}
        COMMENT "Copying resource script file"
    )

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/resource.res
        COMMAND ${CMAKE_RC_COMPILER} /fo ${CMAKE_BINARY_DIR}/resource.res ${CMAKE_BINARY_DIR}/resource.rc
        DEPENDS ${CMAKE_BINARY_DIR}/resource.rc
        COMMENT "Compiling resource file"
    )

    target_sources(${ICG_GAME_TARGET} PRIVATE ${CMAKE_BINARY_DIR}/resource.rc)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_custom_target(run_debug
            COMMAND "$<TARGET_FILE:${ICG_GAME_TARGET}>" -dev --skipSplash
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Running application with -dev (skipping splash)"
        )
    elseif(CMAKE_BUILD_TYPE STREQUAL "Debug (with Splash scene)")
        add_custom_target(run_debug
            COMMAND "$<TARGET_FILE:${ICG_GAME_TARGET}>" -dev
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Running application with -dev (with splash scene)"
        )
    endif()
endif()