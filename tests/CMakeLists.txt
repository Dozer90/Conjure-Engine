# Unit, integration and benchmark tests. Every engine module has a matching directory here; a
# directory is only built when its module target exists (the directory's own CMakeLists.txt
# checks that), so disabling a subsystem also disables its tests. The root enters this directory for
# CONJURE_BUILD_TESTS or CONJURE_BUILD_BENCHMARKS: the support library serves both, the test modules, the
# style, documentation and tool entries and the build entries need the first, the benchmarks the second.

add_subdirectory(support)

if(CONJURE_BUILD_BENCHMARKS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/CMakeLists.txt")
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
    if(EXISTS "${testDir}/CMakeLists.txt")
        add_subdirectory("${testDir}")
    endif()
endforeach()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/CMakeLists.txt")
    add_subdirectory(integration)
endif()

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
