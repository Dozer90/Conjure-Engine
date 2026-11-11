# bindings tests: one test_<header>.cpp per public header of conjure/bindings plus the topic files of
# docs/design/15_bindings.md 10 (one per native group, the DOD span routing and the [dotnet] facade run).
# Everything is headless: a bare world + scene manager + asset manager over a memory source for the entry
# tests, the null platform, audio and script-host backends, and engine::application with the null host for
# the subsystem cases. The [dotnet] cases start the real CLR and are guarded by precondition::DOTNET_RUNTIME.

if(NOT TARGET conjure::bindings)
    return()
endif()

set(CONJURE_BINDINGS_TEST_SOURCES
    bindings_test_support.cpp
    bindings_test_support.h
    test_asset_bindings.cpp
    test_bindings.cpp
    test_bindings_error.cpp
    test_bindings_managed.cpp
    test_bindings_subsystem.cpp
    test_call_counters.cpp
    test_diagnostics_bindings.cpp
    test_entity_bindings.cpp
    test_input_bindings.cpp
    test_log_category.cpp
    test_marshal.cpp
    test_modules_bindings.cpp
    test_native_api_entries.cpp
    test_prefab_scene_bindings.cpp
    test_span_scope_routing.cpp
    test_transform_bindings.cpp
)

set(CONJURE_BINDINGS_TEST_DEPS
    conjure::assets
    conjure::ecs
    conjure::engine
    conjure::modules
    conjure::platform
    conjure::scene
    conjure::scripting
)
if(TARGET conjure::physics)
    list(APPEND CONJURE_BINDINGS_TEST_SOURCES test_physics_bindings.cpp)
    list(APPEND CONJURE_BINDINGS_TEST_DEPS conjure::physics)
endif()
if(TARGET conjure::audio)
    list(APPEND CONJURE_BINDINGS_TEST_SOURCES test_audio_bindings.cpp)
    list(APPEND CONJURE_BINDINGS_TEST_DEPS conjure::audio)
endif()
if(TARGET conjure::renderer)
    list(APPEND CONJURE_BINDINGS_TEST_DEPS conjure::renderer)
endif()

conjure_add_test_module(bindings
    SOURCES
        ${CONJURE_BINDINGS_TEST_SOURCES}
    DEPS
        ${CONJURE_BINDINGS_TEST_DEPS}
    LABELS bindings
    TIMEOUT 300
)

# The manifest the generator wrote and, when the SDK is built, the managed outputs the [dotnet] cases load.
target_compile_definitions(conjure_bindings_tests PRIVATE
    CONJURE_BINDINGS_MANIFEST_PATH="$<PATH:CMAKE_PATH,NORMALIZE,${PROJECT_SOURCE_DIR}/docs/generated/native_api_manifest.json>"
)
if(TARGET conjure_managed_sdk AND TARGET conjure_managed_test_scripts_v1)
    add_dependencies(conjure_bindings_tests conjure_managed_sdk conjure_managed_test_scripts_v1)
    target_compile_definitions(conjure_bindings_tests PRIVATE
        CONJURE_BINDINGS_HAS_DOTNET=1
        CONJURE_BINDINGS_MANAGED_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${CONJURE_MANAGED_SDK_DIR}>"
        CONJURE_BINDINGS_TEST_SCRIPTS_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${CONJURE_MANAGED_TEST_SCRIPTS_DIR}>"
    )
else()
    target_compile_definitions(conjure_bindings_tests PRIVATE CONJURE_BINDINGS_HAS_DOTNET=0)
endif()

# Every public header of the module must compile on its own (build-only check, part of ALL).
conjure_header_check(conjure_bindings)
