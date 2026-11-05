# conjure_test_support: the core-level test support library (docs/design/08_testing.md D2, 2.2, 4.1-4.3
# and docs/design/10_core.md 3). Depends on conjure::core only; never installed or shipped.
#
# Public headers live under include/conjure/testing/. test_main.cpp and tracked_global_new.cpp are
# deliberately not part of the library: conjure_add_test_module() compiles them into every test
# executable (cmake/ConjureTesting.cmake, CONJURE_TEST_MAIN_SOURCES).

set(CONJURE_TEST_SUPPORT_HEADERS
    include/conjure/testing/allocation_scope.h
    include/conjure/testing/approx.h
    include/conjure/testing/assert_capture.h
    include/conjure/testing/doctest.h
    include/conjure/testing/log_capture.h
    include/conjure/testing/memory_guard.h
    include/conjure/testing/temp_directory.h
    include/conjure/testing/test_assets.h
    include/conjure/testing/test_environment.h
)

add_library(conjure_test_support STATIC
    ${CONJURE_TEST_SUPPORT_HEADERS}
    src/allocation_scope.cpp
    src/approx.cpp
    src/assert_capture.cpp
    src/log_capture.cpp
    src/memory_guard.cpp
    src/temp_directory.cpp
    src/test_assets.cpp
    src/test_environment.cpp
)
add_library(conjure::test_support ALIAS conjure_test_support)

target_include_directories(conjure_test_support
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(conjure_test_support
    PUBLIC
        conjure::core
        conjure::thirdparty::doctest
        conjure::platform_defines
    PRIVATE
        conjure::warnings
        conjure::sanitizers
)
set_target_properties(conjure_test_support PROPERTIES FOLDER "Tests")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${CONJURE_TEST_SUPPORT_HEADERS})

add_subdirectory(tests)
