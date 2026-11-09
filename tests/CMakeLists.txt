# Unit, integration and benchmark tests. Every engine module has a matching directory here; a
# directory is only built when its module target exists (the directory's own CMakeLists.txt
# checks that), so disabling a subsystem also disables its tests.

add_subdirectory(support)

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
    editor
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

if(CONJURE_BUILD_BENCHMARKS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/CMakeLists.txt")
    add_subdirectory(benchmarks)
endif()

# Style check (docs/design/08_testing.md 7.3 style-and-headers job): tools/check_style.py over every
# C++ file of the repository plus the test-mirrors-header rule, registered as the CTest entry
# "style.check" with label "style". Skipped with a notice when no Python interpreter is available.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME style.check
        COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tools/check_style.py" --tests-mirror-headers
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
    set_tests_properties(style.check PROPERTIES LABELS "style" TIMEOUT 120)

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
else()
    message(STATUS "tests: Python 3 not found, the style.check test is not registered")
endif()
