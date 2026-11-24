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
        CONJURE_SCRIPTING_SOURCE_ROOT="$<PATH:CMAKE_PATH,NORMALIZE,${PROJECT_SOURCE_DIR}>"
        CONJURE_SCRIPTING_DOTNET_MIN_MAJOR=${CONJURE_DOTNET_MIN_MAJOR}
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
        test_managed_table.cpp
        test_native_api.cpp
        test_native_api_hash.cpp
        test_native_entries.cpp
        test_null_script_host.cpp
        test_script_activity.cpp
        test_script_activity_managed.cpp
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

# The .NET SDK check of cmake/ConjureManaged.cmake (code sweep F778), through configure probes: a throwaway project
# (LANGUAGES NONE, no compiler), configured afresh on every run, that includes ConjureManaged.cmake with a probe
# directory as its source directory, so the global.json there (or above it) is the one dotnet reads. "pinned" holds a
# copy of the repository's global.json and must select an SDK of CONJURE_DOTNET_MIN_MAJOR and make the file a
# configure dependency; "missing" pins an SDK no machine has; "too_old" asks for a major above every SDK; "unreadable"
# hands the check a program (cmake) whose --version names no SDK version. The failure patterns put [ \n]+ between
# words because CMake wraps the error text at spaces.
if(TARGET conjure_managed_sdk)
    if(NetHost_DOTNET_EXECUTABLE)
        set(sdkProbeDotnet "${NetHost_DOTNET_EXECUTABLE}")
    else()
        set(sdkProbeDotnet "${CONJURE_DOTNET_EXECUTABLE}")
    endif()
    set(sdkProbeRoot "${CMAKE_CURRENT_BINARY_DIR}/dotnet_sdk_probe")
    set(sdkProbeIncluder [=[
cmake_minimum_required(VERSION 3.28)
project(conjure_dotnet_sdk_probe NONE)
include("${CONJURE_PROBE_LIST}")
get_property(configureDepends DIRECTORY PROPERTY CMAKE_CONFIGURE_DEPENDS)
if("${PROJECT_SOURCE_DIR}/global.json" IN_LIST configureDepends)
    message(STATUS "probe: global.json is a configure dependency")
endif()
]=])
    foreach(probe IN ITEMS pinned missing too_old unreadable)
        file(WRITE "${sdkProbeRoot}/${probe}/CMakeLists.txt" "${sdkProbeIncluder}")
    endforeach()
    foreach(probe IN ITEMS pinned too_old)
        file(COPY_FILE "${PROJECT_SOURCE_DIR}/global.json" "${sdkProbeRoot}/${probe}/global.json")
    endforeach()
    file(WRITE "${sdkProbeRoot}/missing/global.json"
        "{\n  \"sdk\": {\n    \"version\": \"99.0.100\",\n    \"rollForward\": \"latestFeature\",\n"
        "    \"allowPrerelease\": false\n  }\n}\n"
    )

    function(conjure_add_dotnet_sdk_probe name probe dotnet minimumMajor pattern)
        add_test(NAME build.${name}
            COMMAND "${CMAKE_COMMAND}" --fresh -S "${sdkProbeRoot}/${probe}" -B "${sdkProbeRoot}/build/${probe}"
                -G "${CMAKE_GENERATOR}" "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
                "-DCONJURE_PROBE_LIST=${PROJECT_SOURCE_DIR}/cmake/ConjureManaged.cmake"
                "-DNetHost_DOTNET_EXECUTABLE=${dotnet}" "-DCONJURE_DOTNET_MIN_MAJOR=${minimumMajor}"
        )
        set_tests_properties(build.${name} PROPERTIES LABELS "build" TIMEOUT 60 PASS_REGULAR_EXPRESSION "${pattern}")
    endfunction()

    set(sdkProbeSelected "Managed build uses the \\.NET SDK ${CONJURE_DOTNET_MIN_MAJOR}\\.[0-9]+\\.[0-9]+")
    conjure_add_dotnet_sdk_probe(managed_sdk_follows_global_json pinned "${sdkProbeDotnet}" ${CONJURE_DOTNET_MIN_MAJOR}
        "${sdkProbeSelected}.*probe: global\\.json is a configure dependency"
    )
    conjure_add_dotnet_sdk_probe(managed_sdk_not_installed_fails_configure missing "${sdkProbeDotnet}"
        ${CONJURE_DOTNET_MIN_MAJOR}
        "global\\.json[ \n]+names.*is[ \n]+not[ \n]+installed"
    )
    conjure_add_dotnet_sdk_probe(managed_sdk_too_old_fails_configure too_old "${sdkProbeDotnet}" 99
        "need[ \n]+the[ \n]+\\.NET[ \n]+99[ \n]+SDK"
    )
    conjure_add_dotnet_sdk_probe(managed_sdk_version_unreadable_fails_configure unreadable "${CMAKE_COMMAND}"
        ${CONJURE_DOTNET_MIN_MAJOR}
        "printed[ \n]+no[ \n]+SDK[ \n]+version"
    )
endif()

# The committed generator outputs (native_api_hash.h, NativeApi.g.cs, the manifest) must match a fresh run.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME conjure_scripting_api_generated_up_to_date
        COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tools/generate_native_api.py" --check
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
    set_tests_properties(conjure_scripting_api_generated_up_to_date PROPERTIES LABELS "scripting;generated" TIMEOUT 60)
endif()
