# Core tests: one test executable per core area (conjure_core_<area>_tests), so an area can be built
# and tested without the rest of core being complete. The area list mirrors engine/src/core.

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
    if(EXISTS "${testDir}/CMakeLists.txt" AND TARGET conjure::core_${area})
        add_subdirectory("${testDir}")
    endif()
endforeach()
