# Engine modules, in dependency order. A module directory that does not exist yet is skipped so
# the build keeps working while modules are being brought online wave by wave. Each module's own
# CMakeLists.txt is responsible for honouring its CONJURE_WITH_* option.

set(CONJURE_ENGINE_MODULES
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
)

foreach(module IN LISTS CONJURE_ENGINE_MODULES)
    set(moduleDir "${CMAKE_CURRENT_SOURCE_DIR}/src/${module}")
    if(EXISTS "${moduleDir}/CMakeLists.txt")
        add_subdirectory("${moduleDir}")
    else()
        message(STATUS "engine: module '${module}' not present yet, skipping")
    endif()
endforeach()
