# Engine modules, in dependency order. Every listed module must have its directory: a missing
# CMakeLists.txt stops the configure instead of surfacing later as unrelated link errors. Each module's own
# CMakeLists.txt is responsible for honouring its CONJURE_WITH_* option; engine/src/editor declares
# conjure_editor_core and conjure_editor_ui and returns early unless CONJURE_BUILD_EDITOR, so the editor
# libraries join conjure_shared like every other module (docs/design/07_modules.md 5) and are absent from
# a Distribution build.

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
    bindings
    editor
)

foreach(module IN LISTS CONJURE_ENGINE_MODULES)
    set(moduleDir "${CMAKE_CURRENT_SOURCE_DIR}/src/${module}")
    if(NOT EXISTS "${moduleDir}/CMakeLists.txt")
        message(FATAL_ERROR
            "engine: CONJURE_ENGINE_MODULES lists '${module}' but ${moduleDir}/CMakeLists.txt does not exist")
    endif()
    add_subdirectory("${moduleDir}")
endforeach()

# The engine's shaders (docs/design/02_renderer.md 3.12): conjure_add_shaders() lives in
# cmake/ConjureShaders.cmake and the HLSL sources under engine/shaders. The target conjure_shaders
# is independent of any module directory; conjure_rhi_tests and (wave 3) conjure_renderer depend on it.
if(CONJURE_WITH_RENDERER)
    include(ConjureShaders)
    add_subdirectory(shaders)
endif()
