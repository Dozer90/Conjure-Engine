# Integration tests (docs/design/08_testing.md 6.1, ARCHITECTURE.md 11 "Wave 3 exit"): the cross-module
# scenarios that need no engine module. One file per scenario, one or two cases per file. Everything is
# headless: the null RHI device, the null script host, memory file systems and inline job systems.
#
#   test_assets_renderer_headless.cpp   a generated glTF through assets (database, importer, cache,
#                                       manager) into the renderer on the null device, one frame rendered
#   test_scripting_scene.cpp            null_script_host + scene: scripts survive scene save / load and
#                                       prefab instantiation through the script_component serializer (R29)
#   test_assets_scene.cpp               a .scene.json imported through the database (R45) and loaded by the
#                                       scene_manager binds its asset_ref fields through bind_slot (R25)
#
# The scenarios of 6.1 that need engine::application (headless frames, physics stack, determinism, export
# round trip, the AI protocol) arrive with wave 4.

if(NOT TARGET conjure::scene OR NOT TARGET conjure::renderer)
    message(STATUS "integration: scene and renderer are required, integration tests skipped")
    return()
endif()

# Only the scripting scenario needs conjure::scripting. The sanitizer presets configure with
# CONJURE_WITH_SCRIPTING=OFF (08_testing.md D13) and must still run the asset scenarios under label
# integration, so that one file and its dependency are added only when the module exists.
set(integrationScriptingSources test_scripting_scene.cpp)
set(integrationScriptingDeps conjure::scripting)
if(NOT TARGET conjure::scripting)
    message(STATUS "integration: conjure::scripting is not available, test_scripting_scene.cpp omitted")
    set(integrationScriptingSources "")
    set(integrationScriptingDeps "")
endif()

conjure_add_test_module(integration
    SOURCES
        integration_test_support.cpp
        integration_test_support.h
        test_assets_renderer_headless.cpp
        test_assets_scene.cpp
        ${integrationScriptingSources}
    DEPS
        conjure::assets
        conjure::ecs
        conjure::renderer
        conjure::rhi
        conjure::scene
        ${integrationScriptingDeps}
    LABELS integration
    TIMEOUT 300
)

# The renderer reads the index conjure_shaders built (the same arrangement as tests/renderer).
add_dependencies(conjure_integration_tests conjure_shaders)
get_target_property(integrationShaderDir conjure_shaders CONJURE_SHADER_OUTPUT_DIR)
target_compile_definitions(conjure_integration_tests PRIVATE
    CONJURE_TEST_SHADER_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${integrationShaderDir}>"
)
