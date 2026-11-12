# Benchmarks (docs/design/08_testing.md D10, 2.4, 3.6, 4.7, 7.1; ARCHITECTURE.md 10 / 11): the in-house
# harness library conjure::bench_harness (include/conjure/bench), its doctest self-tests (tests/) and the
# executable conjure_benchmarks, one scenario file per module. Every scenario is one CTest entry
# "benchmark.<module>.<scenario>" (label "benchmark", plus the module name) that runs the executable with
# --filter <name>; the default test presets exclude the label, so benchmarks never slow a unit run.
#
# Thresholds (thresholds.json next to this file) are enforced in Development and Distribution (exit 1 on a
# breach or a missing entry) and only reported in Debug (CONJURE_BENCH_ENFORCE_DEFAULT); --enforce /
# --no-enforce override either default. A scenario that never measured or called fail() exits 1 in every
# configuration. A scenario whose machine precondition is unmet (no .NET runtime) exits 77, which CTest
# reports as Not Run.

if(NOT TARGET conjure::engine)
    message(STATUS "benchmarks: conjure::engine is not available, benchmarks skipped")
    return()
endif()

set(CONJURE_BENCH_HARNESS_HEADERS
    include/conjure/bench/bench_runner.h
    include/conjure/bench/benchmark.h
)

set(CONJURE_BENCH_HARNESS_SOURCES
    src/bench_runner.cpp
    src/benchmark.cpp
)

add_library(conjure_bench_harness STATIC ${CONJURE_BENCH_HARNESS_HEADERS} ${CONJURE_BENCH_HARNESS_SOURCES})
add_library(conjure::bench_harness ALIAS conjure_bench_harness)
target_include_directories(conjure_bench_harness PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(conjure_bench_harness
    PUBLIC
        conjure::core
        conjure::test_support
        conjure::platform_defines
    PRIVATE
        conjure::warnings
        conjure::sanitizers
)
set_target_properties(conjure_bench_harness PROPERTIES FOLDER "Tests/Benchmarks")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${CONJURE_BENCH_HARNESS_HEADERS} ${CONJURE_BENCH_HARNESS_SOURCES})

# The scenario files and the CTest entry names they register (CONJURE_BENCHMARK). The two lists are kept
# in step by hand: a scenario without an add_test runs only by hand, an add_test without a scenario fails
# with exit code 2 ("no scenario matches").
set(CONJURE_BENCHMARK_SOURCES
    bench_core.cpp
    bench_ecs.cpp
    bench_scene.cpp
    bench_support.cpp
    bench_support.h
    bench_full_frame_10k.cpp
)
set(CONJURE_BENCHMARK_SCENARIOS
    core.job_parallel_for_1m
    ecs.iterate_100k
    ecs.spawn_destroy_10k
    ecs.query_changed_10k
    scene.transform_propagation_10k
    scene.transform_propagation_10k_sparse
    scene.snapshot_capture_restore_10k
    scene.load_10k
)
set(CONJURE_BENCHMARK_DEPS
    conjure::bench_harness
    conjure::engine
    conjure::assets
    conjure::ecs
    conjure::scene
)

if(CONJURE_WITH_PHYSICS AND TARGET conjure::physics)
    list(APPEND CONJURE_BENCHMARK_SOURCES bench_physics.cpp)
    list(APPEND CONJURE_BENCHMARK_SCENARIOS
        physics.step_10k_active
        physics.step_10k_sleeping
        physics.sync_10k
        physics.step_2k_settling
        engine.full_frame_10k_physics
    )
    list(APPEND CONJURE_BENCHMARK_DEPS conjure::physics)
endif()
if(CONJURE_WITH_RENDERER AND TARGET conjure::renderer)
    list(APPEND CONJURE_BENCHMARK_SOURCES bench_render_null_frame.cpp)
    list(APPEND CONJURE_BENCHMARK_SCENARIOS
        renderer.extract_10k
        renderer.null_frame_10k
        engine.full_frame_10k_transform_render
    )
    list(APPEND CONJURE_BENCHMARK_DEPS conjure::renderer conjure::rhi)
endif()
if(CONJURE_WITH_SCRIPTING AND TARGET conjure::scripting)
    list(APPEND CONJURE_BENCHMARK_SOURCES bench_scripts_update_phase.cpp)
    list(APPEND CONJURE_BENCHMARK_SCENARIOS
        scripting.update_10k
        scripting.update_10k_dod
        scripting.native_call_100k
        engine.full_frame_10k_scripts
    )
    list(APPEND CONJURE_BENCHMARK_DEPS conjure::scripting)
endif()

# The benchmark executable has its own main (no doctest) but the same global operator new replacement as
# every test executable, so allocation_scope counts std containers exactly (08_testing.md D3).
conjure_add_executable(conjure_benchmarks
    FOLDER "Tests/Benchmarks"
    SOURCES
        ${CONJURE_BENCHMARK_SOURCES}
        src/bench_main.cpp
        src/doctest_implementation.cpp
    DEPS
        ${CONJURE_BENCHMARK_DEPS}
)
set(CONJURE_BENCHMARK_TRACKED_NEW "${CONJURE_TEST_SUPPORT_DIR}/src/tracked_global_new.cpp")
target_sources(conjure_benchmarks PRIVATE "${CONJURE_BENCHMARK_TRACKED_NEW}")
source_group("support" FILES "${CONJURE_BENCHMARK_TRACKED_NEW}")

# Private engine headers (the renderer component names live in engine/src/engine/subsystems).
target_include_directories(conjure_benchmarks PRIVATE "${PROJECT_SOURCE_DIR}/engine/src")
target_compile_options(conjure_benchmarks PRIVATE ${CONJURE_TEST_WARNING_OPTIONS})

set(CONJURE_BENCH_THRESHOLDS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/thresholds.json")
target_compile_definitions(conjure_benchmarks PRIVATE
    CONJURE_BENCH_THRESHOLDS_FILE="$<PATH:CMAKE_PATH,NORMALIZE,${CONJURE_BENCH_THRESHOLDS_FILE}>"
    CONJURE_BENCH_ENFORCE_DEFAULT=$<IF:$<CONFIG:Debug>,0,1>
)

if(TARGET conjure_shaders)
    add_dependencies(conjure_benchmarks conjure_shaders)
    get_target_property(benchShaderDir conjure_shaders CONJURE_SHADER_OUTPUT_DIR)
    target_compile_definitions(conjure_benchmarks PRIVATE
        CONJURE_BENCH_SHADER_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${benchShaderDir}>"
    )
endif()

if(TARGET conjure_managed_sdk AND TARGET conjure_managed_test_scripts_v1)
    add_dependencies(conjure_benchmarks conjure_managed_sdk conjure_managed_test_scripts_v1)
    target_compile_definitions(conjure_benchmarks PRIVATE
        CONJURE_BENCH_HAS_DOTNET=1
        CONJURE_BENCH_MANAGED_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${CONJURE_MANAGED_SDK_DIR}>"
        CONJURE_BENCH_TEST_SCRIPTS_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${CONJURE_MANAGED_TEST_SCRIPTS_DIR}>"
    )
else()
    target_compile_definitions(conjure_benchmarks PRIVATE CONJURE_BENCH_HAS_DOTNET=0)
endif()

# One CTest entry per scenario. The [dotnet] scenarios carry the labels scripting and dotnet as well so a
# CI job without a .NET runtime can leave them out explicitly instead of collecting Not Run entries.
foreach(scenario IN LISTS CONJURE_BENCHMARK_SCENARIOS)
    string(REGEX REPLACE "\\..*$" "" scenarioModule "${scenario}")
    set(scenarioLabels "benchmark;${scenarioModule}")
    if(scenario MATCHES "^scripting\\." OR scenario MATCHES "_scripts$")
        list(APPEND scenarioLabels scripting dotnet)
    endif()
    list(REMOVE_DUPLICATES scenarioLabels)
    add_test(NAME "benchmark.${scenario}"
        COMMAND conjure_benchmarks --filter "${scenario}" --thresholds "${CONJURE_BENCH_THRESHOLDS_FILE}"
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
    )
    set_tests_properties("benchmark.${scenario}" PROPERTIES
        LABELS "${scenarioLabels}"
        TIMEOUT 900
        SKIP_RETURN_CODE 77
    )
endforeach()

add_subdirectory(tests)
