cmake_minimum_required(VERSION 3.26.3)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_C_COMPILER "/usr/bin/gcc")
    set(CMAKE_CXX_COMPILER "/usr/bin/g++")
endif()

project(Incogine)

# Project Scripts (Incogine Puroko)
set(PROJSNAME Puroko)

file(GLOB_RECURSE PROJST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/project/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/project/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scenes/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scenes/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fonts/*.h
)

file(GLOB_RECURSE _RM_PROJST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scenes/splash/*
)
list(REMOVE_ITEM PROJST_SOURCES ${_RM_PROJST_SOURCES})
add_library(${PROJSNAME} STATIC ${PROJST_SOURCES})

target_include_directories(${PROJSNAME} PUBLIC ${PROJST_SOURCES})

foreach(_projfiles IN ITEMS ${PROJST_SOURCES})
    get_filename_component(_source_path "${_projfiles}" PATH)
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" _group_path "${_source_path}")
    string(REPLACE "/" "\\" _group_path "${_group_path}")
    source_group("${_group_path}" FILES "${_projfiles}")
endforeach()


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
    set(ANDROID_NATIVE_APP_GLUE_LIB "${CMAKE_CURRENT_SOURCE_DIR}/reqs/android_ndk/sources/android/native_app_glue")
    set(ANDROID_ABI "arm64-v8a")
    set(ANDROID_PLATFORM_LEVEL "android-26")
    set(CMAKE_ANDROID_NDK_TOOLCHAIN_VERSION clang)
elseif(MIRA)
    set(PLATFORM "mira")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
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

if(PLATFORM STREQUAL "Web")
    set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/emsdk/upstream/emscripten/system")

    find_package(SDL3 REQUIRED)
    find_package(SDL3_ttf REQUIRED)
    find_package(SDL3_image REQUIRED)

    include_directories(${SDL3_INCLUDE_DIRS} ${SDL3_TTF_INCLUDE_DIRS} ${SDL3_IMAGE_INCLUDE_DIRS})

    set_target_properties(${PROJECT_NAME} PROPERTIES
        SUFFIX ".html"
        LINK_FLAGS "-s USE_SDL=3 --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/src/web/init.html"
    )
endif()

if (WIN32)
    if (MSVC)
        add_compile_options(/utf-8)
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
            CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-fexec-charset=UTF-8)
    endif()

    # SDL3
    set(SDL3_PATH "${CMAKE_CURRENT_SOURCE_DIR}/reqs/SDL3")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(SDL3_LIB_DIR ${SDL3_PATH}/lib/x64)
        set(SDL3_BIN_DIR ${SDL3_PATH}/bin/x64)
    else()
        set(SDL3_LIB_DIR ${SDL3_PATH}/lib/x86)
        set(SDL3_BIN_DIR ${SDL3_PATH}/bin/x86)
    endif()

    include_directories(${SDL3_PATH}/include)
    link_directories(${SDL3_LIB_DIR})

    # SDL3_ttf
    set(SDL3_TTF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/reqs/SDL3_ttf")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(SDL3_TTF_LIB_DIR ${SDL3_TTF_PATH}/lib/x64)
        set(SDL3_TTF_BIN_DIR ${SDL3_TTF_PATH}/bin/x64)
    else()
        set(SDL3_TTF_LIB_DIR ${SDL3_TTF_PATH}/lib/x86)
        set(SDL3_TTF_BIN_DIR ${SDL3_TTF_PATH}/bin/x86)
    endif()

    include_directories(${SDL3_TTF_PATH}/include)
    link_directories(${SDL3_TTF_LIB_DIR})

    # SDL3_image
    set(SDL3_IMAGE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/reqs/SDL3_image")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(SDL3_IMAGE_LIB_DIR ${SDL3_IMAGE_PATH}/lib/x64)
        set(SDL3_IMAGE_BIN_DIR ${SDL3_IMAGE_PATH}/bin/x64)
    else()
        set(SDL3_IMAGE_LIB_DIR ${SDL3_IMAGE_PATH}/lib/x86)
        set(SDL3_IMAGE_BIN_DIR ${SDL3_IMAGE_PATH}/bin/x86)
    endif()
        
    include_directories(${SDL3_IMAGE_PATH}/include)
    link_directories(${SDL3_IMAGE_LIB_DIR})

    # SDL3_mixer
    set(SDL3_MIXER_PATH "${CMAKE_CURRENT_SOURCE_DIR}/reqs/SDL3_mixer")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(SDL3_MIXER_LIB_DIR ${SDL3_MIXER_PATH}/lib/x64)
        set(SDL3_MIXER_BIN_DIR ${SDL3_MIXER_PATH}/bin/x64)
    else()
        set(SDL3_MIXER_LIB_DIR ${SDL3_MIXER_PATH}/lib/x86)
        set(SDL3_MIXER_BIN_DIR ${SDL3_MIXER_PATH}/bin/x86)
    endif()

    include_directories(${SDL3_MIXER_PATH}/include)
    link_directories(${SDL3_MIXER_LIB_DIR})
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

if (ANDROID)
    find_package(SDL2 REQUIRED)

    add_library(SDL2 SHARED IMPORTED)
    set_target_properties(SDL2 PROPERTIES
        IMPORTED_LOCATION "${SDL2_PATH}/lib/armeabi-v7a/libSDL2.so"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_PATH}"
    )
endif()

# OpenGL
if (CMAKE_SYSTEM_NAME STREQUAL "iOS" OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    # Do nothing
else()
    find_package(OpenGL REQUIRED)
    include_directories(${OPENGL_INCLUDE_DIR})
    link_directories(${OPENGL_gl_LIBRARY})
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/reqs/GLEW")
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
    set(OPENGL_LIB GLESv1_CM)

    add_definitions("-DANDROID")
endif()


# Fonts
set(MAIN_FONT_FILE "${CMAKE_SOURCE_DIR}/src/fonts/main_font.ttf")
set(MAIN_FONTPARSER_SCRIPT "${CMAKE_SOURCE_DIR}/src/parser/ttfparse_main.py")

add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/src/fonts/main_font.h
    COMMAND python3 ${MAIN_FONTPARSER_SCRIPT} ${MAIN_FONT_FILE} ${CMAKE_SOURCE_DIR}/src/fonts/main_font
    DEPENDS ${MAIN_FONT_FILE} ${MAIN_FONTPARSER_SCRIPT}
    COMMENT "Generating main_font.h from main_font.ttf"
)

set(JPSUP_FONT_FILE "${CMAKE_SOURCE_DIR}/src/fonts/jpsup_font.ttf")
set(JPSUP_FONTPARSER_SCRIPT "${CMAKE_SOURCE_DIR}/src/parser/ttfparse.py")

add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/src/fonts/jpsup_font.h
    COMMAND python3 ${JPSUP_FONTPARSER_SCRIPT} ${JPSUP_FONT_FILE} ${CMAKE_SOURCE_DIR}/src/fonts/jpsup_font _jpsup
    DEPENDS ${JPSUP_FONT_FILE} ${JPSUP_FONTPARSER_SCRIPT}
    COMMENT "Generating jpsup_font.h from jpsup_font.ttf"
)

file(GLOB ALL_FONT_HEADERS "${CMAKE_SOURCE_DIR}/src/fonts/*.h")
add_custom_target(generate_fonts ALL DEPENDS
    ${ALL_FONT_HEADERS}
)
file(GLOB ALL_FONT_CS "${CMAKE_SOURCE_DIR}/src/fonts/*.c")
set(FONT_C_FILES
    ${ALL_FONT_CS}
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/fonts)


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
#add_custom_target(generate_headers ALL DEPENDS ${SVG_HEADERS})


# Project Settings (project.xml)
file(GLOB PROJECT_XML "${CMAKE_SOURCE_DIR}/src/project.xml")
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
file(GLOB_RECURSE FILE_ASSETS ${CMAKE_CURRENT_SOURCE_DIR}/src/assets/*)

file(GLOB_RECURSE RM_PRT_SRC
${CMAKE_CURRENT_SOURCE_DIR}/src/project/*
)

list(REMOVE_ITEM CPP_SOURCES ${RM_PRT_SRC})
list(REMOVE_ITEM CPP_SOURCES ${RM_PRT_SRC})
list(REMOVE_ITEM H_SOURCES ${RM_PRT_SRC})
list(REMOVE_ITEM H_SOURCES ${RM_PRT_SRC})

list(REMOVE_ITEM CPP_SOURCES ${PROJST_SOURCES})
list(REMOVE_ITEM H_SOURCES ${PROJST_SOURCES})

set(SOURCES ${CPP_SOURCES} ${H_SOURCES})

foreach(_source IN ITEMS ${SOURCES})
    get_filename_component(_source_path "${_source}" PATH)
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" _group_path "${_source_path}")
    string(REPLACE "/" "\\" _group_path "${_group_path}")
    source_group("${_group_path}" FILES "${_source}")
endforeach()

set(ASSETS_SOURCES ${FILE_ASSETS})
foreach(_assetfile IN ITEMS ${FILE_ASSETS})
    get_filename_component(_assetfile_path "${_assetfile}" PATH)
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" _group_path "${_assetfile_path}")
    string(REPLACE "/" "\\" _group_path "${_group_path}")
    source_group("Assets" FILES "${_assetfile}")
endforeach()

if(PROJECT_XML)
    source_group("" FILES ${PROJECT_XML})
    list(APPEND SOURCES ${PROJECT_XML})
endif()

add_executable(${PROJECT_NAME} ${SOURCES} ${ASSETS_SOURCES} ${FONT_C_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJSNAME})
add_dependencies(${PROJECT_NAME} ${PROJSNAME} generate_fonts)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:${PROJSNAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

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
        "${SDL3_PATH}/include"
        "${SDL3_IMAGE_PATH}/include"
        "${SDL3_TTF_PATH}/include"
        "${SDL3_MIXER_PATH}/include"
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE SDL3 SDL3_ttf SDL3_image SDL3_mixer)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENGL_LIBRARIES})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${FBX_LIBRARY})

    target_link_libraries(${PROJECT_NAME} PRIVATE d3d11 d3dcompiler)
    target_link_libraries(${PROJECT_NAME} PRIVATE d3d12 d3dcompiler)

    # Assets Audio Files
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/src/assets/audio" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/data/audio")

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SDL3_LIB_DIR}/SDL3.dll $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL3_IMAGE_LIB_DIR}/SDL3_image.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL3_TTF_LIB_DIR}/SDL3_ttf.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL3_MIXER_LIB_DIR}/SDL3_mixer.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
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
        target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/bundle/macos/AppIcon.icns ${FONT_C_FILES})
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

    target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR}/resource.rc ${FONT_C_FILES})

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_custom_target(run_debug
            COMMAND ${CMAKE_COMMAND} ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.exe -dev --skipSplash
            COMMENT "Running application with -dev argument"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug (with Splash scene)")
    add_custom_target(run_debug
        COMMAND ${CMAKE_COMMAND} ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.exe -dev
        COMMENT "Running application with -dev argument along with a splash scene"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    endif()

    #add_custom_target(run ALL
    #    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
    #    COMMAND ${CMAKE_COMMAND} --target run_debug
    #    COMMENT "Build and run the application"
    #)
endif()

# Android Bundle properties
if(ANDROID)
    target_link_libraries(${PROJECT_NAME}
        SDL2
        ${ANDROID_LOG_LIB}
        #${ANDROID_NATIVE_APP_GLUE_LIB}
        ${OPENGL_LIB}
    )

    set_target_properties(${PROJECT_NAME} PROPERTIES
        LINK_FLAGS "-u ANativeActivity_onCreate"
    )

    target_compile_definitions(${PROJECT_NAME} PRIVATE -DANDROID)
endif()