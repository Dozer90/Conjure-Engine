# editor tests (docs/design/04_editor.md 10, 08_testing.md 3.7 and 6.3): one test_<header>.cpp per public
# header of conjure/editor (and conjure/editor/ui) plus the topic files the design names: the command
# catalogue fixtures under data/commands/, the batch command, the per-area command files, the protocol
# robustness and golden cases, the loopback end-to-end session, the headless runner and the Grid Sweeper
# command file. Everything runs headless over the null platform, rhi and audio backends; the UI cases build
# an ImGui frame without a renderer backend (imgui_frame_scope). test_editor_process (label process) spawns
# the real conjure_editor executable and the [slow] cases (label slow) take seconds each; both are excluded
# from the default test presets.

if(NOT TARGET conjure::editor_core)
    return()
endif()

set(CONJURE_EDITOR_TEST_SOURCES
    editor_test_support.cpp
    editor_test_support.h
    test_ai_builds_grid_sweeper.cpp
    test_ai_protocol_e2e.cpp
    test_batch_command.cpp
    test_builtin_commands.cpp
    test_command_catalogue.cpp
    test_command_desc.cpp
    test_command_dispatcher.cpp
    test_command_registry.cpp
    test_command_support.cpp
    test_commands_asset_prefab.cpp
    test_commands_component.cpp
    test_commands_entity.cpp
    test_commands_input.cpp
    test_commands_project_trust.cpp
    test_commands_scene_project.cpp
    test_commands_script.cpp
    test_console_buffer.cpp
    test_editor_application.cpp
    test_editor_camera.cpp
    test_editor_command.cpp
    test_editor_context.cpp
    test_editor_error.cpp
    test_editor_module_consumer.cpp
    test_editor_options.cpp
    test_editor_process.cpp
    test_entity_ref.cpp
    test_event_broadcaster.cpp
    test_headless_runner.cpp
    test_log_category.cpp
    test_panel_desc.cpp
    test_picker.cpp
    test_play_controller.cpp
    test_project_trust.cpp
    test_protocol_robustness.cpp
    test_reflection_commands.cpp
    test_reflection_path.cpp
    test_scene_view.cpp
    test_schema_validator.cpp
    test_screenshot_service.cpp
    test_selection_set.cpp
    test_undo_stack.cpp
    protocol/test_access_token.cpp
    protocol/test_audit_log.cpp
    protocol/test_line_splitter.cpp
    protocol/test_loopback_transport.cpp
    protocol/test_protocol_message.cpp
    protocol/test_protocol_server.cpp
    protocol/test_protocol_transport.cpp
    protocol/test_stdio_transport.cpp
    protocol/test_tcp_transport.cpp
)

set(CONJURE_EDITOR_TEST_DEPS
    conjure::editor_core
    conjure::engine
    conjure::scene
    conjure::ecs
    conjure::assets
    conjure::platform
    conjure::modules
)
if(TARGET conjure::renderer)
    list(APPEND CONJURE_EDITOR_TEST_DEPS conjure::renderer conjure::rhi)
endif()
if(TARGET conjure::physics)
    list(APPEND CONJURE_EDITOR_TEST_DEPS conjure::physics)
endif()
if(TARGET conjure::scripting)
    list(APPEND CONJURE_EDITOR_TEST_DEPS conjure::scripting)
endif()
if(TARGET conjure::bindings)
    list(APPEND CONJURE_EDITOR_TEST_DEPS conjure::bindings)
endif()
if(TARGET conjure::editor_ui)
    list(APPEND CONJURE_EDITOR_TEST_SOURCES
        ui/test_editor_icon.cpp
        ui/test_editor_layer.cpp
        ui/test_editor_panel.cpp
        ui/test_editor_workspace.cpp
        ui/test_gizmo_controller.cpp
        ui/test_hierarchy_cache.cpp
        ui/test_imgui_frame_scope.cpp
        ui/test_property_drawer.cpp
        ui/panels/test_ai_session_panel.cpp
        ui/panels/test_asset_browser_panel.cpp
        ui/panels/test_console_panel.cpp
        ui/panels/test_export_dialog.cpp
        ui/panels/test_hierarchy_panel.cpp
        ui/panels/test_history_panel.cpp
        ui/panels/test_inspector_panel.cpp
        ui/panels/test_profiler_panel.cpp
        ui/panels/test_toolbar_panel.cpp
        ui/panels/test_viewport_panel.cpp
    )
    list(APPEND CONJURE_EDITOR_TEST_DEPS conjure::editor_ui)
endif()

conjure_add_test_module(editor
    SOURCES
        ${CONJURE_EDITOR_TEST_SOURCES}
    DEPS
        ${CONJURE_EDITOR_TEST_DEPS}
    TIMEOUT 300
)

# Private module headers (the socket layer the tcp tests connect with) are reached through engine/src; the
# shared fixture header is reached from the protocol/ and ui/ subdirectories through this directory.
target_include_directories(conjure_editor_tests PRIVATE "${PROJECT_SOURCE_DIR}/engine/src" "${CMAKE_CURRENT_SOURCE_DIR}")
if(TARGET conjure::editor_ui)
    # The UI cases drive ImGui directly (frames, windows, IO) around the panels.
    target_link_libraries(conjure_editor_tests PRIVATE conjure::thirdparty::imgui)
endif()

# Fixtures, the generated protocol document and the real executable for the process test. The C# 14 probe of the
# test scripts is also compiled into a project script.create made, against the language version its Game.csproj pins.
set(editorDataDirectory "${CMAKE_CURRENT_SOURCE_DIR}/data")
set(editorProtocolDocument "${PROJECT_SOURCE_DIR}/docs/generated/editor_protocol.json")
set(editorLanguageProbe "${PROJECT_SOURCE_DIR}/managed/Conjure.TestScripts/LanguageVersionProbe.cs")
target_compile_definitions(conjure_editor_tests PRIVATE
    CONJURE_EDITOR_TEST_DATA_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${editorDataDirectory}>"
    CONJURE_EDITOR_PROTOCOL_DOCUMENT="$<PATH:CMAKE_PATH,NORMALIZE,${editorProtocolDocument}>"
    CONJURE_EDITOR_TEST_LANGUAGE_PROBE="$<PATH:CMAKE_PATH,NORMALIZE,${editorLanguageProbe}>"
)
if(TARGET conjure_editor)
    target_compile_definitions(conjure_editor_tests PRIVATE
        CONJURE_EDITOR_EXECUTABLE="$<TARGET_FILE:conjure_editor>"
    )
    add_dependencies(conjure_editor_tests conjure_editor)
endif()

# The bindings cases of test_editor_application (the subsystem the editor adds at startup step 16) compile only
# with the module; the [dotnet] case loads the managed SDK and Conjure.TestScripts the same way tests/bindings do.
if(TARGET conjure::bindings)
    target_compile_definitions(conjure_editor_tests PRIVATE CONJURE_EDITOR_TEST_HAS_BINDINGS=1)
endif()
if(TARGET conjure_managed_sdk AND TARGET conjure_managed_test_scripts_v1)
    add_dependencies(conjure_editor_tests conjure_managed_sdk conjure_managed_test_scripts_v1)
    target_compile_definitions(conjure_editor_tests PRIVATE
        CONJURE_EDITOR_TEST_HAS_DOTNET=1
        CONJURE_EDITOR_TEST_MANAGED_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${CONJURE_MANAGED_SDK_DIR}>"
        CONJURE_EDITOR_TEST_SCRIPTS_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${CONJURE_MANAGED_TEST_SCRIPTS_DIR}>"
    )
    # The [dotnet][slow] project.export case (follow-up FU-PDB) exports tests/assets/projects/scripted with the runtime
    # of this tree and runs the exported runtime.
    if(TARGET conjure_runtime)
        add_dependencies(conjure_editor_tests conjure_runtime)
        set(editorScriptedProject "${PROJECT_SOURCE_DIR}/tests/assets/projects/scripted")
        target_compile_definitions(conjure_editor_tests PRIVATE
            CONJURE_EDITOR_TEST_RUNTIME_EXECUTABLE="$<TARGET_FILE:conjure_runtime>"
            CONJURE_EDITOR_TEST_SCRIPTED_PROJECT="$<PATH:CMAKE_PATH,NORMALIZE,${editorScriptedProject}>"
        )
    endif()
else()
    target_compile_definitions(conjure_editor_tests PRIVATE CONJURE_EDITOR_TEST_HAS_DOTNET=0)
endif()
if(TARGET conjure_shaders)
    add_dependencies(conjure_editor_tests conjure_shaders)
    get_target_property(editorShaderDir conjure_shaders CONJURE_SHADER_OUTPUT_DIR)
    target_compile_definitions(conjure_editor_tests PRIVATE
        CONJURE_TEST_SHADER_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${editorShaderDir}>"
    )
endif()

# The process cases spawn conjure_editor and the [slow] cases take seconds each (docs/handbook/testing.md):
# label them so the default presets (which exclude "process" and "slow") skip them; CI's label step runs the
# process cases and the nightly presets run both. doctest's discovery already registered every case; the side
# file adds the label to the cases whose name carries the [process] or [slow] tag. A [dotnet][slow] case runs a real
# dotnet publish, minutes on a cold machine: it gets 1860 s instead of the module's 300, a minute more than its doctest
# timeout, so that a hung command fails the case naming it before CTest kills the process.
set(tagLabelsFile "${CMAKE_CURRENT_BINARY_DIR}/conjure_editor_tests_tag_labels.cmake")
file(WRITE "${tagLabelsFile}"
    "if(DEFINED conjure_editor_tests_TESTS)\n"
    "    foreach(entry IN LISTS conjure_editor_tests_TESTS)\n"
    "        if(entry MATCHES \"\\\\[dotnet\\\\]\\\\[slow\\\\]\")\n"
    "            set_tests_properties(\"\${entry}\" PROPERTIES LABELS \"unit;editor;dotnet;slow\" TIMEOUT 1860)\n"
    "        elseif(entry MATCHES \"\\\\[process\\\\]\")\n"
    "            set_tests_properties(\"\${entry}\" PROPERTIES LABELS \"unit;editor;process\")\n"
    "        elseif(entry MATCHES \"\\\\[slow\\\\]\")\n"
    "            set_tests_properties(\"\${entry}\" PROPERTIES LABELS \"unit;editor;slow\")\n"
    "        endif()\n"
    "    endforeach()\n"
    "endif()\n"
)
set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES "${tagLabelsFile}")

# Every public header of both libraries must compile on its own (build-only check, part of ALL).
conjure_header_check(conjure_editor_core)
if(TARGET conjure::editor_ui)
    conjure_header_check(conjure_editor_ui)
endif()
