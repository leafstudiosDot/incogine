# Incogine — top-level CMake configuration
cmake_minimum_required(VERSION 3.26.3)
project(Incogine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_C_COMPILER "/usr/bin/gcc")
    set(CMAKE_CXX_COMPILER "/usr/bin/g++")
endif()

# Project settings (extracted from src/project.xml)
file(READ "${CMAKE_SOURCE_DIR}/src/project.xml" PROJECT_XML_CONTENT)

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
if(PLATFORM STREQUAL "Web")
    set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/emsdk/upstream/emscripten/system")

    find_package(SDL3 REQUIRED)
    find_package(SDL3_ttf REQUIRED)
    find_package(SDL3_image REQUIRED)

    include_directories(${SDL3_INCLUDE_DIRS} ${SDL3_TTF_INCLUDE_DIRS} ${SDL3_IMAGE_INCLUDE_DIRS})
elseif(PLATFORM STREQUAL "Windows")
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
    icg_setup_sdl3(SDL3_ttf   include/SDL3_ttf)
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
elseif(PLATFORM STREQUAL "iOS" OR PLATFORM STREQUAL "macOS")
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/reqs/lib/SDL2.framework")
    find_package(SDL2 REQUIRED COMPONENTS SDL2)
    include_directories(${SDL2_INCLUDE_DIRS})

    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/reqs/lib/SDL2_ttf.framework")
    find_package(SDL2_ttf REQUIRED COMPONENTS SDL2_ttf)
    include_directories(${SDL2_TTF_INCLUDE_DIRS})

    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/reqs/lib/SDL2_image.framework")
    find_package(SDL2_image REQUIRED COMPONENTS SDL2_image)
    include_directories(${SDL2_IMAGE_INCLUDE_DIRS})

    find_library(METAL_LIBRARY Metal)
    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
elseif(PLATFORM STREQUAL "Android")
    find_package(SDL2 REQUIRED)

    add_library(SDL2 SHARED IMPORTED)
    set_target_properties(SDL2 PROPERTIES
        IMPORTED_LOCATION "${SDL2_PATH}/lib/armeabi-v7a/libSDL2.so"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_PATH}"
    )

    find_library(ANDROID_LOG_LIB log)
    set(OPENGL_LIB GLESv1_CM)

    add_definitions("-DANDROID")
endif()

# OpenGL (skipped on iOS/Android, which use their own GL)
if(NOT PLATFORM STREQUAL "iOS" AND NOT PLATFORM STREQUAL "Android")
    find_package(OpenGL REQUIRED)
    include_directories(${OPENGL_INCLUDE_DIR})
    link_directories(${OPENGL_gl_LIBRARY})
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/reqs/GLEW")
endif()

# Generated resources
set(FONT_DIR "${CMAKE_SOURCE_DIR}/src/fonts")
set(PARSER_DIR "${CMAKE_SOURCE_DIR}/src/parser")

# Fonts: TTF -> C source/header pairs
set(FONT_C_FILES
    "${FONT_DIR}/main_font.c"
    "${FONT_DIR}/jpsup_font.c"
)
set(FONT_H_FILES
    "${FONT_DIR}/main_font.h"
    "${FONT_DIR}/jpsup_font.h"
)

add_custom_command(
    OUTPUT "${FONT_DIR}/main_font.h" "${FONT_DIR}/main_font.c"
    COMMAND ${Python3_EXECUTABLE} "${PARSER_DIR}/ttfparse_main.py"
            "${FONT_DIR}/main_font.ttf" "${FONT_DIR}/main_font"
    DEPENDS "${FONT_DIR}/main_font.ttf" "${PARSER_DIR}/ttfparse_main.py"
    COMMENT "Generating main_font.h/.c from main_font.ttf"
)

add_custom_command(
    OUTPUT "${FONT_DIR}/jpsup_font.h" "${FONT_DIR}/jpsup_font.c"
    COMMAND ${Python3_EXECUTABLE} "${PARSER_DIR}/ttfparse.py"
            "${FONT_DIR}/jpsup_font.ttf" "${FONT_DIR}/jpsup_font" _jpsup
    DEPENDS "${FONT_DIR}/jpsup_font.ttf" "${PARSER_DIR}/ttfparse.py"
    COMMENT "Generating jpsup_font.h/.c from jpsup_font.ttf"
)

add_custom_target(generate_fonts ALL DEPENDS ${FONT_H_FILES} ${FONT_C_FILES})

# SVG: root-level .svg files -> _svgdata.h headers
set(SVG_PARSER_SCRIPT "${PARSER_DIR}/svgparse.py")
file(GLOB SVG_FILES "${CMAKE_SOURCE_DIR}/*.svg")
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
add_dependencies(${PROJ_LIB} generate_fonts)
target_include_directories(${PROJ_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fonts
)
target_compile_definitions(${PROJ_LIB} PUBLIC ${PROJECT_DEFINES})
icg_group_sources(PROJ_LIB_SOURCES)

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

if(EXISTS "${CMAKE_SOURCE_DIR}/src/project.xml")
    source_group("" FILES "${CMAKE_SOURCE_DIR}/src/project.xml")
    list(APPEND SOURCES "${CMAKE_SOURCE_DIR}/src/project.xml")
endif()

set(CREDITS_XML "${CMAKE_SOURCE_DIR}/src/credits.xml")
if(EXISTS "${CREDITS_XML}")
    source_group("" FILES "${CREDITS_XML}")
    list(APPEND SOURCES "${CREDITS_XML}")
endif()

foreach(_assetfile IN LISTS FILE_ASSETS)
    source_group("Assets" FILES "${_assetfile}")
endforeach()

# Executable target
add_executable(${PROJECT_NAME} ${SOURCES} ${FILE_ASSETS} ${FONT_C_FILES})
target_compile_definitions(${PROJECT_NAME} PRIVATE ${PROJECT_DEFINES})
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJ_LIB})
add_dependencies(${PROJECT_NAME} ${PROJ_LIB} generate_fonts)

if(PLATFORM STREQUAL "Web")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        SUFFIX ".html"
        LINK_FLAGS "-s USE_SDL=3 --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/src/web/init.html"
    )
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:${PROJ_LIB}> $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

# Platform-specific linking and packaging
if(PLATFORM STREQUAL "iOS")
    find_library(OpenGLES_LIBRARIES OpenGLES)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        SDL2::SDL2 SDL2_ttf::SDL2_ttf SDL2_image::SDL2_image
        ${OpenGLES_LIBRARIES}
    )
elseif(PLATFORM STREQUAL "macOS")
    target_link_libraries(${PROJECT_NAME} PRIVATE
        SDL2::SDL2 SDL2_ttf::SDL2_ttf SDL2_image::SDL2_image
        ${OPENGL_LIBRARIES}
    )
elseif(PLATFORM STREQUAL "Windows")
    target_link_libraries(${PROJECT_NAME} PRIVATE
        SDL3 SDL3_ttf SDL3_image SDL3_mixer
        ${OPENGL_LIBRARIES}
        d3d11 d3d12 d3dcompiler
    )

    # Copy runtime DLLs next to the executable
    foreach(_sdl IN ITEMS SDL3 SDL3_ttf SDL3_image SDL3_mixer)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${${_sdl}_LIB_DIR}/${_sdl}.dll" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        )
    endforeach()

    # Copy audio assets to <exe-dir>/data/audio
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/src/assets/audio" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/data/audio"
    )
elseif(PLATFORM STREQUAL "Android")
    target_link_libraries(${PROJECT_NAME} PRIVATE
        SDL2
        ${ANDROID_LOG_LIB}
        ${OPENGL_LIB}
    )
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LINK_FLAGS "-u ANativeActivity_onCreate"
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE -DANDROID)
endif()

if(PLATFORM STREQUAL "iOS" OR PLATFORM STREQUAL "macOS")
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${COREFOUNDATION_LIBRARY} ${METAL_LIBRARY}
    )

    if(PLATFORM STREQUAL "iOS")
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0")

        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/bundle/ios/Assets.xcassets $<TARGET_FILE_DIR:${PROJECT_NAME}>/Assets.xcassets
        )

        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/bundle/ios/LaunchScreen.storyboard $<TARGET_FILE_DIR:${PROJECT_NAME}>
        )

        add_custom_command(
            OUTPUT ${CMAKE_BINARY_DIR}/Info.plist
            COMMAND ${Python3_EXECUTABLE} ${PARSER_DIR}/ios_infoplist_gen.py
                "${WINDOW_NAME}" "${PROJECT_NAME}" "${BUNDLE_IDENTIFIER}" "${PROJECT_VERSION}" "${PROJECT_NAME}"
            DEPENDS ${PARSER_DIR}/ios_infoplist_gen.py
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
            MACOSX_BUNDLE_COPYRIGHT "${PROJECT_COPYRIGHT}\nIncogine © 2026 leafstudiosDot"
        )
        set(MACOSX_BUNDLE_ICON_FILE "AppIcon.icns")
        set_source_files_properties(${CMAKE_SOURCE_DIR}/bundle/macos/AppIcon.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/bundle/macos/AppIcon.icns)
    endif()
elseif(PLATFORM STREQUAL "Windows")
    find_program(CMAKE_RC_COMPILER NAMES rc.exe)
    if(NOT CMAKE_RC_COMPILER)
        message(FATAL_ERROR "Resource compiler (rc.exe) not found. Please ensure Visual Studio is installed and configured correctly.")
    endif()

    add_custom_command(
        TARGET ${PROJECT_NAME}
        PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/bundle/windows/icon.ico ${CMAKE_BINARY_DIR}
        COMMENT "Copying icon file"
    )

    add_custom_command(
        TARGET ${PROJECT_NAME}
        PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/bundle/windows/resource.rc ${CMAKE_BINARY_DIR}
        COMMENT "Copying resource script file"
    )

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/resource.res
        COMMAND ${CMAKE_RC_COMPILER} /fo ${CMAKE_BINARY_DIR}/resource.res ${CMAKE_BINARY_DIR}/resource.rc
        DEPENDS ${CMAKE_BINARY_DIR}/resource.rc
        COMMENT "Compiling resource file"
    )

    target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR}/resource.rc)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_custom_target(run_debug
            COMMAND "$<TARGET_FILE:${PROJECT_NAME}>" -dev --skipSplash
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Running application with -dev (skipping splash)"
        )
    elseif(CMAKE_BUILD_TYPE STREQUAL "Debug (with Splash scene)")
        add_custom_target(run_debug
            COMMAND "$<TARGET_FILE:${PROJECT_NAME}>" -dev
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Running application with -dev (with splash scene)"
        )
    endif()
endif()