# Incogine Studio — CMake fragment (included via add_subdirectory from the
# top-level CMakeLists.txt; do NOT add project() / cmake_minimum_required).
# Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#
# Targets:
#   IncogineStudioCore — Qt-free models (project/credits XML, scene
#                        discovery, .incoba). Always built when Studio is on.
#   IncogineIncoba     — headless `incoba_packer` CLI. Always built with Core.
#   IncogineStudio     — Qt Widgets IDE shell. Only when Qt6 Widgets is found;
#                        otherwise skipped with a status message (runtime never
#                        links Qt).

# Windows Qt hint (harmless when absent or already configured).
if(WIN32 AND EXISTS "C:/Qt/6.10.2/msvc2022_64")
    list(APPEND CMAKE_PREFIX_PATH "C:/Qt/6.10.2/msvc2022_64")
endif()

set(ICG_STUDIO_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/core/project_paths.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/project_xml.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/credits_xml.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/scene_discovery.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/incoba.cpp
)

add_library(IncogineStudioCore STATIC ${ICG_STUDIO_CORE_SOURCES})
target_include_directories(IncogineStudioCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(IncogineStudioCore PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(IncogineStudioCore PRIVATE /utf-8)
endif()

add_executable(IncogineIncoba ${CMAKE_CURRENT_SOURCE_DIR}/tools/incoba_packer.cpp)
target_link_libraries(IncogineIncoba PRIVATE IncogineStudioCore)
if(MSVC)
    target_compile_options(IncogineIncoba PRIVATE /utf-8)
endif()

# Group Studio sources in Visual Studio Solution Explorer.
source_group("Studio\\Core" FILES ${ICG_STUDIO_CORE_SOURCES})
source_group("Studio\\Tools" FILES ${CMAKE_CURRENT_SOURCE_DIR}/tools/incoba_packer.cpp)

# Optional Qt IDE shell (desktop dev tool only; never linked into the game).
find_package(Qt6 QUIET COMPONENTS Widgets Multimedia)
if(Qt6Widgets_FOUND)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(ICG_STUDIO_UI_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/studio_main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/main_window.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/main_window.h
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/code_editor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/code_editor.h
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/search_panel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/search_panel.h
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/font_preview.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/font_preview.h
    )
    add_executable(IncogineStudio ${ICG_STUDIO_UI_SOURCES})
    target_link_libraries(IncogineStudio PRIVATE IncogineStudioCore Qt6::Widgets)
    if(Qt6Multimedia_FOUND)
        # Audio preview tabs (play/seek/duration/loop). Without it the shell
        # still builds; audio files just open as text with a log note.
        target_sources(IncogineStudio PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/ui/audio_preview.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/ui/audio_preview.h
        )
        target_link_libraries(IncogineStudio PRIVATE Qt6::Multimedia)
        target_compile_definitions(IncogineStudio PRIVATE ICG_STUDIO_HAS_MULTIMEDIA)
        source_group("Studio\\UI" FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/ui/audio_preview.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/ui/audio_preview.h
        )
        message(STATUS "Incogine Studio: Qt6 Multimedia found — audio preview enabled")
    else()
        message(STATUS "Incogine Studio: Qt6 Multimedia not found — audio preview disabled")
    endif()
    target_include_directories(IncogineStudio PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/ui
        ${CMAKE_CURRENT_SOURCE_DIR}/core
    )
    if(MSVC)
        target_compile_options(IncogineStudio PRIVATE /utf-8)
    endif()
    if(WIN32)
        set_target_properties(IncogineStudio PROPERTIES WIN32_EXECUTABLE TRUE)
        # Same icon as the game executable: stage icon.ico next to a
        # Studio .rc (mirrors the game's resource.rc flow) and link it in.
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../../bundle/windows/icon.ico"
                       "${CMAKE_CURRENT_BINARY_DIR}/studio_icon.ico" COPYONLY)
        file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/studio.rc"
             DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
        target_sources(IncogineStudio PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/studio.rc")
    endif()
    source_group("Studio\\UI" FILES ${ICG_STUDIO_UI_SOURCES})
    message(STATUS "Incogine Studio: Qt6 Widgets found — IncogineStudio target enabled")
else()
    message(STATUS "Incogine Studio: Qt6 Widgets not found — IDE shell skipped (Core + IncogineIncoba still build)")
endif()
