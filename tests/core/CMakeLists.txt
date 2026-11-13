# Core tests: one test executable per core area (conjure_core_<area>_tests), so an area can be built
# and tested without the rest of core being complete. The area list mirrors engine/src/core; a listed area
# without its test directory stops the configure, and an area's tests are added when its library exists.

set(CONJURE_CORE_TEST_AREAS
    foundation
    math
    containers
    strings
    memory
    time
    jobs
    json
    vfs
    system
    reflect
)

foreach(area IN LISTS CONJURE_CORE_TEST_AREAS)
    set(testDir "${CMAKE_CURRENT_SOURCE_DIR}/${area}")
    if(NOT EXISTS "${testDir}/CMakeLists.txt")
        message(FATAL_ERROR
            "core tests: CONJURE_CORE_TEST_AREAS lists '${area}' but ${testDir}/CMakeLists.txt does not exist")
    endif()
    if(TARGET conjure::core_${area})
        add_subdirectory("${testDir}")
    endif()

    # Every public header of the area must compile on its own (build-only check, part of ALL).
    if(TARGET conjure_core_${area})
        conjure_header_check(conjure_core_${area})
    endif()
endforeach()
