# Integration tests (docs/design/08_testing.md 6.1, ARCHITECTURE.md 11 "Wave 3 exit" and "Wave 4 exit"): the
# cross-module scenarios, one file per scenario, one or two cases per file. Everything is headless: the null
# window, the null rhi device, the null audio backend, the null script host unless a [scripting] case starts
# the real CLR, memory file systems and temp directories.
#
# Wave 3 (no engine module needed):
#   test_assets_renderer_headless.cpp   a generated glTF through assets into the renderer on the null device
#   test_scripting_scene.cpp            null_script_host + scene: scripts survive save / load and prefabs (R29)
#   test_assets_scene.cpp               a .scene.json imported through the database binds asset_ref fields (R25)
#
# Wave 4 (engine::application through conjure::testing::headless_engine, ENGINE):
#   test_headless_frames.cpp            600 frames: fixed updates, rhi submissions, no ERROR
#   test_scene_roundtrip.cpp            tests/assets/scenes/roundtrip.scene.json byte-identical; hash after load
#   test_hierarchy_propagation.cpp      depth-8 chain, 10,000-entity frame allocation-free
#   test_physics_stack.cpp              ten boxes settle and sleep
#   test_prefab_spawn_destroy.cpp       tests/assets/prefabs/crate.prefab.json spawned and destroyed per frame
#   test_scene_switch.cpp               additive load, unload, reload
#   test_determinism.cpp                hash sequences at 0, 2 and 8 workers (R41)
#   test_script_lifecycle.cpp           [scripting] LifecycleProbe over the real CLR
#   test_hot_reload.cpp                 [scripting] variant B reload: fields kept, removed class disabled
#   test_export_roundtrip.cpp           tests/assets/projects/minimal exported, conjure_runtime boots it (R21)
#   test_ai_protocol_e2e.cpp            the Grid Sweeper command file through editor::headless_runner (R15)
#   test_engine_physics_interpolation.cpp, test_engine_project_boot.cpp, test_engine_render_failure.cpp
#                                       the engine scenarios of 06_engine_loop.md 10
#
# conjure_integration_plugin_tests (LINK_ENGINE_SHARED) holds test_module_plugin.cpp alone: it loads the
# fixture plugin fixtures/sample_plugin, which links conjure_shared, so the host must share that one engine copy
# (07_modules.md D1). The static executable keeps meaningful memory_guard and allocation_scope checks for the
# other scenarios, which a LINK_ENGINE_SHARED executable cannot count (cmake/ConjureTesting.cmake).

if(NOT TARGET conjure::scene OR NOT TARGET conjure::renderer)
    message(STATUS "integration: scene and renderer are required, integration tests skipped")
    return()
endif()

#------------------------------------------------------------------------------
# Wave 3 scenarios and the engine scenarios in one executable
#------------------------------------------------------------------------------
# Only the scripting scenarios need conjure::scripting. The sanitizer presets configure with
# CONJURE_WITH_SCRIPTING=OFF (08_testing.md D13) and must still run the other scenarios under label integration.
set(integrationSources
    integration_test_support.cpp
    integration_test_support.h
    test_assets_renderer_headless.cpp
    test_assets_scene.cpp
)
set(integrationDeps
    conjure::assets
    conjure::ecs
    conjure::renderer
    conjure::rhi
    conjure::scene
)
set(integrationOptions "")
if(TARGET conjure::scripting)
    list(APPEND integrationSources test_scripting_scene.cpp)
    list(APPEND integrationDeps conjure::scripting)
else()
    message(STATUS "integration: conjure::scripting is not available, the scripting scenarios are omitted")
endif()

set(integrationHasEngine FALSE)
if(TARGET conjure::test_support_engine)
    set(integrationHasEngine TRUE)
    set(integrationOptions ENGINE)
    list(APPEND integrationSources
        integration_engine_support.cpp
        integration_engine_support.h
        test_determinism.cpp
        test_engine_project_boot.cpp
        test_engine_render_failure.cpp
        test_headless_frames.cpp
        test_hierarchy_propagation.cpp
        test_prefab_spawn_destroy.cpp
        test_scene_roundtrip.cpp
        test_scene_switch.cpp
    )
    list(APPEND integrationDeps conjure::engine conjure::modules conjure::platform)
    if(TARGET conjure::physics)
        list(APPEND integrationSources test_engine_physics_interpolation.cpp test_physics_stack.cpp)
        list(APPEND integrationDeps conjure::physics)
    endif()
    if(TARGET conjure::scripting)
        list(APPEND integrationSources test_hot_reload.cpp test_script_lifecycle.cpp)
    endif()
    if(TARGET conjure_runtime)
        list(APPEND integrationSources test_export_roundtrip.cpp)
    endif()
    if(TARGET conjure::editor_core)
        list(APPEND integrationSources test_ai_protocol_e2e.cpp)
        list(APPEND integrationDeps conjure::editor_core)
    endif()
else()
    message(STATUS "integration: conjure::test_support_engine is not available, the engine scenarios are omitted")
endif()

conjure_add_test_module(integration
    ${integrationOptions}
    SOURCES
        ${integrationSources}
    DEPS
        ${integrationDeps}
    LABELS integration
    TIMEOUT 600
)

# The renderer reads the index conjure_shaders built (the same arrangement as tests/renderer).
add_dependencies(conjure_integration_tests conjure_shaders)
get_target_property(integrationShaderDir conjure_shaders CONJURE_SHADER_OUTPUT_DIR)
target_compile_definitions(conjure_integration_tests PRIVATE
    CONJURE_TEST_SHADER_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${integrationShaderDir}>"
)

if(integrationHasEngine)
    # The export round trip spawns the built runtime and copies it from its bin directory.
    if(TARGET conjure_runtime)
        add_dependencies(conjure_integration_tests conjure_runtime)
        target_compile_definitions(conjure_integration_tests PRIVATE
            CONJURE_INTEGRATION_RUNTIME_EXECUTABLE="$<PATH:CMAKE_PATH,NORMALIZE,$<TARGET_FILE:conjure_runtime>>"
            CONJURE_INTEGRATION_RUNTIME_BIN_DIR="$<PATH:CMAKE_PATH,NORMALIZE,$<TARGET_FILE_DIR:conjure_runtime>>"
        )
    endif()
    # The [scripting] scenarios load the v1 and v2 builds of Conjure.TestScripts.
    if(TARGET conjure_managed_test_scripts_v2 AND CONJURE_MANAGED_TEST_SCRIPTS_DIR)
        add_dependencies(conjure_integration_tests conjure_managed_test_scripts_v2)
        target_compile_definitions(conjure_integration_tests PRIVATE
            CONJURE_INTEGRATION_TEST_SCRIPTS_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${CONJURE_MANAGED_TEST_SCRIPTS_DIR}>"
        )
    endif()
    # The AI protocol scenario runs the editor owner's Grid Sweeper command file (08_testing.md 6.1).
    target_compile_definitions(conjure_integration_tests PRIVATE
        CONJURE_INTEGRATION_EDITOR_COMMAND_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${PROJECT_SOURCE_DIR}/tests/editor/data/commands>"
    )
endif()

#------------------------------------------------------------------------------
# The plugin scenario: the fixture module and its LINK_ENGINE_SHARED host
#------------------------------------------------------------------------------
if(integrationHasEngine AND TARGET conjure::shared AND TARGET conjure::test_support_engine_shared)
    set(CONJURE_INTEGRATION_MODULE_DIR "${PROJECT_BINARY_DIR}/test_modules_integration")
    set(pluginSourceDir "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sample_plugin")
    configure_file(
        "${pluginSourceDir}/module.json"
        "${CONJURE_INTEGRATION_MODULE_DIR}/sample_plugin/module.json"
        COPYONLY
    )
    conjure_add_game_module(conjure_integration_sample_plugin
        MANIFEST "${pluginSourceDir}/module.json"
        OUTPUT_ROOT "${CONJURE_INTEGRATION_MODULE_DIR}/sample_plugin"
        SOURCES "${pluginSourceDir}/sample_plugin.cpp"
    )
    set_target_properties(conjure_integration_sample_plugin PROPERTIES FOLDER "Tests/ModuleFixtures")

    conjure_add_test_module(integration_plugin
        ENGINE
        LINK_ENGINE_SHARED
        SOURCES
            integration_engine_support.cpp
            integration_engine_support.h
            test_module_plugin.cpp
        LABELS integration
        TIMEOUT 300
    )
    add_dependencies(conjure_integration_plugin_tests conjure_integration_sample_plugin)
    target_compile_definitions(conjure_integration_plugin_tests PRIVATE
        CONJURE_INTEGRATION_MODULE_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${CONJURE_INTEGRATION_MODULE_DIR}>"
    )
endif()
