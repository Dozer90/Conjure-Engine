# Unit, integration and benchmark tests. Every engine module has a matching directory here, and a listed
# directory without its CMakeLists.txt stops the configure; a directory builds its tests only when its module
# target exists (the directory's own CMakeLists.txt checks that), so disabling a subsystem also disables its
# tests. The root enters this directory for CONJURE_BUILD_TESTS or CONJURE_BUILD_BENCHMARKS: the support library
# serves both, the test modules, the style, documentation and tool entries and the build entries need the first,
# the benchmarks the second.

add_subdirectory(support)

if(CONJURE_BUILD_BENCHMARKS)
    set(benchmarksList "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/CMakeLists.txt")
    if(NOT EXISTS "${benchmarksList}")
        message(FATAL_ERROR "tests: CONJURE_BUILD_BENCHMARKS is ON but ${benchmarksList} does not exist")
    endif()
    add_subdirectory(benchmarks)
endif()

if(NOT CONJURE_BUILD_TESTS)
    return()
endif()

set(CONJURE_TEST_MODULES
    core
    ecs
    platform
    rhi
    assets
    audio
    modules
    scene
    renderer
    physics
    scripting
    engine
    bindings
    editor
    runtime
)

foreach(module IN LISTS CONJURE_TEST_MODULES)
    set(testDir "${CMAKE_CURRENT_SOURCE_DIR}/${module}")
    if(NOT EXISTS "${testDir}/CMakeLists.txt")
        message(FATAL_ERROR
            "tests: CONJURE_TEST_MODULES lists '${module}' but ${testDir}/CMakeLists.txt does not exist")
    endif()
    add_subdirectory("${testDir}")
endforeach()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/CMakeLists.txt")
    message(FATAL_ERROR "tests: ${CMAKE_CURRENT_SOURCE_DIR}/integration/CMakeLists.txt does not exist")
endif()
add_subdirectory(integration)

# Style check (docs/design/08_testing.md 7.3 style-and-headers job): tools/check_style.py over every
# C++ file of the repository plus the test-mirrors-header rule, registered as the CTest entry
# "style.check" with label "style", and the checker's own self-test as "style.check_tool". Skipped with a
# notice when no Python interpreter is available.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME style.check
        COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tools/check_style.py" --tests-mirror-headers
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
    set_tests_properties(style.check PROPERTIES LABELS "style" TIMEOUT 120)
    add_test(NAME style.check_tool
        COMMAND Python3::Interpreter -I "${PROJECT_SOURCE_DIR}/tools/tests/test_check_style.py"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
    set_tests_properties(style.check_tool PROPERTIES LABELS "style" TIMEOUT 120)

    # Documentation checks (docs/design/16_documentation.md D9): structure, badges, links and coverage of
    # the wiki and handbook, plus the checker's own self-test.
    add_test(NAME docs.check
        COMMAND Python3::Interpreter -I "${PROJECT_SOURCE_DIR}/tools/check_docs.py"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
    set_tests_properties(docs.check PROPERTIES LABELS "docs" TIMEOUT 120)
    add_test(NAME docs.check_tool
        COMMAND Python3::Interpreter -I "${PROJECT_SOURCE_DIR}/tools/tests/test_check_docs.py"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
    set_tests_properties(docs.check_tool PROPERTIES LABELS "docs" TIMEOUT 120)

    # Self-tests of the build driver's preset resolution, the build-tree pruner (a fake build/ folder), the
    # test-asset downloader (fake transfers, never the network) and the native API generator's table parser.
    add_test(NAME build.driver_tool
        COMMAND Python3::Interpreter -I "${PROJECT_SOURCE_DIR}/tools/tests/test_build.py"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
    set_tests_properties(build.driver_tool PROPERTIES LABELS "tools" TIMEOUT 120)
    add_test(NAME build.prune_tool
        COMMAND Python3::Interpreter -I "${PROJECT_SOURCE_DIR}/tools/tests/test_prune_builds.py"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
    set_tests_properties(build.prune_tool PROPERTIES LABELS "tools" TIMEOUT 120)
    add_test(NAME test_assets.downloader_tool
        COMMAND Python3::Interpreter -I "${PROJECT_SOURCE_DIR}/tools/tests/test_download_test_assets.py"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
    set_tests_properties(test_assets.downloader_tool PROPERTIES LABELS "tools" TIMEOUT 120)
    add_test(NAME native_api.generator_tool
        COMMAND Python3::Interpreter -I "${PROJECT_SOURCE_DIR}/tools/tests/test_generate_native_api.py"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
    set_tests_properties(native_api.generator_tool PROPERTIES LABELS "tools" TIMEOUT 120)

    # The editor's application icon (docs/handbook/brand.md): the .ico and the embedded PNGs must match what
    # tools/generate_brand_icons.py renders from the logo drawings, plus the generator's own self-test.
    add_test(NAME brand.icons_up_to_date
        COMMAND Python3::Interpreter -I "${PROJECT_SOURCE_DIR}/tools/generate_brand_icons.py" --check
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
    set_tests_properties(brand.icons_up_to_date PROPERTIES LABELS "generated" TIMEOUT 120)
    add_test(NAME brand.icons_tool
        COMMAND Python3::Interpreter -I "${PROJECT_SOURCE_DIR}/tools/tests/test_generate_brand_icons.py"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
    set_tests_properties(brand.icons_tool PROPERTIES LABELS "generated" TIMEOUT 120)
else()
    message(STATUS "tests: Python 3 not found, the style, docs and tool self-test entries are not registered")
endif()

# The ABI identity script (cmake/ConjureSharedLibrary.cmake, script mode) over a header list naming a path with a
# non-ASCII character, as a checkout under a user directory with an accented name produces: the entry fails when
# the list is read back truncated at that character. The probe directory is "Jos" followed by U+00E9, spelled as
# its UTF-8 bytes 0xC3 0xA9 so this file stays ASCII.
set(abiProbeRoot "${CMAKE_CURRENT_BINARY_DIR}/abi_identity_probe")
string(ASCII 195 169 abiProbeAccent)
set(abiProbeHeader "${abiProbeRoot}/include/Jos${abiProbeAccent}/probe.h")
file(WRITE "${abiProbeHeader}" "#pragma once\n")
file(WRITE "${abiProbeRoot}/headers.txt" "${abiProbeHeader}\n")
add_test(NAME build.abi_identity_non_ascii_path
    COMMAND "${CMAKE_COMMAND}"
        "-DCONJURE_ABI_IDENTITY_OUTPUT=${abiProbeRoot}/abi_identity.h"
        "-DCONJURE_ABI_IDENTITY_HEADER_LIST=${abiProbeRoot}/headers.txt"
        "-DCONJURE_ABI_IDENTITY_INCLUDE_ROOT=${abiProbeRoot}/include"
        -DCONJURE_ABI_IDENTITY_COMPILER_MAJOR=0
        -DCONJURE_ABI_IDENTITY_DYNAMIC_CRT=1
        -DCONJURE_ABI_IDENTITY_ASAN=0
        -DCONJURE_ABI_IDENTITY_UBSAN=0
        -P "${PROJECT_SOURCE_DIR}/cmake/ConjureSharedLibrary.cmake"
)
set_tests_properties(build.abi_identity_non_ascii_path PROPERTIES LABELS "build" TIMEOUT 60)

# The <ProjectReference> parser of cmake/ConjureManaged.cmake (conjure_managed_project_references), which feeds the
# managed stamp dependencies and the source-tree check, through that check in script mode: App.csproj references
# ..\Lib\Lib.csproj (relative, with backslashes) and Lib/ holds an obj/, so the check must name Lib/obj. The entry
# fails when the parser returns the reference unresolved or resolves it against anything but the project's directory,
# and, where a backslash is not a path separator (Linux, macOS), when it loses the backslash conversion. CMake wraps
# the error text at spaces (a build tree under "Git Repos" splits the path), so the pattern lets the path start on a
# later line.
set(managedProbeRoot "${CMAKE_CURRENT_BINARY_DIR}/managed_reference_probe")
file(WRITE "${managedProbeRoot}/App/App.csproj"
    "<Project><ItemGroup><ProjectReference Include=\"..\\Lib\\Lib.csproj\" /></ItemGroup></Project>\n"
)
file(WRITE "${managedProbeRoot}/Lib/Lib.csproj" "")
file(MAKE_DIRECTORY "${managedProbeRoot}/Lib/obj")
add_test(NAME build.managed_project_references
    COMMAND "${CMAKE_COMMAND}"
        "-DCONJURE_MANAGED_CHECK_PROJECT=${managedProbeRoot}/App/App.csproj"
        -DCONJURE_MANAGED_CHECK_CLEAN=ON
        -P "${PROJECT_SOURCE_DIR}/cmake/ConjureManaged.cmake"
)
set_tests_properties(build.managed_project_references PROPERTIES
    LABELS "build"
    TIMEOUT 60
    PASS_REGULAR_EXPRESSION "managed build outputs in the source tree:.*/managed_reference_probe/Lib/obj"
)

# Configure probes: a throwaway project (LANGUAGES NONE, so no compiler is involved), configured afresh on every run,
# that includes one list file of the build with a probe directory as its source directory. The missing-directory
# entries pass only when the configure stops with the FATAL_ERROR naming the list or option and the missing path: the
# "missing" probe has no module, area, test or benchmarks list file, only the support/ that tests/CMakeLists.txt adds
# first and the foundation/build_info.h.in that engine/src/core/CMakeLists.txt configures before its loop (without
# them those two list files stop earlier on an unrelated error); the "complete" probe has every test module but no
# integration/. Their patterns span lines and avoid spaces because CMake wraps the error text at spaces. The
# link-flags entry includes cmake/ConjureConfigs.cmake and passes only when conjure_derive_config_flags() leaves
# /INCREMENTAL:NO in the Development linker flags whether it derives them (EXE), finds the old /INCREMENTAL in the
# cache (SHARED) or finds them right already (MODULE).
set(configureProbeRoot "${CMAKE_CURRENT_BINARY_DIR}/configure_probe")
set(configureProbeIncluder [=[
cmake_minimum_required(VERSION 3.28)
project(conjure_configure_probe NONE)
include("${CONJURE_PROBE_LIST}")
]=])
file(WRITE "${configureProbeRoot}/missing/CMakeLists.txt" "${configureProbeIncluder}")
file(WRITE "${configureProbeRoot}/missing/foundation/build_info.h.in" "")
file(WRITE "${configureProbeRoot}/missing/support/CMakeLists.txt" "")
file(WRITE "${configureProbeRoot}/complete/CMakeLists.txt" "${configureProbeIncluder}")
foreach(stubDirectory IN LISTS CONJURE_TEST_MODULES ITEMS support)
    file(WRITE "${configureProbeRoot}/complete/${stubDirectory}/CMakeLists.txt" "")
endforeach()
file(WRITE "${configureProbeRoot}/link_flags/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.28)
include("${CONJURE_PROBE_LIST}")
project(conjure_configure_probe NONE)
conjure_derive_config_flags()
foreach(kind IN ITEMS EXE SHARED MODULE)
    if(NOT CMAKE_${kind}_LINKER_FLAGS_DEVELOPMENT STREQUAL "/debug /INCREMENTAL:NO")
        message(FATAL_ERROR "${kind} Development linker flags: '${CMAKE_${kind}_LINKER_FLAGS_DEVELOPMENT}'")
    endif()
endforeach()
message(STATUS "probe: Development links with /INCREMENTAL:NO")
]=])

function(conjure_add_configure_probe name probe listFile pattern)
    add_test(NAME build.${name}
        COMMAND "${CMAKE_COMMAND}" --fresh -S "${configureProbeRoot}/${probe}" -B "${configureProbeRoot}/build/${name}"
            -G "${CMAKE_GENERATOR}" "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
            "-DCONJURE_PROBE_LIST=${listFile}" ${ARGN}
    )
    set_tests_properties(build.${name} PROPERTIES LABELS "build" TIMEOUT 60 PASS_REGULAR_EXPRESSION "${pattern}")
endfunction()

conjure_add_configure_probe(missing_engine_module_fails_configure missing
    "${PROJECT_SOURCE_DIR}/engine/CMakeLists.txt"
    "CONJURE_ENGINE_MODULES.*'core'.*/src/core/CMakeLists.txt"
)
conjure_add_configure_probe(missing_core_area_fails_configure missing
    "${PROJECT_SOURCE_DIR}/engine/src/core/CMakeLists.txt"
    "CONJURE_CORE_AREAS.*'foundation'.*/foundation/CMakeLists.txt"
)
conjure_add_configure_probe(missing_test_module_fails_configure missing
    "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
    "CONJURE_TEST_MODULES.*'core'.*/core/CMakeLists.txt"
    -DCONJURE_BUILD_TESTS=ON
)
conjure_add_configure_probe(missing_benchmarks_fail_configure missing
    "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
    "CONJURE_BUILD_BENCHMARKS.*/benchmarks/CMakeLists.txt"
    -DCONJURE_BUILD_BENCHMARKS=ON
)
conjure_add_configure_probe(missing_integration_fails_configure complete
    "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
    "tests:.*/integration/CMakeLists.txt"
    -DCONJURE_BUILD_TESTS=ON
)
conjure_add_configure_probe(missing_core_test_area_fails_configure missing
    "${CMAKE_CURRENT_SOURCE_DIR}/core/CMakeLists.txt"
    "CONJURE_CORE_TEST_AREAS.*'foundation'.*/foundation/CMakeLists.txt"
)
conjure_add_configure_probe(development_links_without_incremental link_flags
    "${PROJECT_SOURCE_DIR}/cmake/ConjureConfigs.cmake"
    "probe: Development links with /INCREMENTAL:NO"
    "-DCMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO=/debug /INCREMENTAL"
    "-DCMAKE_SHARED_LINKER_FLAGS_DEVELOPMENT=/debug /INCREMENTAL"
    "-DCMAKE_MODULE_LINKER_FLAGS_DEVELOPMENT=/debug /INCREMENTAL:NO"
)
