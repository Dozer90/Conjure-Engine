# scripting tests: one test_<header>.cpp per public header of conjure/scripting plus the topic files of
# docs/design/03_scripting.md 10. The null-host cases run everywhere; the [dotnet] cases start the real CLR
# and need precondition::DOTNET_RUNTIME (the probe lives in scripting_test_support.cpp), which holds when
# the managed SDK target exists and a .NET runtime is installed. The generator check regenerates the
# native table outputs and diffs them against the committed files.
#
# The cases that spawn dotnet themselves (the self-contained publish and the test-scripts build, minutes on
# a cold machine) live in the second executable conjure_scripting_slow_tests, labelled "dotnet;slow" so the
# default test presets skip them and their TIMEOUT covers a full restore (D13).

if(NOT TARGET conjure::scripting)
    return()
endif()

# The paths and the SDK flag both executables compile against.
function(conjure_scripting_test_definitions target)
    target_compile_definitions(${target} PRIVATE
        CONJURE_SCRIPTING_MANIFEST_PATH="$<PATH:CMAKE_PATH,NORMALIZE,${PROJECT_SOURCE_DIR}/docs/generated/native_api_manifest.json>"
    )
    if(TARGET conjure_managed_sdk AND TARGET conjure_managed_test_scripts_v3)
        add_dependencies(${target} conjure_managed_sdk conjure_managed_test_scripts_v3)
        target_compile_definitions(${target} PRIVATE
            CONJURE_SCRIPTING_HAS_DOTNET=1
            CONJURE_SCRIPTING_MANAGED_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${CONJURE_MANAGED_SDK_DIR}>"
            CONJURE_SCRIPTING_TEST_SCRIPTS_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${CONJURE_MANAGED_TEST_SCRIPTS_DIR}>"
            CONJURE_SCRIPTING_TEST_SCRIPTS_PROJECT="$<PATH:CMAKE_PATH,NORMALIZE,${CONJURE_MANAGED_TEST_SCRIPTS_PROJECT}>"
            CONJURE_SCRIPTING_BOOTSTRAP_PROJECT="$<PATH:CMAKE_PATH,NORMALIZE,${CONJURE_MANAGED_BOOTSTRAP_PROJECT}>"
        )
    else()
        target_compile_definitions(${target} PRIVATE CONJURE_SCRIPTING_HAS_DOTNET=0)
    endif()
endfunction()

conjure_add_test_module(scripting
    SOURCES
        scripting_test_support.cpp
        scripting_test_support.h
        test_dod_query.cpp
        test_dotnet_script_host.cpp
        test_hot_reload.cpp
        test_log_category.cpp
        test_managed_build.cpp
        test_native_api.cpp
        test_native_api_hash.cpp
        test_native_entries.cpp
        test_null_script_host.cpp
        test_script_class_registry.cpp
        test_script_collisions.cpp
        test_script_component.cpp
        test_script_exceptions.cpp
        test_script_fields.cpp
        test_script_fields_inspector.cpp
        test_script_host.cpp
        test_script_hot_reload.cpp
        test_script_lifecycle.cpp
        test_script_runtime.cpp
        test_script_system.cpp
        test_scripting.cpp
        test_scripting_error.cpp
    LABELS scripting
)
conjure_scripting_test_definitions(conjure_scripting_tests)

conjure_add_test_module(scripting_slow
    SOURCES
        scripting_test_support.cpp
        scripting_test_support.h
        slow/test_dotnet_publish.cpp
        slow/test_managed_build_scripts.cpp
    DEPS conjure::scripting
    LABELS scripting;dotnet;slow
    TIMEOUT 900
)
conjure_scripting_test_definitions(conjure_scripting_slow_tests)
target_include_directories(conjure_scripting_slow_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

conjure_header_check(conjure_scripting)

# The committed generator outputs (native_api_hash.h, NativeApi.g.cs, the manifest) must match a fresh run.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME conjure_scripting_api_generated_up_to_date
        COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tools/generate_native_api.py" --check
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
    set_tests_properties(conjure_scripting_api_generated_up_to_date PROPERTIES LABELS "scripting;generated" TIMEOUT 60)
endif()
