# conjure_modules_tests (docs/design/07_modules.md 11).
#
# The module tests link the umbrella shared library conjure::shared instead of the static libraries
# (conjure_add_test_module LINK_ENGINE_SHARED, cmake/ConjureTesting.cmake), because the fixture
# plugins link it too and the suite must prove that host and plugins share one engine copy (one
# logger, one string_id table). The executable therefore keeps the C runtime's allocation
# functions: memory_guard and allocation_scope count nothing here and the suite asserts "no growth"
# through the registry (record count, generations) instead.
#
# Fixture plugins are built with conjure_add_game_module() into ${PROJECT_BINARY_DIR}/test_modules/
# <name>/, each with a copy of its module.json, so the tree a test discovers looks exactly like a
# project's modules/ directory and the source tree stays free of build output.
#
# Export checks (07_modules.md 5, ARCHITECTURE.md 9; tools/check_exports.py), registered here because
# they prove the umbrella the module tests and fixtures load through:
#   exports.check_annotations     every public declaration that needs CONJURE_API carries it
#   exports.count_shared_library  conjure_shared exports at most 52428 symbols (dumpbin / nm)
#   exports.check_tool            the checker's own self-test

if(NOT TARGET conjure::modules OR NOT TARGET conjure::shared)
    return()
endif()

#------------------------------------------------------------------------------
# Fixture plugins
#------------------------------------------------------------------------------
set(CONJURE_TEST_MODULE_DIR "${PROJECT_BINARY_DIR}/test_modules")
set(fixtureSourceDir "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
set(fixtureTargets "")

# Every fixture, binary or not, gets its manifest copied so discover() sees the whole set.
set(fixtureNames
    module_alpha
    module_beta
    module_bad_abi
    module_bad_version
    module_wrong_name
    module_bad_entry
    module_no_symbol
    module_managed_only
    module_wrong_platform
    module_cycle_a
    module_cycle_b
)
foreach(fixture IN LISTS fixtureNames)
    configure_file(
        "${fixtureSourceDir}/${fixture}/module.json"
        "${CONJURE_TEST_MODULE_DIR}/${fixture}/module.json"
        COPYONLY
    )
endforeach()
# A managed assembly the export test copies; the content is irrelevant.
file(WRITE "${CONJURE_TEST_MODULE_DIR}/module_managed_only/managed/ManagedOnly.dll.in" "not a real assembly\n")
configure_file(
    "${CONJURE_TEST_MODULE_DIR}/module_managed_only/managed/ManagedOnly.dll.in"
    "${CONJURE_TEST_MODULE_DIR}/module_managed_only/managed/ManagedOnly.dll"
    COPYONLY
)

function(conjure_modules_add_fixture name)
    set(options "")
    set(oneValueArgs NAME)
    set(multiValueArgs DEFINES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(nameArgument "")
    if(ARG_NAME)
        set(nameArgument NAME "${ARG_NAME}")
    endif()
    conjure_add_game_module(conjure_fixture_${name}
        MANIFEST "${fixtureSourceDir}/${name}/module.json"
        OUTPUT_ROOT "${CONJURE_TEST_MODULE_DIR}/${name}"
        SOURCES "${fixtureSourceDir}/${name}/${name}.cpp"
        DEFINES ${ARG_DEFINES}
        ${nameArgument}
    )
    target_include_directories(conjure_fixture_${name} PRIVATE "${fixtureSourceDir}")
    set_target_properties(conjure_fixture_${name} PROPERTIES FOLDER "Tests/ModuleFixtures")
    set(fixtureTargets ${fixtureTargets} conjure_fixture_${name} PARENT_SCOPE)
endfunction()

conjure_modules_add_fixture(module_alpha)
conjure_modules_add_fixture(module_beta)
conjure_modules_add_fixture(module_bad_abi DEFINES CONJURE_TEST_ABI_VERSION_OVERRIDE=999)
conjure_modules_add_fixture(module_bad_version DEFINES CONJURE_TEST_ENGINE_VERSION_OVERRIDE=9000)
conjure_modules_add_fixture(module_wrong_name NAME module_wrong_name_actual)
conjure_modules_add_fixture(module_bad_entry)
conjure_modules_add_fixture(module_no_symbol)

# module_surface links a representative slice of the public engine surface through the import side of
# conjure_shared (07_modules.md 5): it fails to link whenever a CONJURE_API annotation goes missing,
# independently of tools/check_exports.py. It lives in its own root so discover() over the fixture
# tree above keeps its record count; the test adds its manifest explicitly.
set(CONJURE_TEST_SURFACE_MODULE_DIR "${PROJECT_BINARY_DIR}/test_modules_surface")
set(surfaceRenderer 0)
if(TARGET conjure::renderer)
    set(surfaceRenderer 1)
endif()
configure_file(
    "${fixtureSourceDir}/module_surface/module.json"
    "${CONJURE_TEST_SURFACE_MODULE_DIR}/module_surface/module.json"
    COPYONLY
)
conjure_add_game_module(conjure_fixture_module_surface
    MANIFEST "${fixtureSourceDir}/module_surface/module.json"
    OUTPUT_ROOT "${CONJURE_TEST_SURFACE_MODULE_DIR}/module_surface"
    SOURCES "${fixtureSourceDir}/module_surface/module_surface.cpp"
    DEFINES CONJURE_TEST_SURFACE_RENDERER=${surfaceRenderer}
)
set_target_properties(conjure_fixture_module_surface PROPERTIES FOLDER "Tests/ModuleFixtures")
list(APPEND fixtureTargets conjure_fixture_module_surface)

#------------------------------------------------------------------------------
# Test executable
#------------------------------------------------------------------------------
conjure_add_test_module(modules
    SOURCES
        test_dynamic_library.cpp
        test_module_abi.cpp
        test_module_consumer.cpp
        test_module_context.cpp
        test_module_entry.cpp
        test_module_error.cpp
        test_module_events.cpp
        test_module_export_layout.cpp
        test_module_lifecycle_order.cpp
        test_module_manager.cpp
        test_module_manager_shared.cpp
        test_module_manager_static.cpp
        test_module_manifest.cpp
        test_module_registrar.cpp
        test_module_registration_set.cpp
        test_module_registry.cpp
        test_module_reload.cpp
        test_module_requests.cpp
        test_module_resolver.cpp
        test_module_service.cpp
        test_module_shadow_copy.cpp
        test_module_types.cpp
        test_module_version.cpp
        test_modules.cpp
    LINK_ENGINE_SHARED
)

target_include_directories(conjure_modules_tests PRIVATE "${fixtureSourceDir}")
target_compile_definitions(conjure_modules_tests PRIVATE
    CONJURE_TEST_MODULE_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${CONJURE_TEST_MODULE_DIR}>"
    CONJURE_TEST_SURFACE_MODULE_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${CONJURE_TEST_SURFACE_MODULE_DIR}>"
    CONJURE_TEST_SURFACE_RENDERER=${surfaceRenderer}
)
add_dependencies(conjure_modules_tests ${fixtureTargets})

conjure_header_check(conjure_modules)
add_dependencies(conjure_modules_header_check conjure_abi_identity)

#------------------------------------------------------------------------------
# Export checks
#------------------------------------------------------------------------------
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    set(checkExports "${PROJECT_SOURCE_DIR}/tools/check_exports.py")

    add_test(NAME exports.check_annotations
        COMMAND Python3::Interpreter -I "${checkExports}"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
    add_test(NAME exports.check_tool
        COMMAND Python3::Interpreter -I "${checkExports}" --self-test
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )

    # The export-listing tool of the toolchain that linked conjure_shared: dumpbin sits beside the
    # MSVC linker, GCC and Clang toolchains provide nm (CMAKE_NM). Without either the script searches
    # PATH and fails with exit code 2 when nothing is found.
    set(exportToolArguments "")
    if(MSVC)
        get_filename_component(linkerDirectory "${CMAKE_LINKER}" DIRECTORY)
        if(EXISTS "${linkerDirectory}/dumpbin.exe")
            set(exportToolArguments --tool "${linkerDirectory}/dumpbin.exe")
        endif()
    elseif(CMAKE_NM)
        set(exportToolArguments --tool "${CMAKE_NM}")
    endif()
    add_test(NAME exports.count_shared_library
        COMMAND Python3::Interpreter -I "${checkExports}"
            --count-exports "$<TARGET_FILE:conjure_shared>" --limit 52428 ${exportToolArguments}
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )

    set_tests_properties(exports.check_annotations exports.check_tool exports.count_shared_library
        PROPERTIES LABELS "exports;style" TIMEOUT 120
    )
else()
    message(STATUS "tests/modules: Python 3 not found, the exports.* tests are not registered")
endif()
