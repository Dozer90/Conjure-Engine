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
)
add_dependencies(conjure_modules_tests ${fixtureTargets})

conjure_header_check(conjure_modules)
add_dependencies(conjure_modules_header_check conjure_abi_identity)
