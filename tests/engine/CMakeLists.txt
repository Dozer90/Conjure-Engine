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
    test_dotnet_sdk.cpp
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
# The cases that open the shader library next to the executable cannot pass in a multi-configuration tree
# (bin/<Config>/ against bin/shaders, code sweep F329) and are compiled out there.
if(CONJURE_IS_MULTI_CONFIG)
    target_compile_definitions(conjure_engine_tests PRIVATE CONJURE_TEST_MULTI_CONFIG=1)
endif()

# test_dotnet_sdk.cpp (owner decision OWN-INSTALL) compares the baked-in SDK version with global.json, and it and
# test_export.cpp stand in for a dotnet without an SDK with conjure_process_child (tests/core/system), which answers
# --version with its usage line and exit code 2, or with a copy of it that replays a fixed mode.
file(READ "${PROJECT_SOURCE_DIR}/global.json" engineTestGlobalJson)
string(JSON engineTestDotnetSdk GET "${engineTestGlobalJson}" sdk version)
add_dependencies(conjure_engine_tests conjure_process_child)
target_compile_definitions(conjure_engine_tests PRIVATE
    CONJURE_ENGINE_TEST_GLOBAL_JSON_SDK="${engineTestDotnetSdk}"
    CONJURE_ENGINE_TEST_PROCESS_CHILD="$<PATH:CMAKE_PATH,NORMALIZE,$<TARGET_FILE:conjure_process_child>>"
)

# The [dotnet][slow] export cases of test_export.cpp run export step 3 with the real dotnet over copies of the
# engine's managed projects. The first (follow-up FU-PDB) exports the game template and checks the managed
# assemblies with tools/check_distribution_paths.py, outside git and, with git, in a repository with a commit; the
# second (follow-up FU-MANAGEDDIR) exports the scripts project the editor's script.create writes, read from the
# templates in its source; the third (follow-up FU-ARTIFACTS) exports the game template into the default artifacts
# directory and into one the caller names, and checks that nothing lands in the staged engine's
# build/managed_artifacts or the game's scripts/obj and bin. They are compiled only where the build has a .NET SDK
# (the managed SDK target), so no build registers an entry that could only skip; without Python the FU-PDB case
# checks the assemblies' own bytes and says that the .pdb documents went unchecked, and without git it says that the
# repository SUBCASE went unchecked.
if(TARGET conjure::scripting AND TARGET conjure_managed_sdk)
    if(NetHost_DOTNET_EXECUTABLE)
        set(engineTestDotnet "${NetHost_DOTNET_EXECUTABLE}")
    elseif(CONJURE_DOTNET_EXECUTABLE)
        set(engineTestDotnet "${CONJURE_DOTNET_EXECUTABLE}")
    else()
        set(engineTestDotnet "dotnet")
    endif()
    # Normalised to '/' so that a Windows path is no string of escape sequences in the C++ literal.
    find_package(Python3 COMPONENTS Interpreter QUIET)
    set(engineTestPython "")
    if(Python3_Interpreter_FOUND)
        set(engineTestPython "$<PATH:CMAKE_PATH,NORMALIZE,${Python3_EXECUTABLE}>")
    endif()
    find_package(Git QUIET)
    set(engineTestGit "")
    if(GIT_EXECUTABLE)
        set(engineTestGit "$<PATH:CMAKE_PATH,NORMALIZE,${GIT_EXECUTABLE}>")
    endif()
    target_compile_definitions(conjure_engine_tests PRIVATE
        CONJURE_ENGINE_TEST_HAS_DOTNET=1
        CONJURE_ENGINE_TEST_DOTNET="$<PATH:CMAKE_PATH,NORMALIZE,${engineTestDotnet}>"
        CONJURE_ENGINE_TEST_PYTHON="${engineTestPython}"
        CONJURE_ENGINE_TEST_GIT="${engineTestGit}"
        CONJURE_ENGINE_TEST_SOURCE_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${PROJECT_SOURCE_DIR}>"
        CONJURE_ENGINE_TEST_BINARY_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${PROJECT_BINARY_DIR}>"
    )
else()
    target_compile_definitions(conjure_engine_tests PRIVATE CONJURE_ENGINE_TEST_HAS_DOTNET=0)
endif()

# The [slow] cases take a minute: label them so the default presets, which exclude "slow", skip them and the nightly
# presets run them. The [dotnet] exports spawn dotnet and get 3120 s instead of the module's 300: the doctest timeout
# of the largest of them (three SUBCASEs of DOTNET_SUBCASE_SECONDS in test_export.cpp, 3060 s) and a minute more, so
# that a hung command fails the case naming it before CTest kills the process. doctest's discovery already
# registered every case; the side file adds the labels to the cases whose name carries the tag.
set(engineTagLabelsFile "${CMAKE_CURRENT_BINARY_DIR}/conjure_engine_tests_tag_labels.cmake")
file(WRITE "${engineTagLabelsFile}"
    "if(DEFINED conjure_engine_tests_TESTS)\n"
    "    foreach(entry IN LISTS conjure_engine_tests_TESTS)\n"
    "        if(entry MATCHES \"\\\\[dotnet\\\\]\\\\[slow\\\\]\")\n"
    "            set_tests_properties(\"\${entry}\" PROPERTIES LABELS \"unit;engine;dotnet;slow\" TIMEOUT 3120)\n"
    "        elseif(entry MATCHES \"\\\\[slow\\\\]\")\n"
    "            set_tests_properties(\"\${entry}\" PROPERTIES LABELS \"unit;engine;slow\")\n"
    "        endif()\n"
    "    endforeach()\n"
    "endif()\n"
)
set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES "${engineTagLabelsFile}")

# Every public header of the module must compile on its own (build-only check, part of ALL).
conjure_header_check(conjure_engine)
