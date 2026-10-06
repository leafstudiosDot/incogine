# Incogine Studio — CMake fragment (included via add_subdirectory from the
# top-level CMakeLists.txt; do NOT add project() / cmake_minimum_required).
# Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#
# Targets:
#   IncogineStudioCore — Qt-free models (project/credits XML, scene
#                        discovery, .incoba). Always built when Studio is on.
#   IncogineIncoba     — headless `incoba_packer` CLI. Always built with Core.
#   IncogineAnimator   — Qt Widgets vector-animation editor. SEPARATE
#                        top-level app, so closing Studio leaves an open
#                        animation running.
#   IncogineStudio     — Qt Widgets IDE shell. Only when Qt6 Widgets is found;
#                        otherwise skipped with a status message (runtime never
#                        links Qt).

# Windows Qt hint. Qt is optional and its default install root is not on
# CMake's search path, so probe the usual locations and use the newest Qt 6
# kit that matches our compiler. An explicit -DQt6_DIR=... or a user-set
# CMAKE_PREFIX_PATH already on the command line always wins (we skip the hint
# when Qt6_DIR is cached). Default layout is <root>/<version>/<kit>, e.g.
# C:/Qt/6.12.0/msvc2022_64. Version-agnostic on purpose: do not pin a version.
if(WIN32 AND NOT Qt6_DIR)
    if(MSVC)
        set(_icg_qt_kit_glob "*/*msvc*")
    else()
        set(_icg_qt_kit_glob "*/*mingw*")
    endif()
    set(_icg_qt_hint "")
    set(_icg_qt_best "")
    foreach(_icg_qt_root "C:/Qt" "$ENV{QTDIR}")
        if(NOT _icg_qt_root OR NOT EXISTS "${_icg_qt_root}")
            continue()
        endif()
        file(GLOB _icg_qt_kits "${_icg_qt_root}/${_icg_qt_kit_glob}")
        foreach(_icg_qt_kit ${_icg_qt_kits})
            if(NOT IS_DIRECTORY "${_icg_qt_kit}")
                continue()
            endif()
            # <version> = parent dir name; skip non-versioned layouts.
            get_filename_component(_icg_qt_ver "${_icg_qt_kit}" DIRECTORY)
            get_filename_component(_icg_qt_ver "${_icg_qt_ver}" NAME)
            if(NOT _icg_qt_ver MATCHES "^6\\.[0-9]+\\.[0-9]+$")
                continue()
            endif()
            # Highest version wins, so an old cache entry can't pin us to 6.10.
            if(NOT _icg_qt_best OR _icg_qt_ver VERSION_GREATER _icg_qt_best)
                set(_icg_qt_best "${_icg_qt_ver}")
                set(_icg_qt_hint "${_icg_qt_kit}")
            endif()
        endforeach()
    endforeach()
    if(_icg_qt_hint)
        list(APPEND CMAKE_PREFIX_PATH "${_icg_qt_hint}")
        message(STATUS "Incogine Studio: Qt hint -> ${_icg_qt_hint}")
    endif()
endif()

set(ICG_STUDIO_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/core/project_paths.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/xml/project_xml.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/xml/credits_xml.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/xml/xml_util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/scene/scene_discovery.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/scene/scene_cpp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/scene/scene_cpp_text.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/scene/scene_cpp_fonts.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/scene/scene_cpp_ops.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/preview/preview_client.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/preview/preview_exe.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/anim_channel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/anim_channel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/crypto/sha256.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/incoba/incoba.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/incoba/incoba_index.cpp
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
# Network is required for the Animator's single-instance local socket
# (QLocalServer/QLocalSocket); it comes in with Widgets' deps but is requested
# explicitly so a minimal Qt install fails loudly instead of at link time.
find_package(Qt6 QUIET COMPONENTS Widgets Multimedia Network)
if(Qt6Widgets_FOUND)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)

    # ---- Windows: stage the Qt runtime next to the tools ----
    # The game target copies SDL3's DLLs post-build (see the top-level
    # CMakeLists.txt) so it runs from the build dir without any PATH setup. The
    # Qt tools need the same treatment or they fail to start with
    # "Qt6Widgetsd.dll was not found" — and even with the DLLs present, the
    # qwindows platform plugin is mandatory, so only copying DLLs by hand would
    # still fail. windeployqt resolves the full dependency set including plugins.
    function(icg_deploy_qt_runtime _target)
        if(NOT WIN32)
            return()
        endif()
        # Prefer Qt's own imported tool so no Qt path is hardcoded anywhere; fall
        # back to a lookup for older/odd Qt layouts that do not define it.
        set(_tool "")
        if(TARGET Qt6::windeployqt)
            set(_tool Qt6::windeployqt)
        else()
            find_program(_tool NAMES windeployqt qt_deploy_assistant)
        endif()
        if(NOT _tool)
            message(WARNING "Incogine Studio: windeployqt not found — ${_target} "
                            "may fail to start without Qt's bin on PATH")
            return()
        endif()
        add_custom_command(TARGET ${_target} POST_BUILD
            COMMAND ${_tool}
                --dir "$<TARGET_FILE_DIR:${_target}>"
                # Multi-config generators need to be told which flavor to stage.
                # A single IF expression, not two separate ones: a NOT-generator
                # that evaluates false still emits an empty argument, and
                # windeployqt rejects a bare "".
                "--$<IF:$<CONFIG:Debug>,debug,release>"
                # The UI ships English-only strings, so .qm catalogues would be
                # tens of megabytes nobody loads.
                --no-translations
                # Widgets only — no Qt Quick, so skip its import scan.
                --no-quick-import
                # windeployqt only deploys the platform plugin the binary
                # actually references (qwindows). The offscreen platform is
                # equally required, because both --self-test modes run headless
                # with QT_QPA_PLATFORM=offscreen — without it QApplication aborts
                # in its constructor and the diagnostic modes are unusable from a
                # staged build.
                --include-plugins qoffscreen
                # Stay consistent with how the game is staged (SDL DLLs only):
                # the MSVC/MinGW runtime is the developer's toolchain, and
                # copying a second copy into the build dir invites version skew.
                --no-compiler-runtime
                "$<TARGET_FILE:${_target}>"
            COMMENT "Deploying the Qt runtime next to ${_target}"
            VERBATIM
        )
    endfunction()

    set(ICG_STUDIO_UI_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/studio_main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/main_window.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/main_window.h
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/animator_launcher.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/animator_launcher.h
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/compile_runner.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/compile_runner.h
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/settings_dialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/settings_dialog.h
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/code_editor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/code_editor.h
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/search_panel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/search_panel.h
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/preview_viewport.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/preview_viewport.h
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/preview_session.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/preview_session.h
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/scene_editor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/scene_editor.h
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/font_preview.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/font_preview.h
    )
    # Incogine Animator — vector animation editor, its own top-level app.
    # Kept separate from IncogineStudio (rather than a child window) so closing
    # Studio does not close an animation the user is editing. Shares the engine
    # animation module, so the editor edits the same model the runtime bakes.
    set(ICG_ANIMATOR_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/animator/animator_main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/animator/animator_channel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/animator/animator_channel.h
        ${CMAKE_CURRENT_SOURCE_DIR}/animator/animator_canvas.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/animator/animator_canvas.h
        ${CMAKE_CURRENT_SOURCE_DIR}/animator/animator_document.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/animator/animator_document.h
        ${CMAKE_CURRENT_SOURCE_DIR}/animator/animator_tools.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/animator/animator_tools.h
        ${CMAKE_CURRENT_SOURCE_DIR}/animator/animator_window.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/animator/animator_window.h
    )
    add_executable(IncogineAnimator ${ICG_ANIMATOR_SOURCES})
    # IncogineStudioCore supplies the shared single-instance channel name, so
    # Studio and the Animator always agree on the socket to talk over.
    # Qt6::Network carries QLocalServer for that channel.
    target_link_libraries(IncogineAnimator PRIVATE
        IncogineStudioCore
        IncogineAnim
        Qt6::Widgets
        Qt6::Network
    )
    target_include_directories(IncogineAnimator PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/animator
    )
    if(MSVC)
        target_compile_options(IncogineAnimator PRIVATE /utf-8)
    endif()
    if(WIN32)
        set_target_properties(IncogineAnimator PROPERTIES WIN32_EXECUTABLE TRUE)
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../../bundle/windows/icon.ico"
                       "${CMAKE_CURRENT_BINARY_DIR}/animator_icon.ico" COPYONLY)
        file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/studio.rc"
             DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
        target_sources(IncogineAnimator PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/studio.rc")
    endif()
    source_group("Animator" FILES ${ICG_ANIMATOR_SOURCES})
    icg_deploy_qt_runtime(IncogineAnimator)
    message(STATUS "Incogine Studio: IncogineAnimator animation editor enabled")

    add_executable(IncogineStudio ${ICG_STUDIO_UI_SOURCES})
    target_link_libraries(IncogineStudio PRIVATE
        IncogineStudioCore
        # Engine-side animation module: the Animator window edits this same
        # model and rasterizer the runtime bakes with, so the editor preview
        # and the baked sprite sheet cannot drift apart. Qt-free and
        # SDL-free, so it links into either side of the build.
        IncogineAnim
        Qt6::Widgets
    )
    # Engine camera math is dependency-free; the editor links it directly
    # (the runtime must never depend on Studio, not the reverse).
    target_sources(IncogineStudio PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../core/render/camera.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../core/render/camera.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../core/render/camera_math.h
    )
    source_group("Studio\\EngineMath" FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../core/render/camera.cpp
    )
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
    icg_deploy_qt_runtime(IncogineStudio)
    message(STATUS "Incogine Studio: Qt6 Widgets found — IncogineStudio target enabled")
else()
    message(STATUS "Incogine Studio: Qt6 Widgets not found — IDE shell skipped (Core + IncogineIncoba still build)")
endif()
