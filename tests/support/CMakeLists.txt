# Shared test support: doctest main, log capture, assert capture, temporary directories.

add_library(conjure_test_support STATIC
    conjure_test.h
    conjure_test.cpp
    test_main.cpp
)
add_library(conjure::test_support ALIAS conjure_test_support)

target_include_directories(conjure_test_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(conjure_test_support
    PUBLIC
        conjure::core_foundation
        conjure::thirdparty::doctest
        conjure::platform_defines
    PRIVATE
        conjure::warnings
        conjure::sanitizers
)
set_target_properties(conjure_test_support PROPERTIES FOLDER "Tests")
