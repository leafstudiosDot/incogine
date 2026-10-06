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

# Scripting options
option(ICG_SCRIPTING_CSHARP "Enable C# scripting via .NET 10 CoreCLR" ON)
option(ICG_SCRIPTING_KODO "Enable Kodo scripting (placeholder until language is defined)" ON)

if(ICG_SCRIPTING_CSHARP)
    find_program(DOTNET_EXECUTABLE dotnet)
    if(DOTNET_EXECUTABLE)
        message(STATUS "Found dotnet SDK: ${DOTNET_EXECUTABLE}")
        add_compile_definitions(ICG_SCRIPTING_CSHARP)
    else()
        message(WARNING "dotnet SDK not found — disabling C# scripting")
        set(ICG_SCRIPTING_CSHARP OFF)
    endif()
endif()

if(ICG_SCRIPTING_KODO)
    add_compile_definitions(ICG_SCRIPTING_KODO)
endif()

# Incogine Studio IDE (Qt). Desktop dev tool only: the game/runtime targets
# below never link Studio sources or Qt (see the src/studio exclusion and the
# standalone targets in src/studio/CMakeLists.txt).
option(ICG_BUILD_STUDIO "Build Incogine Studio IDE and .incoba packer (Qt shell is optional)" ON)

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

# Kodo — scripting language placeholder (static library for VS visibility)
if(ICG_SCRIPTING_KODO)
    set(KODO_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/scripting/kodo/kodohandler.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/scripting/kodo/kodohandler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/scripting/kodo/kodoparser.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/scripting/kodo/kodoparser.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/scripting/kodo/kodolexer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/scripting/kodo/kodolexer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/scripting/kodo/kodoast.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/scripting/kodo/kodovalue.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/scripting/kodo/kodoengine.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/scripting/kodo/kodointerpreter.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/scripting/kodo/kodointerpreter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/scripting/kodo/kodoeval.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/scripting/kodo/kodobindings.cpp
    )
    add_library(Kodo STATIC ${KODO_SOURCES})
    target_include_directories(Kodo PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/scripting/kodo
    )
    target_compile_definitions(Kodo PUBLIC ${PROJECT_DEFINES})
    icg_group_sources(KODO_SOURCES)
    set_target_properties(Kodo PROPERTIES LINKER_LANGUAGE CXX)
    # kodoengine.h reaches engine.h/SDL via square.h; give the Kodo target
    # the same SDL headers the exe gets through Puroko.
    if(PLATFORM STREQUAL "Linux" OR PLATFORM STREQUAL "Ubuntu" OR
       PLATFORM STREQUAL "mira_linux" OR PLATFORM STREQUAL "Linux - Unknown" OR
       PLATFORM STREQUAL "Linux - Unknown Distribution" OR PLATFORM STREQUAL "macOS" OR
       PLATFORM STREQUAL "iOS" OR PLATFORM STREQUAL "Android" OR PLATFORM STREQUAL "Web")
        target_link_libraries(Kodo PUBLIC
            SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image SDL3_mixer::SDL3_mixer
        )
    endif()
endif()

# Engine modules — static libraries shared by the runtime and by Studio.
#
# IncogineAssets: the generic asset-importer seam (extension -> importer). It
# knows nothing about any specific format, so a future 3D importer (FBX, OBJ,
# glTF, .blend) registers the same way .incoanim does.
# IncogineAnim: the 2D vector animation model, geometry, .incoanim IO, and the
# .incoanim importer that plugs into IncogineAssets.
#
# Both are plain C++17 with no SDL and no Qt, which is deliberate: the SDL3
# runtime and the Qt-free test harness can both link them, and keeping the
# importer seam in its own target means a 3D importer never has to drag in the
# 2D model. Sources are excluded from the executable glob further down, so
# they are linked once rather than compiled twice.
set(ICG_ASSETS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/assets/assetimport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/assets/assetimport.h
)

set(ICG_ANIM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/animation/anim_types.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/animation/anim_types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/animation/anim_document.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/animation/anim_document.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/animation/anim_geometry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/animation/anim_geometry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/animation/anim_path.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/animation/anim_path.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/animation/anim_json.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/animation/anim_json.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/animation/anim_io.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/animation/anim_io.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/animation/anim2d_importer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/animation/anim2d_importer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/animation/anim_commands.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/animation/anim_commands.h
)

add_library(IncogineAssets STATIC ${ICG_ASSETS_SOURCES})
target_include_directories(IncogineAssets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/core)

add_library(IncogineAnim STATIC ${ICG_ANIM_SOURCES})
target_include_directories(IncogineAnim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/core)
target_link_libraries(IncogineAnim PUBLIC IncogineAssets)

set_target_properties(IncogineAssets IncogineAnim PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    LINKER_LANGUAGE CXX
)
if(MSVC)
    target_compile_options(IncogineAssets PRIVATE /utf-8)
    target_compile_options(IncogineAnim PRIVATE /utf-8)
endif()
icg_group_sources(ICG_ASSETS_SOURCES)
icg_group_sources(ICG_ANIM_SOURCES)

# ---- Headless test suites ----
# The repo had no test infrastructure, so this adds a deliberately tiny
# stdlib harness (tests/test_check.h) rather than a framework dependency. Each
# suite is its own executable with its own main(), so adding a module is a few
# lines of CMake instead of a central registration list.
#
# Enabled by BUILD_TESTING (the CTest convention, ON by default). Skipped on
# Android and Web, which cannot build host executables.
option(BUILD_TESTING "Build headless test suites" ON)
if(BUILD_TESTING AND NOT PLATFORM STREQUAL "Android" AND NOT PLATFORM STREQUAL "Web")
    enable_testing()

    add_executable(IncogineAnimTests ${CMAKE_CURRENT_SOURCE_DIR}/tests/anim_tests.cpp)
    target_include_directories(IncogineAnimTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    )
    target_link_libraries(IncogineAnimTests PRIVATE IncogineAnim)
    if(MSVC)
        target_compile_options(IncogineAnimTests PRIVATE /utf-8)
    endif()

    add_executable(IncogineAssetsTests ${CMAKE_CURRENT_SOURCE_DIR}/tests/asset_tests.cpp)
    target_include_directories(IncogineAssetsTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    )
    target_link_libraries(IncogineAssetsTests PRIVATE IncogineAnim)
    if(MSVC)
        target_compile_options(IncogineAssetsTests PRIVATE /utf-8)
    endif()

    # Run from the build dir so a suite that writes a temp file (the file-io
    # round-trip) has a writable cwd.
    add_test(NAME anim COMMAND IncogineAnimTests)
    add_test(NAME assets COMMAND IncogineAssetsTests)
    source_group("Tests" FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/anim_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/asset_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_check.h
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

# Same for the engine modules: they are linked as libraries, not recompiled.
list(REMOVE_ITEM CPP_SOURCES ${ICG_ASSETS_SOURCES})
list(REMOVE_ITEM H_SOURCES   ${ICG_ASSETS_SOURCES})
list(REMOVE_ITEM CPP_SOURCES ${ICG_ANIM_SOURCES})
list(REMOVE_ITEM H_SOURCES   ${ICG_ANIM_SOURCES})

# Studio is a developer-only tool: its sources must never link into the game
# executable / runtime (Staging+Release ship no Studio, no Qt).
file(GLOB_RECURSE STUDIO_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/studio/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/studio/*.h
)
list(REMOVE_ITEM CPP_SOURCES ${STUDIO_SOURCES})
list(REMOVE_ITEM H_SOURCES   ${STUDIO_SOURCES})

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

# Script files — visible in Visual Studio Solution Explorer
file(GLOB CSHARP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scripts/csharp/Incogine/*.cs
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scripts/csharp/Incogine/*.csproj
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scripts/csharp/Incogine/*.json
)
file(GLOB CSHARP_EXAMPLES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scripts/csharp/examples/*.cs
)
file(GLOB KODO_SCRIPTS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scripts/kodo/*.kodo
)
file(GLOB KODO_EXAMPLES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scripts/kodo/examples/*.kodo
)
source_group("Scripts\\CSharp\\Incogine" FILES ${CSHARP_SOURCES})
source_group("Scripts\\CSharp\\Examples" FILES ${CSHARP_EXAMPLES})
source_group("Scripts\\Kodo" FILES ${KODO_SCRIPTS})
source_group("Scripts\\Kodo\\Examples" FILES ${KODO_EXAMPLES})

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

# Engine modules: .incoanim loading, baking, and playback live here so the
# SDL3 runtime can read animation files with no Qt anywhere in the path.
target_link_libraries(${ICG_GAME_TARGET} PRIVATE IncogineAnim)
# Scripting P/Invoke resolves "Incogine" to the host executable
# (InteropLib.cs -> NativeLibrary.GetMainProgramHandle), so the game
# target must export its symbols (needed on Linux/macOS; harmless elsewhere).
set_target_properties(${ICG_GAME_TARGET} PROPERTIES ENABLE_EXPORTS TRUE)
if(ICG_EMBED_ASSETS)
    add_dependencies(${ICG_GAME_TARGET} generate_fonts generate_audio)
endif()

# Scripting: link Kodo placeholder library and build C# managed assembly
if(ICG_SCRIPTING_KODO)
    add_dependencies(${ICG_GAME_TARGET} Kodo)
    target_link_libraries(${ICG_GAME_TARGET} PRIVATE Kodo)
endif()

if(ICG_SCRIPTING_CSHARP AND DOTNET_EXECUTABLE)
    set(CSHARP_PROJECT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/scripts/csharp/Incogine")
    set(CSHARP_RUNTIMECONFIG "${CSHARP_PROJECT_DIR}/Incogine.runtimeconfig.json")

    # Locate .NET SDK native hosting headers and libraries (nethost, hostfxr)
    execute_process(
        COMMAND ${DOTNET_EXECUTABLE} --list-runtimes
        OUTPUT_VARIABLE DOTNET_RUNTIMES
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    # Find the .NET installation root
    get_filename_component(DOTNET_DIR ${DOTNET_EXECUTABLE} DIRECTORY)

    if(PLATFORM STREQUAL "Windows")
        set(NETHOST_SEARCH_PATHS
            "${DOTNET_DIR}"
            "C:/Program Files/dotnet/packs/Microsoft.NETCore.App.Host.win-x64"
        )
        set(NETHOST_RID "win-x64")
    elseif(PLATFORM STREQUAL "macOS")
        # Apple Silicon is the default target: x64 macOS binaries are being
        # discontinued, and desktop CI builds arm64-only. An explicit
        # -DCMAKE_OSX_ARCHITECTURES=x86_64 still selects the x64 pack for local
        # cross-builds; anything else (including "arm64;x86_64") prefers arm64.
        if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64" AND NOT CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
            set(NETHOST_RID "osx-x64")
        else()
            set(NETHOST_RID "osx-arm64")
        endif()
        set(NETHOST_SEARCH_PATHS
            "${DOTNET_DIR}"
            "/usr/local/share/dotnet/packs/Microsoft.NETCore.App.Host.${NETHOST_RID}"
            "/usr/local/share/dotnet/packs/Microsoft.NETCore.App.Host.osx-arm64"
            "/usr/local/share/dotnet/packs/Microsoft.NETCore.App.Host.osx-x64"
        )
    else()
        set(NETHOST_SEARCH_PATHS
            "${DOTNET_DIR}"
            "/usr/share/dotnet/packs/Microsoft.NETCore.App.Host.linux-x64"
        )
        set(NETHOST_RID "linux-x64")
    endif()

    # The hosting pack ships the hosting headers (nethost.h, hostfxr.h,
    # coreclr_delegates.h) and the nethost import lib side by side:
    #   packs/Microsoft.NETCore.App.Host.<rid>/<ver>/runtimes/<rid>/native/
    # Some layouts keep the headers in a sibling include/ dir instead, so
    # probe both. Several versions can be installed side by side — sort the
    # candidates newest-first with a natural compare (so 10.x beats 8.x) to
    # match the TFM in Incogine.runtimeconfig.json.
    set(NETHOST_VERSIONED_INCLUDE_DIRS "")
    set(NETHOST_VERSIONED_LIB_DIRS "")
    set(NETHOST_RUNTIME_DLLS "")

    # Probe roots: the platform search paths, the dotnet dir, and the
    # well-known pack locations (DOTNET_ROOT layouts vary).
    set(NETHOST_PROBE_ROOTS ${NETHOST_SEARCH_PATHS} "${DOTNET_DIR}"
        "${DOTNET_DIR}/packs/Microsoft.NETCore.App.Host.${NETHOST_RID}"
        "C:/Program Files/dotnet/packs/Microsoft.NETCore.App.Host.${NETHOST_RID}"
        "/usr/local/share/dotnet/packs/Microsoft.NETCore.App.Host.${NETHOST_RID}"
        "/usr/share/dotnet/packs/Microsoft.NETCore.App.Host.${NETHOST_RID}")

    foreach(_pack IN LISTS NETHOST_PROBE_ROOTS)
        # A probe root is either the pack dir itself or the dotnet dir that
        # contains packs/.
        foreach(_cand "${_pack}" "${_pack}/packs/Microsoft.NETCore.App.Host.${NETHOST_RID}")
            if(EXISTS "${_cand}")
                file(GLOB _natives "${_cand}/*/runtimes/${NETHOST_RID}/native")
                list(SORT _natives COMPARE NATURAL ORDER DESCENDING)
                foreach(_dir IN LISTS _natives)
                    # csharphost.cpp guards on all three headers with
                    # __has_include, so only offer a dir holding all of them —
                    # otherwise CMake would report success while the host
                    # still silently compiled as a disabled stub.
                    if(EXISTS "${_dir}/nethost.h" AND EXISTS "${_dir}/hostfxr.h"
                       AND EXISTS "${_dir}/coreclr_delegates.h")
                        list(APPEND NETHOST_VERSIONED_INCLUDE_DIRS "${_dir}")
                    endif()
                    # nethost.lib is the usual Windows import lib name; the
                    # Unix packs ship libnethost.lib (older layouts) or the
                    # static archive libnethost.a, which find_library() below
                    # only sees if this dir is offered as a search path.
                    if(EXISTS "${_dir}/nethost.lib" OR EXISTS "${_dir}/libnethost.lib"
                       OR EXISTS "${_dir}/libnethost.a")
                        list(APPEND NETHOST_VERSIONED_LIB_DIRS "${_dir}")
                        # Windows only: linking the import lib makes the game
                        # depend on nethost.dll, and the .NET install dir is not
                        # on PATH by default — so stage it next to the exe.
                        if(WIN32 AND EXISTS "${_dir}/nethost.dll")
                            list(APPEND NETHOST_RUNTIME_DLLS "${_dir}/nethost.dll")
                        endif()
                    endif()
                endforeach()
                # Fallback layouts: headers under <ver>/include, import lib
                # under <ver>/lib.
                file(GLOB _vers "${_cand}/*/include/nethost.h")
                foreach(_h IN LISTS _vers)
                    get_filename_component(_inc "${_h}" DIRECTORY)
                    list(APPEND NETHOST_VERSIONED_INCLUDE_DIRS "${_inc}")
                endforeach()
                file(GLOB _libs "${_cand}/*/lib/*nethost*.lib" "${_cand}/*/lib/*nethost*.a")
                foreach(_l IN LISTS _libs)
                    get_filename_component(_libdir "${_l}" DIRECTORY)
                    list(APPEND NETHOST_VERSIONED_LIB_DIRS "${_libdir}")
                endforeach()
            endif()
        endforeach()
    endforeach()
    list(REMOVE_DUPLICATES NETHOST_VERSIONED_INCLUDE_DIRS)
    list(REMOVE_DUPLICATES NETHOST_VERSIONED_LIB_DIRS)
    list(REMOVE_DUPLICATES NETHOST_RUNTIME_DLLS)

    find_path(NETHOST_INCLUDE_DIR nethost.h
        PATHS ${NETHOST_VERSIONED_INCLUDE_DIRS} ${NETHOST_SEARCH_PATHS}
        PATH_SUFFIXES include NO_DEFAULT_PATH)
    if(NOT NETHOST_INCLUDE_DIR)
        find_path(NETHOST_INCLUDE_DIR nethost.h PATH_SUFFIXES include)
    endif()
    find_library(NETHOST_LIBRARY nethost
        PATHS ${NETHOST_VERSIONED_LIB_DIRS} ${NETHOST_SEARCH_PATHS}
        PATH_SUFFIXES native NO_DEFAULT_PATH)
    if(NOT NETHOST_LIBRARY)
        find_library(NETHOST_LIBRARY nethost PATHS ${NETHOST_SEARCH_PATHS} PATH_SUFFIXES native)
    endif()
    if(NETHOST_LIBRARY AND NOT WIN32)
        # The Unix packs ship both libnethost.a and libnethost.so. find_library
        # prefers the shared object, which would add a runtime .so dependency
        # the game would have to ship — so take the static archive instead, for
        # the same reason SDL3 and the addons are forced to static above.
        get_filename_component(_nethost_lib_dir "${NETHOST_LIBRARY}" DIRECTORY)
        if(EXISTS "${_nethost_lib_dir}/libnethost.a")
            set(NETHOST_LIBRARY "${_nethost_lib_dir}/libnethost.a")
        endif()
    endif()

    if(NETHOST_INCLUDE_DIR)
        target_include_directories(${ICG_GAME_TARGET} PRIVATE ${NETHOST_INCLUDE_DIR})
        message(STATUS "C# scripting: nethost include dir: ${NETHOST_INCLUDE_DIR}")
    else()
        message(WARNING "C# scripting: nethost.h not found under "
            "${DOTNET_DIR}/packs/Microsoft.NETCore.App.Host.${NETHOST_RID} — "
            "C# host compiles as disabled stub. Install the .NET SDK hosting "
            "pack for ${NETHOST_RID} (usually shipped with the SDK), or point "
            "NETHOST_INCLUDE_DIR/NETHOST_LIBRARY at an existing install.")
    endif()

    if(NETHOST_LIBRARY)
        target_link_libraries(${ICG_GAME_TARGET} PRIVATE ${NETHOST_LIBRARY})
        message(STATUS "C# scripting: nethost library: ${NETHOST_LIBRARY}")
    else()
        message(WARNING "C# scripting: nethost library not found — C# host compiles as disabled stub")
    endif()

    # nethost.lib is an import lib, so the executable gains a load-time
    # dependency on nethost.dll. The .NET install dir is not on PATH by
    # default, so copy it next to the game or the process fails to start.
    foreach(_nethost_dll IN LISTS NETHOST_RUNTIME_DLLS)
        add_custom_command(TARGET ${ICG_GAME_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_nethost_dll}" "$<TARGET_FILE_DIR:${ICG_GAME_TARGET}>"
        )
        message(STATUS "C# scripting: staging nethost.dll: ${_nethost_dll}")
        break()
    endforeach()

    add_custom_target(IncogineScripting ALL
        COMMAND ${DOTNET_EXECUTABLE} build
            "${CSHARP_PROJECT_DIR}/Incogine.csproj"
            --configuration Release
            --output "$<TARGET_FILE_DIR:${ICG_GAME_TARGET}>"
            -p:DebugType=embedded
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CSHARP_RUNTIMECONFIG}"
            "$<TARGET_FILE_DIR:${ICG_GAME_TARGET}>/Incogine.runtimeconfig.json"
        COMMENT "Building C# managed assembly (Incogine.dll)"
    )
    # NOTE: DebugType=embedded keeps the managed debug info inside
    # Incogine.dll instead of emitting Incogine.pdb next to the game
    # executable — that filename belongs to the NATIVE linker (Incogine.exe)
    # and sharing it breaks every link with LNK1207. C# debugging in VS
    # keeps working with embedded portable PDBs.
    add_dependencies(${ICG_GAME_TARGET} IncogineScripting)
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

# Asset shipping is decided in the "Asset shipping mode" section at the end
# of this file (single ICG_USE_INCOBA switch: bundles-only vs loose).
# Android and Web package src/assets/ through their own mechanisms (gradle
# assets source set, emscripten preload).

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

    # Guarantee the resource files exist at generate time: a brand-fresh
    # binary dir has no PRE_BUILD outputs yet, and `target_sources` below
    # fails generation when the file is missing (seen with fresh VS 2026
    # `.slnx` configures). Mirrors the PRE_BUILD copies; harmless thereafter.
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/bundle/windows/icon.ico
         DESTINATION ${CMAKE_BINARY_DIR})
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/bundle/windows/resource.rc
         DESTINATION ${CMAKE_BINARY_DIR})

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

# Incogine Studio — developer-only tooling (excluded from Android/Web game
# packaging; the runtime never depends on these targets — see section 17 in
# docs/studio.md).
if(ICG_BUILD_STUDIO AND NOT PLATFORM STREQUAL "Android" AND NOT PLATFORM STREQUAL "Web")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/studio ${CMAKE_BINARY_DIR}/studio_build)
endif()

# Asset shipping mode (desktop + iOS bundle; Android and Web package
# src/assets/ through their own mechanisms).
#
# Single switch, exactly one layout — Dev, Debug, Staging, and Release all
# behave the same:
#   ICG_USE_INCOBA=ON  (default): <exe>/assets/ holds ONLY index.incobai +
#                       *.incoba (headless packer, no Studio/Qt needed).
#   ICG_USE_INCOBA=OFF: loose src/assets/ files are copied instead
#                       (direct-disk flow, no bundles involved).
# Generator-agnostic — Ninja, Make, and the Visual Studio generator (VS 2026
# `.slnx`) alike — because it only uses generator expressions and `cmake -E`.
# See docs/studio.md and docs/assets.md.
option(ICG_USE_INCOBA "Ship assets as .incoba bundles instead of loose files" ON)
set(ICG_INCOBA_MAX_MB "128" CACHE STRING "Payload cap in MiB per .incoba bundle")

if(NOT PLATFORM STREQUAL "Android" AND NOT PLATFORM STREQUAL "Web")
    if(ICG_USE_INCOBA AND TARGET IncogineIncoba)
        file(GLOB_RECURSE ICG_INCOBA_ASSET_FILES CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/*")
        set(ICG_INCOBA_STAGING_DIR "${CMAKE_BINARY_DIR}/incoba_staging")
        set(ICG_INCOBA_INDEX "${ICG_INCOBA_STAGING_DIR}/index.incobai")
        add_custom_command(
            OUTPUT "${ICG_INCOBA_INDEX}"
            COMMAND $<TARGET_FILE:IncogineIncoba>
                    --split "${CMAKE_CURRENT_SOURCE_DIR}/src/assets"
                    "${ICG_INCOBA_STAGING_DIR}"
                    --stem a --max-mb ${ICG_INCOBA_MAX_MB}
            DEPENDS IncogineIncoba ${ICG_INCOBA_ASSET_FILES}
            COMMENT "Splitting src/assets/ into .incoba bundles + index (headless packer, no Studio required)"
            VERBATIM
        )
        add_custom_target(IncogineIncobaBundle ALL DEPENDS "${ICG_INCOBA_INDEX}")
        # Ship bundles ONLY: clear the folder first so no stale loose files
        # (or older bundles) survive mode switches or asset deletions.
        add_custom_command(TARGET ${ICG_GAME_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:${ICG_GAME_TARGET}>/assets"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${ICG_INCOBA_STAGING_DIR}" "$<TARGET_FILE_DIR:${ICG_GAME_TARGET}>/assets"
            COMMENT "Shipping .incoba bundles ONLY into the executable assets folder"
        )
        add_dependencies(${ICG_GAME_TARGET} IncogineIncobaBundle)
    else()
        if(ICG_USE_INCOBA)
            message(WARNING "ICG_USE_INCOBA requested but the IncogineIncoba packer is unavailable (ICG_BUILD_STUDIO=OFF?) — falling back to loose assets")
        endif()
        # Direct-disk flow: loose files ONLY, no bundles.
        add_custom_command(TARGET ${ICG_GAME_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:${ICG_GAME_TARGET}>/assets"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/assets" "$<TARGET_FILE_DIR:${ICG_GAME_TARGET}>/assets"
            COMMENT "Copying loose assets ONLY to $<TARGET_FILE_DIR:${ICG_GAME_TARGET}>/assets"
        )
    endif()
endif()

# SHA-256 sidecar for Studio dev-build binding (desktop dev workflow only).
# Written next to the game binary on every link: Studio refuses to preview
# executables whose hash does not match (foreign projects, released or
# swapped-in binaries). See docs/studio.md and preview_exe.h.
if(NOT PLATFORM STREQUAL "Android" AND NOT PLATFORM STREQUAL "Web")
    add_custom_command(TARGET ${ICG_GAME_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
                -DEXE=$<TARGET_FILE:${ICG_GAME_TARGET}>
                -DOUT=$<TARGET_FILE_DIR:${ICG_GAME_TARGET}>/${PROJECT_NAME}.sha256
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sha256sidecar.cmake
        COMMENT "Writing SHA-256 sidecar for Studio dev-build binding"
    )
endif()