# engine tests: one test_<header>.cpp per public header of conjure/engine plus the topic files of
# docs/design/06_engine_loop.md 10 (startup order, phase order, frame pacer, main-thread queue, the headless
# application, render transforms) and the subsystem / module consumer files. Everything is headless: the null
# window, the null device (when rendering is enabled at all), the null audio backend, memory or temp-directory
# file systems. The renderer-backed cases read the shader index conjure_shaders built (CONJURE_TEST_SHADER_DIR)
# and the scripting cases run on the null script host.

if(NOT TARGET conjure::engine)
    return()
endif()

set(CONJURE_ENGINE_TEST_SOURCES
    engine_test_support.cpp
    engine_test_support.h
    diagnostics/test_check_registry.cpp
    test_application.cpp
    test_application_desc.cpp
    test_application_headless.cpp
    test_builtin_modules.cpp
    test_command_line.cpp
    test_debug_overlay_layer.cpp
    test_engine_config.cpp
    test_engine_context.cpp
    test_engine_error.cpp
    test_engine_phase.cpp
    test_engine_subsystem.cpp
    test_engine_time.cpp
    test_engine_version.cpp
    test_executable_path.cpp
    test_exit_code.cpp
    test_export.cpp
    test_fixed_step_clock.cpp
    test_frame_memory.cpp
    test_frame_pacer.cpp
    test_frame_stats.cpp
    test_game_layer.cpp
    test_imgui_layer.cpp
    test_layer.cpp
    test_layer_stack.cpp
    test_log_category.cpp
    test_main_thread_queue.cpp
    test_module_consumers.cpp
    test_phase_order.cpp
    test_project.cpp
    test_render_failure.cpp
    test_render_transform.cpp
    test_startup_order.cpp
)

set(CONJURE_ENGINE_TEST_DEPS
    conjure::assets
    conjure::ecs
    conjure::modules
    conjure::platform
    conjure::rhi
    conjure::scene
)
if(TARGET conjure::renderer)
    list(APPEND CONJURE_ENGINE_TEST_SOURCES test_renderer_subsystem.cpp)
    list(APPEND CONJURE_ENGINE_TEST_DEPS conjure::renderer)
endif()
if(TARGET conjure::audio)
    list(APPEND CONJURE_ENGINE_TEST_SOURCES test_audio_subsystem.cpp)
    list(APPEND CONJURE_ENGINE_TEST_DEPS conjure::audio)
endif()
if(TARGET conjure::physics)
    list(APPEND CONJURE_ENGINE_TEST_DEPS conjure::physics)
endif()
if(TARGET conjure::scripting)
    list(APPEND CONJURE_ENGINE_TEST_SOURCES test_scripting_subsystem.cpp)
    list(APPEND CONJURE_ENGINE_TEST_DEPS conjure::scripting)
endif()
if(CONJURE_BUILD_EDITOR AND TARGET conjure::thirdparty::imgui)
    list(APPEND CONJURE_ENGINE_TEST_DEPS conjure::thirdparty::imgui)
endif()

conjure_add_test_module(engine
    SOURCES
        ${CONJURE_ENGINE_TEST_SOURCES}
    DEPS
        ${CONJURE_ENGINE_TEST_DEPS}
    TIMEOUT 300
)

# The private adapters (subsystems, module consumers, frame loop) are tested through the engine/src include path.
target_include_directories(conjure_engine_tests PRIVATE "${PROJECT_SOURCE_DIR}/engine/src")

# The renderer-backed cases read the index conjure_shaders built (the same arrangement as tests/renderer).
if(TARGET conjure_shaders)
    add_dependencies(conjure_engine_tests conjure_shaders)
    get_target_property(engineShaderDir conjure_shaders CONJURE_SHADER_OUTPUT_DIR)
    target_compile_definitions(conjure_engine_tests PRIVATE
        CONJURE_TEST_SHADER_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${engineShaderDir}>"
    )
endif()

# Every public header of the module must compile on its own (build-only check, part of ALL).
conjure_header_check(conjure_engine)
