# conjure_editor (docs/design/04_editor.md 1.1, 3.1, 8; docs/design/07_modules.md 5): the one executable of
# the editor. main.cpp parses the editor command line, builds an editor_application over an engine::application
# and runs it: the ImGui workspace with a window, or headless with --exec / --ai-port / --ai-stdio.
#
# The two editor libraries (conjure_editor_core, conjure_editor_ui) live in engine/src/editor and are declared
# through engine/CMakeLists.txt, so they are whole-archived into conjure_shared with every other module and the
# executable links conjure::shared and nothing else from the engine (R17): the one ImGui context, the renderer's
# ImGui backend and the editor workspace all live in the umbrella. The directory is reached only when
# CONJURE_BUILD_EDITOR is on (the root CMakeLists.txt forces it off in Distribution).

if(NOT CONJURE_BUILD_EDITOR)
    return()
endif()

if(NOT TARGET conjure::editor_core OR NOT TARGET conjure::editor_ui OR NOT TARGET conjure::shared)
    message(STATUS "editor: the editor libraries are not available, conjure_editor skipped")
    return()
endif()

conjure_add_executable(conjure_editor
    WIN32_GUI
    SOURCES
        src/main.cpp
    DEPS
        conjure::shared
)

set_target_properties(conjure_editor PROPERTIES OUTPUT_NAME "conjure_editor")

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
