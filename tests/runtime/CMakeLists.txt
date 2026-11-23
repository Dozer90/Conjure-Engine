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
#                                     survives, --help -> 0; a scripted project boots headless without a script
#                                     runtime, and with --scripting it starts one (the [dotnet] case runs C#);
#                                     a Windows Distribution build ships the Visual C++ runtime DLLs it copied;
#                                     every build wrote its licence notices next to the runtime and ships them
#   test_missing_project.cpp          the 5.2 project file: missing, broken or too-new project.json and project
#                                     settings that do not validate -> 2, named on stderr and in --log-file
#   test_exit_codes.cpp               bad arguments -> 3 (with the usage text or the help hint on stderr), a
#                                     failed startup -> 1 (named in --log-file as well)
#   runtime.msvc_runtime_folder_rules the probe of runtime/cmake/ConjureMsvcRuntime.cmake: which Visual C++
#                                     redistributable folder a Distribution build ships (every platform)
#   runtime.distribution_path_options the probe of cmake/ConjureDistributionPaths.cmake (code sweep F613): the
#                                     compile options that keep the checkout out of a Distribution binary, for
#                                     every compiler family and root layout (every platform)
#   runtime.check_distribution_symbols_tool
#                                     the self-test of tools/check_distribution_symbols.py (always registered)
#   runtime.check_distribution_paths_tool
#                                     the self-test tools/tests/test_check_distribution_paths.py (always registered)
#   runtime.distribution_symbols      tools/check_distribution_symbols.py over the Distribution conjure_runtime
#                                     and conjure_shared
#   runtime.distribution_paths        tools/check_distribution_paths.py over the same two binaries: neither names
#                                     the source or build directory of the tree (code sweep F613)
#
# Every case spawns the executable, so the module carries the label "process" (08_testing.md 6.3 item 8,
# docs/handbook/ci.md): the default test presets skip it and CI runs it explicitly after the build:
#   ctest --test-dir build/<preset> -L process --output-on-failure
# The directory runtime.distribution_symbols and runtime.distribution_paths check is CONJURE_DISTRIBUTION_BUILD_DIR
# when set (a Debug build can then verify a Distribution build next to it), else this build directory when it is a
# Distribution one (or a multi-config build, where the tools report 77 = skipped unless ctest runs -C Distribution).

if(NOT TARGET conjure_runtime OR NOT TARGET conjure::engine)
    message(STATUS "tests/runtime: conjure_runtime is not built, runtime tests skipped")
    return()
endif()

# The scripted project of the --scripting cases is authored through the script runtime on the null host.
set(CONJURE_RUNTIME_TEST_DEPS
    conjure::assets
    conjure::ecs
    conjure::engine
    conjure::scene
)
if(TARGET conjure::scripting)
    list(APPEND CONJURE_RUNTIME_TEST_DEPS conjure::scripting)
endif()

conjure_add_test_module(runtime
    SOURCES
        runtime_test_support.cpp
        runtime_test_support.h
        test_exit_codes.cpp
        test_missing_project.cpp
        test_runtime_boot.cpp
    DEPS
        ${CONJURE_RUNTIME_TEST_DEPS}
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

# The Visual C++ runtime DLLs an MSVC Distribution build copies next to conjure_runtime (runtime/CMakeLists.txt,
# 06_engine_loop.md 3.6 step 5): the case that looks for them runs in the configuration that copies them and
# reports that it has nothing to check in every other one.
get_target_property(runtimeCrtLibraries conjure_runtime CONJURE_MSVC_CRT_LIBRARIES)
if(runtimeCrtLibraries)
    target_compile_definitions(conjure_runtime_tests PRIVATE
        CONJURE_RUNTIME_TEST_HAS_MSVC_CRT=$<IF:$<CONFIG:Distribution>,1,0>
    )
else()
    target_compile_definitions(conjure_runtime_tests PRIVATE CONJURE_RUNTIME_TEST_HAS_MSVC_CRT=0)
endif()

# The [dotnet] case runs Conjure.TestScripts on the real CLR inside conjure_runtime, which loads the managed SDK
# from "managed" next to itself. It is compiled only where the build can run it (CONJURE_RUNTIME_TEST_HAS_DOTNET=1:
# the scripting module, the managed SDK and the test scripts), the way tests/integration lists its [dotnet] files
# and tests/editor compiles its own: a build without them (the sanitizer leg, -DCONJURE_WITH_SCRIPTING=OFF) has no
# entry that could only exit 77, which tools/ctest_check_skips.py refuses for [dotnet] on CI. Where it is compiled
# it is guarded by precondition::DOTNET_RUNTIME (probe in runtime_test_support.cpp) and skipped only on a machine
# without a .NET runtime.
if(TARGET conjure::scripting AND TARGET conjure_managed_sdk AND TARGET conjure_managed_test_scripts_v1)
    add_dependencies(conjure_runtime_tests conjure_managed_sdk conjure_managed_test_scripts_v1)
    target_compile_definitions(conjure_runtime_tests PRIVATE
        CONJURE_RUNTIME_TEST_HAS_DOTNET=1
        CONJURE_RUNTIME_TEST_SCRIPTS_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${CONJURE_MANAGED_TEST_SCRIPTS_DIR}>"
    )
else()
    target_compile_definitions(conjure_runtime_tests PRIVATE CONJURE_RUNTIME_TEST_HAS_DOTNET=0)
endif()

#------------------------------------------------------------------------------
# Which Visual C++ redistributable folder a Distribution build ships (runtime/cmake/ConjureMsvcRuntime.cmake)
#------------------------------------------------------------------------------
# The rule is path and version handling, so its probe runs on every platform over fake folders in a scratch
# directory of this tree: a folder older than the compiler's toolset or without a readable version is refused,
# the fallback candidate is taken when the first one is refused, and nothing is the answer when none passes
# (06_engine_loop.md D14). No compiler and no Visual Studio are involved. The label is "build", not "process":
# the entry spawns nothing but CMake and belongs to the default test run.
add_test(NAME runtime.msvc_runtime_folder_rules
    COMMAND "${CMAKE_COMMAND}" "-DPROBE_ROOT=${CMAKE_CURRENT_BINARY_DIR}/msvc_runtime_probe"
        -P "${PROJECT_SOURCE_DIR}/runtime/cmake/ConjureMsvcRuntimeProbe.cmake"
)
set_tests_properties(runtime.msvc_runtime_folder_rules PROPERTIES
    LABELS "runtime;build" TIMEOUT 120 PASS_REGULAR_EXPRESSION "msvc runtime probe: passed"
)

#------------------------------------------------------------------------------
# Source paths in Distribution binaries (cmake/ConjureDistributionPaths.cmake, code sweep F613)
#------------------------------------------------------------------------------
# The options are string handling, so their probe runs in script mode on every platform, with the CMake that
# configured this tree, over stand-in roots and compiler ids. Label "build", as the probe above.
add_test(NAME runtime.distribution_path_options
    COMMAND "${CMAKE_COMMAND}" -P "${PROJECT_SOURCE_DIR}/cmake/ConjureDistributionPathsProbe.cmake"
)
set_tests_properties(runtime.distribution_path_options PROPERTIES
    LABELS "runtime;build" TIMEOUT 120 PASS_REGULAR_EXPRESSION "distribution paths probe: passed"
)

#------------------------------------------------------------------------------
# Distribution stripping and source paths (tools/check_distribution_symbols.py, tools/check_distribution_paths.py)
#------------------------------------------------------------------------------
set(CONJURE_DISTRIBUTION_BUILD_DIR "" CACHE PATH
    "A built Distribution build directory whose conjure_runtime and conjure_shared the tests \
runtime.distribution_symbols and runtime.distribution_paths check (empty: this directory when it is a \
Distribution build)")

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    set(checkSymbols "${PROJECT_SOURCE_DIR}/tools/check_distribution_symbols.py")

    add_test(NAME runtime.check_distribution_symbols_tool
        COMMAND Python3::Interpreter -I "${checkSymbols}" --self-test
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
    set_tests_properties(runtime.check_distribution_symbols_tool PROPERTIES LABELS "runtime;tools" TIMEOUT 120)

    # Its case of the options probe runs the CMake of this tree (CONJURE_CMAKE_COMMAND), not one found on PATH.
    add_test(NAME runtime.check_distribution_paths_tool
        COMMAND Python3::Interpreter -I "${PROJECT_SOURCE_DIR}/tools/tests/test_check_distribution_paths.py"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
    set_tests_properties(runtime.check_distribution_paths_tool PROPERTIES
        LABELS "runtime;tools" TIMEOUT 120 ENVIRONMENT "CONJURE_CMAKE_COMMAND=${CMAKE_COMMAND}"
    )

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

        # It reads the bytes of the binaries itself, so it takes no symbol-listing tool.
        add_test(NAME runtime.distribution_paths
            COMMAND Python3::Interpreter -I "${PROJECT_SOURCE_DIR}/tools/check_distribution_paths.py"
                ${distributionArguments}
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        )
        set_tests_properties(runtime.distribution_paths PROPERTIES
            LABELS "runtime;process" TIMEOUT 300 SKIP_RETURN_CODE 77
        )
    else()
        message(STATUS "tests/runtime: not a Distribution build and CONJURE_DISTRIBUTION_BUILD_DIR is empty; "
                       "runtime.distribution_symbols and runtime.distribution_paths not registered")
    endif()
else()
    message(STATUS "tests/runtime: Python 3 not found, the check_distribution_symbols and "
                   "check_distribution_paths tests are not registered")
endif()
