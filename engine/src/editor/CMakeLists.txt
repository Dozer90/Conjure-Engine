# conjure_editor_core and conjure_editor_ui (docs/design/04_editor.md 1.1, D1, D13). Both are ordinary
# conjure_add_module() static libraries. editor_core holds everything the AI protocol and the headless mode
# need (commands, undo, protocol, dispatcher, play, picking, screenshots, headless runner, the editor
# application) and never includes ImGui or sockets in a public header; editor_ui holds the Dear ImGui
# workspace, the panels, the property drawers and the ImGuizmo gizmos. Nothing here is built in
# Distribution: the root CMakeLists.txt forces CONJURE_BUILD_EDITOR off there and editor/CMakeLists.txt
# only reaches this directory when the option is on.

if(NOT CONJURE_BUILD_EDITOR)
    return()
endif()
if(NOT TARGET conjure::engine)
    message(STATUS "editor: conjure::engine is not available, editor libraries skipped")
    return()
endif()

set(CONJURE_EDITOR_INCLUDE "${PROJECT_SOURCE_DIR}/engine/include/conjure/editor")

#------------------------------------------------------------------------------
# conjure_editor_core
#------------------------------------------------------------------------------
set(CONJURE_EDITOR_CORE_PUBLIC_HEADERS
    ${CONJURE_EDITOR_INCLUDE}/builtin_commands.h
    ${CONJURE_EDITOR_INCLUDE}/command_desc.h
    ${CONJURE_EDITOR_INCLUDE}/command_dispatcher.h
    ${CONJURE_EDITOR_INCLUDE}/command_registry.h
    ${CONJURE_EDITOR_INCLUDE}/console_buffer.h
    ${CONJURE_EDITOR_INCLUDE}/editor_application.h
    ${CONJURE_EDITOR_INCLUDE}/editor_camera.h
    ${CONJURE_EDITOR_INCLUDE}/editor_command.h
    ${CONJURE_EDITOR_INCLUDE}/editor_context.h
    ${CONJURE_EDITOR_INCLUDE}/editor_error.h
    ${CONJURE_EDITOR_INCLUDE}/editor_module_consumer.h
    ${CONJURE_EDITOR_INCLUDE}/editor_options.h
    ${CONJURE_EDITOR_INCLUDE}/entity_ref.h
    ${CONJURE_EDITOR_INCLUDE}/event_broadcaster.h
    ${CONJURE_EDITOR_INCLUDE}/headless_runner.h
    ${CONJURE_EDITOR_INCLUDE}/log_category.h
    ${CONJURE_EDITOR_INCLUDE}/panel_desc.h
    ${CONJURE_EDITOR_INCLUDE}/picker.h
    ${CONJURE_EDITOR_INCLUDE}/play_controller.h
    ${CONJURE_EDITOR_INCLUDE}/reflection_path.h
    ${CONJURE_EDITOR_INCLUDE}/schema_validator.h
    ${CONJURE_EDITOR_INCLUDE}/screenshot_service.h
    ${CONJURE_EDITOR_INCLUDE}/selection_set.h
    ${CONJURE_EDITOR_INCLUDE}/undo_stack.h
    ${CONJURE_EDITOR_INCLUDE}/protocol/line_splitter.h
    ${CONJURE_EDITOR_INCLUDE}/protocol/loopback_transport.h
    ${CONJURE_EDITOR_INCLUDE}/protocol/protocol_message.h
    ${CONJURE_EDITOR_INCLUDE}/protocol/protocol_server.h
    ${CONJURE_EDITOR_INCLUDE}/protocol/protocol_transport.h
)

set(CONJURE_EDITOR_CORE_SOURCES
    core/builtin_commands.cpp
    core/command_dispatcher.cpp
    core/command_registry.cpp
    core/component_access.cpp
    core/component_access.h
    core/console_buffer.cpp
    core/editor_application.cpp
    core/editor_camera.cpp
    core/editor_command.cpp
    core/editor_context.cpp
    core/editor_error.cpp
    core/editor_module_consumer.cpp
    core/editor_options.cpp
    core/entity_ref.cpp
    core/event_broadcaster.cpp
    core/headless_runner.cpp
    core/log_category.cpp
    core/picker.cpp
    core/play_controller.cpp
    core/reflection_path.cpp
    core/schema_validator.cpp
    core/screenshot_service.cpp
    core/selection_set.cpp
    core/undo_stack.cpp
    core/commands/command_support.cpp
    core/commands/command_support.h
    core/commands/commands_asset.cpp
    core/commands/commands_component.cpp
    core/commands/commands_console.cpp
    core/commands/commands_editor.cpp
    core/commands/commands_entity.cpp
    core/commands/commands_history.cpp
    core/commands/commands_input.cpp
    core/commands/commands_modules.cpp
    core/commands/commands_physics.cpp
    core/commands/commands_play.cpp
    core/commands/commands_prefab.cpp
    core/commands/commands_profiler.cpp
    core/commands/commands_project.cpp
    core/commands/commands_reflection.cpp
    core/commands/commands_scene.cpp
    core/commands/commands_script.cpp
    core/commands/commands_selection.cpp
    core/commands/commands_session.cpp
    core/commands/commands_view.cpp
    core/net/socket.h
    core/protocol/line_splitter.cpp
    core/protocol/loopback_transport.cpp
    core/protocol/protocol_message.cpp
    core/protocol/protocol_server.cpp
    core/protocol/protocol_transport.cpp
    core/protocol/stdio_transport.cpp
    core/protocol/stdio_transport.h
    core/protocol/tcp_transport.cpp
    core/protocol/tcp_transport.h
)

# The socket layer is the one platform-specific file pair (04_editor.md 3.12): selected here, no #ifdef
# anywhere else in the module.
if(WIN32)
    list(APPEND CONJURE_EDITOR_CORE_SOURCES core/net/socket_win32.cpp)
else()
    list(APPEND CONJURE_EDITOR_CORE_SOURCES core/net/socket_posix.cpp)
endif()

set(CONJURE_EDITOR_CORE_PRIVATE_DEPS "")
if(CONJURE_WITH_RENDERER AND TARGET conjure::renderer)
    list(APPEND CONJURE_EDITOR_CORE_PRIVATE_DEPS conjure::renderer conjure::rhi)
endif()
if(CONJURE_WITH_PHYSICS AND TARGET conjure::physics)
    list(APPEND CONJURE_EDITOR_CORE_PRIVATE_DEPS conjure::physics)
endif()
if(CONJURE_WITH_SCRIPTING AND TARGET conjure::scripting)
    list(APPEND CONJURE_EDITOR_CORE_PRIVATE_DEPS conjure::scripting)
endif()
# The bindings module precedes the editor in CONJURE_ENGINE_MODULES: when it exists, editor_application adds
# bindings::scripting_bindings_subsystem before initialise() (04_editor.md 3.1, startup step 16), the way
# runtime/CMakeLists.txt does with CONJURE_RUNTIME_WITH_BINDINGS.
set(CONJURE_EDITOR_CORE_PRIVATE_DEFINES "")
if(TARGET conjure::bindings)
    list(APPEND CONJURE_EDITOR_CORE_PRIVATE_DEPS conjure::bindings)
    list(APPEND CONJURE_EDITOR_CORE_PRIVATE_DEFINES CONJURE_EDITOR_WITH_BINDINGS=1)
endif()
if(WIN32)
    list(APPEND CONJURE_EDITOR_CORE_PRIVATE_DEPS ws2_32)
endif()

conjure_add_module(editor_core
    PUBLIC_HEADERS
        ${CONJURE_EDITOR_CORE_PUBLIC_HEADERS}
    SOURCES
        ${CONJURE_EDITOR_CORE_SOURCES}
    PUBLIC_DEPS
        conjure::core
        conjure::ecs
        conjure::platform
        conjure::assets
        conjure::scene
        conjure::modules
        conjure::engine
    PRIVATE_DEPS
        ${CONJURE_EDITOR_CORE_PRIVATE_DEPS}
    PRIVATE_DEFINES
        ${CONJURE_EDITOR_CORE_PRIVATE_DEFINES}
)

#------------------------------------------------------------------------------
# conjure_editor_ui
#------------------------------------------------------------------------------
if(NOT TARGET conjure::thirdparty::imgui)
    message(STATUS "editor: Dear ImGui is not available, conjure_editor_ui skipped")
    return()
endif()

set(CONJURE_EDITOR_UI_PUBLIC_HEADERS
    ${CONJURE_EDITOR_INCLUDE}/ui/editor_icon.h
    ${CONJURE_EDITOR_INCLUDE}/ui/editor_layer.h
    ${CONJURE_EDITOR_INCLUDE}/ui/editor_panel.h
    ${CONJURE_EDITOR_INCLUDE}/ui/editor_workspace.h
    ${CONJURE_EDITOR_INCLUDE}/ui/gizmo_controller.h
    ${CONJURE_EDITOR_INCLUDE}/ui/hierarchy_cache.h
    ${CONJURE_EDITOR_INCLUDE}/ui/imgui_frame_scope.h
    ${CONJURE_EDITOR_INCLUDE}/ui/property_drawer.h
    ${CONJURE_EDITOR_INCLUDE}/ui/panels/ai_session_panel.h
    ${CONJURE_EDITOR_INCLUDE}/ui/panels/asset_browser_panel.h
    ${CONJURE_EDITOR_INCLUDE}/ui/panels/console_panel.h
    ${CONJURE_EDITOR_INCLUDE}/ui/panels/export_dialog.h
    ${CONJURE_EDITOR_INCLUDE}/ui/panels/hierarchy_panel.h
    ${CONJURE_EDITOR_INCLUDE}/ui/panels/history_panel.h
    ${CONJURE_EDITOR_INCLUDE}/ui/panels/inspector_panel.h
    ${CONJURE_EDITOR_INCLUDE}/ui/panels/profiler_panel.h
    ${CONJURE_EDITOR_INCLUDE}/ui/panels/toolbar_panel.h
    ${CONJURE_EDITOR_INCLUDE}/ui/panels/viewport_panel.h
)

set(CONJURE_EDITOR_UI_SOURCES
    ui/editor_icon.cpp
    ui/editor_icon_data.cpp     # generated by tools/generate_brand_icons.py
    ui/editor_icon_data.h
    ui/editor_layer.cpp
    ui/editor_panel.cpp
    ui/editor_workspace.cpp
    ui/gizmo_controller.cpp
    ui/hierarchy_cache.cpp
    ui/imgui_frame_scope.cpp
    ui/property_drawer.cpp
    ui/ui_support.cpp
    ui/ui_support.h
    ui/panels/ai_session_panel.cpp
    ui/panels/asset_browser_panel.cpp
    ui/panels/console_panel.cpp
    ui/panels/export_dialog.cpp
    ui/panels/hierarchy_panel.cpp
    ui/panels/history_panel.cpp
    ui/panels/inspector_panel.cpp
    ui/panels/profiler_panel.cpp
    ui/panels/toolbar_panel.cpp
    ui/panels/viewport_panel.cpp
)

set(CONJURE_EDITOR_UI_PRIVATE_DEPS conjure::thirdparty::imgui)
if(CONJURE_WITH_RENDERER AND TARGET conjure::renderer)
    list(APPEND CONJURE_EDITOR_UI_PRIVATE_DEPS conjure::renderer)
endif()

conjure_add_module(editor_ui
    PUBLIC_HEADERS
        ${CONJURE_EDITOR_UI_PUBLIC_HEADERS}
    SOURCES
        ${CONJURE_EDITOR_UI_SOURCES}
    PUBLIC_DEPS
        conjure::editor_core
    PRIVATE_DEPS
        ${CONJURE_EDITOR_UI_PRIVATE_DEPS}
)
