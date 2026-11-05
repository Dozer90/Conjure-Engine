# core/system tests. Besides the test executable this builds three helpers the tests exercise:
#   conjure_process_child           - a tiny program process tests spawn (echo, exit codes, sleep, ...)
#   conjure_test_shared_library     - a tiny shared library dynamic_library tests load
#   conjure_test_dependent_library  - a shared library that imports conjure_test_shared_library
# All land in the same bin directory as the tests, which locate them through their build paths.

conjure_add_executable(conjure_process_child
    SOURCES
        process_child.cpp
    FOLDER "Tests"
)
target_include_directories(conjure_process_child PRIVATE "${PROJECT_SOURCE_DIR}/engine/include")

add_library(conjure_test_shared_library SHARED test_shared_library.cpp)
target_include_directories(conjure_test_shared_library PRIVATE "${PROJECT_SOURCE_DIR}/engine/include")
target_link_libraries(conjure_test_shared_library
    PRIVATE
        conjure::platform_defines
        conjure::warnings
        conjure::sanitizers
)
set_target_properties(conjure_test_shared_library PROPERTIES FOLDER "Tests")

add_library(conjure_test_dependent_library SHARED test_dependent_library.cpp)
target_include_directories(conjure_test_dependent_library PRIVATE "${PROJECT_SOURCE_DIR}/engine/include")
target_link_libraries(conjure_test_dependent_library
    PRIVATE
        conjure_test_shared_library
        conjure::platform_defines
        conjure::warnings
        conjure::sanitizers
)
set_target_properties(conjure_test_dependent_library PROPERTIES FOLDER "Tests")

# The dependent library must find its dependency next to itself and nowhere else, so copies of
# the pair load from any directory and a lone copy fails: Windows does that through
# dynamic_library::load, Linux through an $ORIGIN run path and macOS through the dependency's
# @loader_path install name. Both libraries therefore live outside the application directory
# (which every loader searches) in a subdirectory of bin.
set_target_properties(conjure_test_shared_library conjure_test_dependent_library PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/core_system_libraries"
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/core_system_libraries"
)
if(APPLE)
    set_target_properties(conjure_test_shared_library PROPERTIES
        INSTALL_NAME_DIR "@loader_path"
        BUILD_WITH_INSTALL_NAME_DIR TRUE
    )
elseif(NOT WIN32)
    set_target_properties(conjure_test_dependent_library PROPERTIES BUILD_RPATH_USE_ORIGIN TRUE)
endif()

conjure_add_test_module(core_system
    SOURCES
        test_dynamic_library.cpp
        test_image.cpp
        test_process.cpp
    DEPS
        conjure::thirdparty::stb
)

add_dependencies(conjure_core_system_tests
    conjure_process_child
    conjure_test_shared_library
    conjure_test_dependent_library
)
target_compile_definitions(conjure_core_system_tests PRIVATE
    CONJURE_PROCESS_CHILD_PATH="$<PATH:CMAKE_PATH,NORMALIZE,$<TARGET_FILE:conjure_process_child>>"
    CONJURE_TEST_SHARED_LIBRARY_PATH="$<PATH:CMAKE_PATH,NORMALIZE,$<TARGET_FILE:conjure_test_shared_library>>"
    CONJURE_TEST_DEPENDENT_LIBRARY_PATH="$<PATH:CMAKE_PATH,NORMALIZE,$<TARGET_FILE:conjure_test_dependent_library>>"
)
