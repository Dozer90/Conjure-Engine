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
