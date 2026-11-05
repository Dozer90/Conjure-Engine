# Self-tests of the test support library (docs/design/08_testing.md 11): conjure_support_tests.
#
# conjure_support_child is a second doctest program the self-tests spawn through core::process
# (support_child_runner.h) to observe behaviour that cannot be asserted in-process: the exit code 77
# of an unmet precondition, a memory_guard, log_capture or gate macro that fails its case, an
# unexpected assert aborting, the start-up banner. It is never registered with CTest.

conjure_add_test_executable(conjure_support_child
    SOURCES
        support_child.cpp
)

conjure_add_test_module(support
    SOURCES
        support_child_runner.h
        test_allocation_scope.cpp
        test_approx.cpp
        test_assert_capture.cpp
        test_doctest.cpp
        test_log_capture.cpp
        test_memory_guard.cpp
        test_temp_directory.cpp
        test_test_assets.cpp
        test_test_environment.cpp
    DEPS
        conjure::core
    LABELS
        support
)

add_dependencies(conjure_support_tests conjure_support_child)
target_compile_definitions(conjure_support_tests PRIVATE
    CONJURE_SUPPORT_CHILD_PATH="$<PATH:CMAKE_PATH,NORMALIZE,$<TARGET_FILE:conjure_support_child>>"
)
