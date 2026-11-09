# Test infrastructure helpers (docs/design/08_testing.md 3.8, docs/design/10_core.md 3).
#
#   conjure_add_test_module(<module> SOURCES <files> [DEPS <targets>] [ENGINE] [LINK_ENGINE_SHARED]
#                           [LABELS <labels>] [TIMEOUT <seconds>])
#
#     Creates the executable conjure_<module>_tests from SOURCES plus the shared doctest main
#     (tests/support/src/test_main.cpp) and the global operator new/delete replacement
#     (tests/support/src/tracked_global_new.cpp), linking conjure::<module> when that target exists
#     (test groups without a library of their own, such as `support` or `integration`, list what they
#     need in DEPS), conjure::test_support, conjure::platform_defines, conjure::warnings and
#     conjure::sanitizers. Every TEST_CASE becomes one CTest entry named "<module>.<case>" through
#     doctest_discover_tests(), run with --conjure-isolated from ${PROJECT_BINARY_DIR}/bin, labelled
#     "unit;<module>;<labels>", with TIMEOUT (default 120 s) and SKIP_RETURN_CODE 77.
#
#     LINK_ENGINE_SHARED (07_modules.md D1 and 5, R17) links the umbrella shared library
#     conjure::shared instead of the static conjure::<module>, together with
#     conjure::test_support_shared (tests/support, the same support sources compiled against the
#     shared library), so the executable and the plugins it loads share one engine copy. Such an
#     executable keeps the C runtime's allocation functions: tracked_global_new.cpp releases through
#     tracked_free, which is only correct for memory that came from the executable's own operator
#     new, while objects allocated inside conjure_shared come from the C runtime's (the file documents
#     the core::memory hooks that would make it CRT-compatible). memory_guard and allocation_scope
#     therefore count nothing in a LINK_ENGINE_SHARED executable. The option requires
#     conjure_finalize_engine() to have run (the root CMakeLists.txt does that before tests/).
#
#     ENGINE is accepted today and does nothing: it will add conjure::test_support_engine (wave 4)
#     once that target exists.
#
#     Multi-config generators (the windows-vs preset): doctest 2.4.11's discovery writes one
#     configuration-independent <target>_tests-<hash>.cmake per target from a POST_BUILD step, and
#     the label side file below has the same shape, so the CTest entries always reflect the
#     configuration built last (building Development after Debug makes `ctest -C Debug` run the
#     Development binaries). Single-config presets are unaffected; CMakePresets.json says so on the
#     windows-vs test preset.
#
#   conjure_add_test_executable(<name> SOURCES <files> [DEPS <targets>] [FOLDER <folder>]
#                               [LINK_ENGINE_SHARED])
#
#     The executable half of conjure_add_test_module without CTest registration: a doctest program
#     built from SOURCES plus the shared main. Used for helper programs the test tree spawns (the
#     support library's precondition child) and by conjure_add_test_module itself.
#     LINK_ENGINE_SHARED has the meaning described above.
#
#   conjure_header_check(<target>)
#
#     Compiles every header listed in the target's CONJURE_PUBLIC_HEADERS property (set by
#     conjure_add_module / conjure_add_core_area) in its own translation unit with the target's
#     usage requirements and the project warning set, proving each public header stands alone. The
#     object library <target>_header_check hangs off the aggregate target conjure_header_check,
#     which is part of ALL, so a header that does not compile alone breaks the build.
#
# Options: CONJURE_TEST_GOLDEN_IMAGES (OFF) gates the golden-image tests of later waves; the test
# executables see it as the compile definition CONJURE_TEST_GOLDEN_IMAGES=1.
#
# Test programs additionally receive CONJURE_TEST_WARNING_OPTIONS (below): the one warning that the
# doctest macros themselves trip on Clang 21+ (-Wc2y-extensions for __COUNTER__). Engine targets never
# see it.

include_guard(GLOBAL)

option(CONJURE_TEST_GOLDEN_IMAGES "Build and register the golden-image tests (software rasteriser)" OFF)

set(CONJURE_TEST_SUPPORT_DIR "${PROJECT_SOURCE_DIR}/tests/support")
set(CONJURE_TEST_MAIN_SOURCE "${CONJURE_TEST_SUPPORT_DIR}/src/test_main.cpp")
set(CONJURE_TEST_MAIN_SOURCES
    "${CONJURE_TEST_MAIN_SOURCE}"
    "${CONJURE_TEST_SUPPORT_DIR}/src/tracked_global_new.cpp"
)
set(CONJURE_TEST_DEFAULT_TIMEOUT 120)

# doctest's TEST_CASE / TEST_SUITE / REGISTER_LISTENER macros expand __COUNTER__ inside every test
# translation unit, which Clang 21+ reports as a C2y extension under -Wpedantic; -Werror then fails
# the build. Declaring doctest SYSTEM does not help because the expansion happens in the test file,
# and doctest 2.4.11 offers no configuration that avoids __COUNTER__. Engine code never uses it, so
# the option is applied to test programs only. check_cxx_compiler_flag() rejects unknown options,
# which leaves older Clang (and GCC, which has no such diagnostic) alone.
set(CONJURE_TEST_WARNING_OPTIONS "")
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag(-Wno-c2y-extensions CONJURE_CXX_HAS_WNO_C2Y_EXTENSIONS)
    if(CONJURE_CXX_HAS_WNO_C2Y_EXTENSIONS)
        list(APPEND CONJURE_TEST_WARNING_OPTIONS -Wno-c2y-extensions)
    endif()
endif()


function(conjure_add_test_executable name)
    set(options LINK_ENGINE_SHARED)
    set(oneValueArgs FOLDER)
    set(multiValueArgs SOURCES DEPS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "conjure_add_test_executable(${name}): unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "conjure_add_test_executable(${name}): SOURCES is required")
    endif()
    if(NOT ARG_FOLDER)
        set(ARG_FOLDER "Tests")
    endif()

    set(mainSources ${CONJURE_TEST_MAIN_SOURCES})
    set(supportLibrary conjure::test_support)
    if(ARG_LINK_ENGINE_SHARED)
        if(NOT TARGET conjure::shared OR NOT TARGET conjure::test_support_shared)
            message(FATAL_ERROR
                "conjure_add_test_executable(${name}): LINK_ENGINE_SHARED needs conjure::shared and "
                "conjure::test_support_shared; call conjure_finalize_engine() before adding tests/"
            )
        endif()
        set(mainSources "${CONJURE_TEST_MAIN_SOURCE}")
        set(supportLibrary conjure::test_support_shared)
    endif()

    add_executable(${name} ${ARG_SOURCES} ${mainSources})

    # Tests may reach private module headers ("core/jobs/thread_name.h") through engine/src.
    target_include_directories(${name} PRIVATE "${PROJECT_SOURCE_DIR}/engine/src")
    target_compile_options(${name} PRIVATE ${CONJURE_TEST_WARNING_OPTIONS})

    target_link_libraries(${name}
        PRIVATE
            ${supportLibrary}
            conjure::platform_defines
            conjure::warnings
            conjure::sanitizers
            ${ARG_DEPS}
    )

    # Paths the shared main hands to conjure::testing::environment(). The output and temporary
    # roots are per executable so parallel CTest processes never share a directory.
    target_compile_definitions(${name}
        PRIVATE
            CONJURE_TEST_EXECUTABLE_NAME="${name}"
            CONJURE_TEST_REPO_ASSET_ROOT="${PROJECT_SOURCE_DIR}/tests/assets"
            CONJURE_TEST_DOWNLOADED_ASSET_ROOT="${PROJECT_SOURCE_DIR}/tests/.downloaded"
            CONJURE_TEST_OUTPUT_ROOT="${PROJECT_BINARY_DIR}/test_output/${name}"
            CONJURE_TEST_TEMP_ROOT="${PROJECT_BINARY_DIR}/test_tmp/${name}"
            $<$<BOOL:${CONJURE_TEST_GOLDEN_IMAGES}>:CONJURE_TEST_GOLDEN_IMAGES=1>
    )

    set_target_properties(${name} PROPERTIES FOLDER "${ARG_FOLDER}")
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${ARG_SOURCES})
    source_group("support" FILES ${mainSources})
endfunction()


function(conjure_add_test_module module)
    set(options ENGINE LINK_ENGINE_SHARED)
    set(oneValueArgs TIMEOUT)
    set(multiValueArgs SOURCES DEPS LABELS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "conjure_add_test_module(${module}): unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "conjure_add_test_module(${module}): SOURCES is required")
    endif()
    if(NOT ARG_TIMEOUT)
        set(ARG_TIMEOUT ${CONJURE_TEST_DEFAULT_TIMEOUT})
    endif()
    if(ARG_ENGINE)
        message(VERBOSE "conjure_add_test_module(${module}): ENGINE accepted; conjure::test_support_engine does not exist yet")
    endif()

    set(target "conjure_${module}_tests")
    set(moduleDeps "")
    set(linkOption "")
    if(ARG_LINK_ENGINE_SHARED)
        # The static conjure::<module> is inside conjure_shared; linking both would duplicate every
        # engine global (07_modules.md D1).
        set(moduleDeps conjure::shared)
        set(linkOption LINK_ENGINE_SHARED)
    elseif(TARGET conjure::${module})
        set(moduleDeps conjure::${module})
    endif()

    conjure_add_test_executable(${target}
        SOURCES ${ARG_SOURCES}
        DEPS ${moduleDeps} ${ARG_DEPS}
        ${linkOption}
    )

    # One CTest entry per TEST_CASE (08_testing D1). TIMEOUT and SKIP_RETURN_CODE travel through
    # PROPERTIES; the label list cannot (doctestAddTests.cmake flattens nested lists), so it is
    # applied by a second TEST_INCLUDE_FILES script that runs after doctest's and uses the
    # <target>_TESTS variable doctest leaves behind.
    doctest_discover_tests(${target}
        TEST_PREFIX "${module}."
        EXTRA_ARGS --conjure-isolated
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
        PROPERTIES TIMEOUT ${ARG_TIMEOUT} SKIP_RETURN_CODE 77
    )

    set(labels "unit;${module}")
    if(ARG_LABELS)
        list(APPEND labels ${ARG_LABELS})
    endif()
    list(REMOVE_DUPLICATES labels)

    # Inside the generated script the quoted label list stays one argument, semicolons included.
    set(labelsFile "${CMAKE_CURRENT_BINARY_DIR}/${target}_labels.cmake")
    file(WRITE "${labelsFile}"
        "if(DEFINED ${target}_TESTS)\n"
        "    set_tests_properties(\${${target}_TESTS} PROPERTIES LABELS \"${labels}\")\n"
        "endif()\n"
    )
    set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES "${labelsFile}")
endfunction()


function(conjure_header_check target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "conjure_header_check(${target}): no such target")
    endif()

    get_target_property(headers ${target} CONJURE_PUBLIC_HEADERS)
    if(NOT headers)
        message(VERBOSE "conjure_header_check(${target}): no public headers registered")
        return()
    endif()

    set(includeRoot "${PROJECT_SOURCE_DIR}/engine/include")
    set(checkDir "${CMAKE_CURRENT_BINARY_DIR}/header_check/${target}")
    set(sources "")
    set(index 0)
    foreach(header IN LISTS headers)
        file(RELATIVE_PATH relative "${includeRoot}" "${header}")
        if(relative MATCHES "^\\.\\.")
            message(FATAL_ERROR "conjure_header_check(${target}): ${header} is not under ${includeRoot}")
        endif()
        get_filename_component(stem "${header}" NAME_WE)
        set(source "${checkDir}/${index}_${stem}.cpp")
        # Written through a staging file: file(WRITE) always touches its output and would make
        # every header-check object recompile after each configure, while configure_file(COPYONLY)
        # only replaces the stub when its content changed.
        set(staged "${checkDir}/${index}_${stem}.cpp.in")
        file(WRITE "${staged}"
            "// Generated by conjure_header_check: the header must compile on its own.\n"
            "#include \"${relative}\"\n"
        )
        configure_file("${staged}" "${source}" COPYONLY)
        list(APPEND sources "${source}")
        math(EXPR index "${index} + 1")
    endforeach()

    set(checkTarget "${target}_header_check")
    add_library(${checkTarget} OBJECT ${sources})
    target_link_libraries(${checkTarget}
        PRIVATE
            ${target}
            conjure::platform_defines
            conjure::warnings
            conjure::sanitizers
    )
    set_target_properties(${checkTarget} PROPERTIES FOLDER "Tests/HeaderChecks")

    if(NOT TARGET conjure_header_check)
        add_custom_target(conjure_header_check ALL)
        set_target_properties(conjure_header_check PROPERTIES FOLDER "Tests/HeaderChecks")
    endif()
    add_dependencies(conjure_header_check ${checkTarget})
endfunction()
