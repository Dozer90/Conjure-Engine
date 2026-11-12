# conjure_editor (docs/design/04_editor.md 1.1, 3.1, 8; docs/design/07_modules.md 5): the one executable of
# the editor. main.cpp parses the editor command line, builds an editor_application over an engine::application
# and runs it: the ImGui workspace with a window, or headless with --exec / --ai-port / --ai-stdio.
#
# The two editor libraries (conjure_editor_core, conjure_editor_ui) live in engine/src/editor and are declared
# through engine/CMakeLists.txt, so they are whole-archived into conjure_shared with every other module and the
# executable links conjure::shared and nothing else from the engine (R17): the one ImGui context, the renderer's
# ImGui backend and the editor workspace all live in the umbrella. The directory is reached only when
# CONJURE_BUILD_EDITOR is on (the root CMakeLists.txt forces it off in Distribution).
#
# The editor keeps the console subsystem in every configuration: --exec, --ai-stdio and the log write to the
# standard streams, and WIN32_GUI would only apply in Distribution, where the editor is never built.

if(NOT CONJURE_BUILD_EDITOR)
    return()
endif()

if(NOT TARGET conjure::editor_core OR NOT TARGET conjure::editor_ui OR NOT TARGET conjure::shared)
    message(STATUS "editor: the editor libraries are not available, conjure_editor skipped")
    return()
endif()

# The platform backend of editor_arguments.h is selected here rather than with preprocessor guards, as the runtime
# does. On Windows the backend needs shell32 (CommandLineToArgvW), and the manifest makes UTF-8 the process code
# page so the engine's narrow-string paths decode the UTF-8 arguments.
set(CONJURE_EDITOR_SOURCES
    src/main.cpp
    src/editor_arguments.h
)
set(CONJURE_EDITOR_PLATFORM_LIBRARIES "")
if(WIN32)
    list(APPEND CONJURE_EDITOR_SOURCES
        src/editor_arguments_windows.cpp
        src/conjure_editor.manifest
    )
    list(APPEND CONJURE_EDITOR_PLATFORM_LIBRARIES shell32)
else()
    list(APPEND CONJURE_EDITOR_SOURCES src/editor_arguments_posix.cpp)
endif()

conjure_add_executable(conjure_editor
    SOURCES
        ${CONJURE_EDITOR_SOURCES}
    DEPS
        conjure::shared
        ${CONJURE_EDITOR_PLATFORM_LIBRARIES}
)

# libconjure_shared sits next to the executable in bin/, so the run path is the executable's own directory and a
# moved bin/ keeps working (BUILD_RPATH adds to the build-tree entries CMake generates).
set_target_properties(conjure_editor PROPERTIES
    OUTPUT_NAME "conjure_editor"
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
    BUILD_RPATH_USE_ORIGIN TRUE
)
if(APPLE)
    set_target_properties(conjure_editor PROPERTIES
        BUILD_RPATH "@loader_path"
        INSTALL_RPATH "@loader_path"
    )
endif()

# The application icon (docs/handbook/brand.md): on Windows a resource script compiles the generated .ico into the
# executable. The resource compiler looks for the .ico on its include path rather than next to the script, so the
# resources directory is added for this one source, and the .ico is a dependency so a regenerated icon relinks.
# Other platforms get the icon at run time from editor_ui (window::set_icon).
if(WIN32)
    set(editorResourceDirectory "${CMAKE_CURRENT_SOURCE_DIR}/resources")
    set(editorResourceScript "${editorResourceDirectory}/conjure_editor.rc")
    set_property(SOURCE "${editorResourceScript}" PROPERTY INCLUDE_DIRECTORIES "${editorResourceDirectory}")
    set_property(SOURCE "${editorResourceScript}" APPEND PROPERTY
        OBJECT_DEPENDS "${editorResourceDirectory}/conjure_editor.ico"
    )
    target_sources(conjure_editor PRIVATE "${editorResourceScript}")
endif()
