# Self-tests of the engine-level test support library (docs/design/08_testing.md 11):
# conjure_support_engine_tests, one test_<header>.cpp per public header of tests/support/engine. Everything
# is headless: the null window, the null rhi device, the null audio backend, in-memory and temp-directory
# file systems. The protocol client is exercised against an in-memory scripted channel.
#
# conjure_support_engine_child is a second doctest program the self-tests spawn through core::process
# (support_engine_child_runner.h) to observe a headless_engine failing its case from a constructor, a
# destructor or step_frames: the exit code, the failure text and that each failure is reported exactly once.
# It is never registered with CTest.

if(NOT TARGET conjure::test_support_engine)
    message(STATUS "tests/support/tests/engine: conjure::test_support_engine is not built, self-tests skipped")
    return()
endif()

set(CONJURE_SUPPORT_ENGINE_TEST_SOURCES
    support_engine_child_runner.h
    test_golden_image.cpp
    test_headless_engine.cpp
    test_input_script.cpp
    test_log_category.cpp
    test_protocol_client.cpp
    test_world_builder.cpp
)

set(CONJURE_SUPPORT_ENGINE_TEST_DEPS
    conjure::test_support_engine
    conjure::engine
    conjure::ecs
    conjure::platform
    conjure::rhi
    conjure::scene
)
foreach(optionalModule IN ITEMS renderer physics audio scripting)
    if(TARGET conjure::${optionalModule})
        list(APPEND CONJURE_SUPPORT_ENGINE_TEST_DEPS conjure::${optionalModule})
    endif()
endforeach()

conjure_add_test_executable(conjure_support_engine_child
    SOURCES
        support_engine_child.cpp
    DEPS
        ${CONJURE_SUPPORT_ENGINE_TEST_DEPS}
)

conjure_add_test_module(support_engine
    ENGINE
    SOURCES
        ${CONJURE_SUPPORT_ENGINE_TEST_SOURCES}
    DEPS
        ${CONJURE_SUPPORT_ENGINE_TEST_DEPS}
    LABELS
        support
    TIMEOUT 300
)

add_dependencies(conjure_support_engine_tests conjure_support_engine_child)

# The golden update-mode cases run this executable again with --conjure-update-golden (the flag is a
# process-wide setting test_main parses, so it can only be observed from a second process); the fixture
# failure cases run the child.
target_compile_definitions(conjure_support_engine_tests PRIVATE
    CONJURE_SUPPORT_ENGINE_TESTS_PATH="$<PATH:CMAKE_PATH,NORMALIZE,$<TARGET_FILE:conjure_support_engine_tests>>"
    CONJURE_SUPPORT_ENGINE_CHILD_PATH="$<PATH:CMAKE_PATH,NORMALIZE,$<TARGET_FILE:conjure_support_engine_child>>"
)

# The managed SDK the library's DOTNET_RUNTIME probe looks for (CONJURE_TEST_MANAGED_DIR is private to the
# library), so the probe case can compute its expectation on its own instead of trusting the probe.
if(TARGET conjure_managed_sdk AND CONJURE_MANAGED_SDK_DIR)
    target_compile_definitions(conjure_support_engine_tests PRIVATE
        CONJURE_SUPPORT_ENGINE_TEST_MANAGED_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${CONJURE_MANAGED_SDK_DIR}>"
    )
endif()
