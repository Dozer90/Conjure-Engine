# conjure_test_support: the core-level test support library (docs/design/08_testing.md D2, 2.2, 4.1-4.3
# and docs/design/10_core.md 3). Depends on conjure::core only; never installed or shipped.
#
# Public headers live under include/conjure/testing/ and are header-checked (conjure_header_check: each
# one compiles alone, like an engine module's). test_main.cpp and tracked_global_new.cpp are
# deliberately not part of the library: conjure_add_test_module() compiles them into every test
# executable (cmake/ConjureTesting.cmake, CONJURE_TEST_MAIN_SOURCES).
#
# conjure_test_support_shared is the same library compiled against the umbrella conjure::shared
# instead of the static core libraries, for the LINK_ENGINE_SHARED test modules (07_modules.md D1):
# an executable that links conjure_shared must not also link a second copy of core through the
# support library.

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

set(CONJURE_TEST_SUPPORT_SOURCES
    src/allocation_scope.cpp
    src/approx.cpp
    src/assert_capture.cpp
    src/log_capture.cpp
    src/memory_guard.cpp
    src/temp_directory.cpp
    src/test_assets.cpp
    src/test_environment.cpp
)

# conjure_add_test_support_library(<target> <alias> <engine library>)
function(conjure_add_test_support_library target alias engineLibrary)
    add_library(${target} STATIC ${CONJURE_TEST_SUPPORT_HEADERS} ${CONJURE_TEST_SUPPORT_SOURCES})
    add_library(${alias} ALIAS ${target})

    target_include_directories(${target}
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    target_link_libraries(${target}
        PUBLIC
            ${engineLibrary}
            conjure::thirdparty::doctest
            conjure::platform_defines
        PRIVATE
            conjure::warnings
            conjure::sanitizers
    )
    # Registered like a module's public headers so conjure_header_check() compiles each one alone.
    set(absoluteHeaders "")
    foreach(header IN LISTS CONJURE_TEST_SUPPORT_HEADERS)
        list(APPEND absoluteHeaders "${CMAKE_CURRENT_SOURCE_DIR}/${header}")
    endforeach()
    set_property(TARGET ${target} PROPERTY CONJURE_PUBLIC_HEADERS "${absoluteHeaders}")

    set_target_properties(${target} PROPERTIES FOLDER "Tests")
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${CONJURE_TEST_SUPPORT_HEADERS} ${CONJURE_TEST_SUPPORT_SOURCES})
endfunction()

conjure_add_test_support_library(conjure_test_support conjure::test_support conjure::core)
conjure_header_check(conjure_test_support INCLUDE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(TARGET conjure::shared)
    conjure_add_test_support_library(conjure_test_support_shared conjure::test_support_shared conjure::shared)
endif()

# The engine-level support library (08_testing.md D2, wave 4) lives in engine/ and skips itself when the
# engine module is not built.
add_subdirectory(engine)

add_subdirectory(tests)
