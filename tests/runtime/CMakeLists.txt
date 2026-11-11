# runtime tests (docs/design/06_engine_loop.md 4.2, docs/ARCHITECTURE.md 11 "conjure_runtime" row,
# docs/design/08_testing.md 5.2 runtime row and 6.3 item 8, docs/design/04_editor.md 8 "Distribution stripping").
#
#   runtime_test_support.{h,cpp}      the shared fixtures: the built conjure_runtime spawned through
#                                     core::process, a tiny project generated into a temp directory and its
#                                     export through engine::export_pipeline
#   test_runtime_boot.cpp             the 5.2 boot file: an exported folder boots headless for 60 frames with
#                                     exit 0 (with the build's runtime, with the copy the export placed into the
#                                     folder, and without --project from that copy), a development project boots
#                                     directly, the flags reach the configuration, a non-ASCII project path
#                                     survives, --help -> 0
#   test_missing_project.cpp          the 5.2 project file: missing, broken or too-new project.json and project
#                                     settings that do not validate -> 2, named on stderr and in --log-file
#   test_exit_codes.cpp               bad arguments -> 3 (with the usage text or the help hint on stderr), a
#                                     failed startup -> 1 (named in --log-file as well)
#   runtime.check_distribution_symbols_tool
#                                     the self-test of tools/check_distribution_symbols.py (always registered)
#   runtime.distribution_symbols      tools/check_distribution_symbols.py over the Distribution conjure_runtime
#                                     and conjure_shared
#
# Every case spawns the executable, so the module carries the label "process" (08_testing.md 6.3 item 8,
# docs/handbook/ci.md): the default test presets skip it and CI runs it explicitly after the build:
#   ctest --test-dir build/<preset> -L process --output-on-failure
# The directory runtime.distribution_symbols checks is CONJURE_DISTRIBUTION_BUILD_DIR when set (a Debug build
# can then verify a Distribution build next to it), else this build directory when it is a Distribution one
# (or a multi-config build, where the tool reports 77 = skipped unless ctest runs -C Distribution).

if(NOT TARGET conjure_runtime OR NOT TARGET conjure::engine)
    message(STATUS "tests/runtime: conjure_runtime is not built, runtime tests skipped")
    return()
endif()

conjure_add_test_module(runtime
    SOURCES
        runtime_test_support.cpp
        runtime_test_support.h
        test_exit_codes.cpp
        test_missing_project.cpp
        test_runtime_boot.cpp
    DEPS
        conjure::assets
        conjure::ecs
        conjure::engine
        conjure::scene
    LABELS process
    TIMEOUT 300
)

# The tests spawn the executable; they find it and the directory holding conjure_shared through their build
# paths. The fixture header sits next to the test files.
add_dependencies(conjure_runtime_tests conjure_runtime)
target_include_directories(conjure_runtime_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(conjure_runtime_tests PRIVATE
    CONJURE_RUNTIME_EXECUTABLE="$<PATH:CMAKE_PATH,NORMALIZE,$<TARGET_FILE:conjure_runtime>>"
    CONJURE_RUNTIME_BIN_DIR="$<PATH:CMAKE_PATH,NORMALIZE,$<TARGET_FILE_DIR:conjure_runtime>>"
)

#------------------------------------------------------------------------------
# Distribution stripping (tools/check_distribution_symbols.py)
#------------------------------------------------------------------------------
set(CONJURE_DISTRIBUTION_BUILD_DIR "" CACHE PATH
    "A built Distribution build directory whose conjure_runtime and conjure_shared the test \
runtime.distribution_symbols checks (empty: this directory when it is a Distribution build)")

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    set(checkSymbols "${PROJECT_SOURCE_DIR}/tools/check_distribution_symbols.py")

    add_test(NAME runtime.check_distribution_symbols_tool
        COMMAND Python3::Interpreter -I "${checkSymbols}" --self-test
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
    set_tests_properties(runtime.check_distribution_symbols_tool PROPERTIES LABELS "runtime;tools" TIMEOUT 120)

    # The symbol-listing tool of the toolchain that linked the binaries: dumpbin sits beside the MSVC
    # linker, GCC and Clang toolchains provide nm (CMAKE_NM). Without either the script searches PATH.
    set(symbolToolArguments "")
    if(MSVC)
        get_filename_component(linkerDirectory "${CMAKE_LINKER}" DIRECTORY)
        if(EXISTS "${linkerDirectory}/dumpbin.exe")
            set(symbolToolArguments --tool "${linkerDirectory}/dumpbin.exe")
        endif()
    elseif(CMAKE_NM)
        set(symbolToolArguments --tool "${CMAKE_NM}")
    endif()

    set(distributionArguments "")
    if(CONJURE_DISTRIBUTION_BUILD_DIR)
        set(distributionArguments --build-dir "${CONJURE_DISTRIBUTION_BUILD_DIR}")
    elseif(CONJURE_IS_MULTI_CONFIG)
        set(distributionArguments --build-dir "${PROJECT_BINARY_DIR}" --config "$<CONFIG>")
    elseif(CMAKE_BUILD_TYPE STREQUAL "Distribution")
        set(distributionArguments --build-dir "${PROJECT_BINARY_DIR}")
    endif()

    if(distributionArguments)
        add_test(NAME runtime.distribution_symbols
            COMMAND Python3::Interpreter -I "${checkSymbols}" ${distributionArguments} ${symbolToolArguments}
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        )
        set_tests_properties(runtime.distribution_symbols PROPERTIES
            LABELS "runtime;process" TIMEOUT 300 SKIP_RETURN_CODE 77
        )
    else()
        message(STATUS "tests/runtime: not a Distribution build and CONJURE_DISTRIBUTION_BUILD_DIR is empty; "
                       "runtime.distribution_symbols not registered")
    endif()
else()
    message(STATUS "tests/runtime: Python 3 not found, the check_distribution_symbols tests are not registered")
endif()
