# conjure_test_support_engine: the engine-level test support library (docs/design/08_testing.md D2, 2.3,
# 4.4-4.6). Depends on conjure::engine (and on the optional renderer, physics, audio and scripting
# libraries when they are compiled); never installed or shipped. Test modules link it through
# conjure_add_test_module(... ENGINE) (cmake/ConjureTesting.cmake).
#
# Public headers live under include/conjure/testing/ next to the core library's. The self-tests are in
# tests/support/tests/engine (conjure_support_engine_tests).
#
# conjure_test_support_engine_shared is the same library compiled against the umbrella conjure::shared and
# conjure::test_support_shared, for the LINK_ENGINE_SHARED test modules (07_modules.md D1): an executable
# that links conjure_shared must not also link a second copy of the engine through the support library.

if(NOT TARGET conjure::engine)
    message(STATUS "tests/support/engine: conjure::engine is not available, conjure_test_support_engine skipped")
    return()
endif()

set(CONJURE_TEST_SUPPORT_ENGINE_HEADERS
    include/conjure/testing/golden_image.h
    include/conjure/testing/headless_engine.h
    include/conjure/testing/input_script.h
    include/conjure/testing/log_category.h
    include/conjure/testing/protocol_client.h
    include/conjure/testing/world_builder.h
)

set(CONJURE_TEST_SUPPORT_ENGINE_SOURCES
    src/golden_image.cpp
    src/headless_engine.cpp
    src/input_script.cpp
    src/log_category.cpp
    src/protocol_client.cpp
    src/world_builder.cpp
)

# The optional engine libraries the fixtures reach directly (null backend statistics, body and script
# spawning). Each is a public dependency so a test module linking ENGINE sees their headers too.
set(CONJURE_TEST_SUPPORT_ENGINE_OPTIONAL_DEPS "")
foreach(optionalModule IN ITEMS renderer physics audio scripting)
    if(TARGET conjure::${optionalModule})
        list(APPEND CONJURE_TEST_SUPPORT_ENGINE_OPTIONAL_DEPS conjure::${optionalModule})
    endif()
endforeach()

# conjure_add_test_support_engine_library(<target> <alias> <support library> <engine libraries...>)
function(conjure_add_test_support_engine_library target alias supportLibrary)
    add_library(${target} STATIC ${CONJURE_TEST_SUPPORT_ENGINE_HEADERS} ${CONJURE_TEST_SUPPORT_ENGINE_SOURCES})
    add_library(${alias} ALIAS ${target})

    target_include_directories(${target}
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    target_link_libraries(${target}
        PUBLIC
            ${supportLibrary}
            ${ARGN}
            conjure::platform_defines
        PRIVATE
            conjure::warnings
            conjure::sanitizers
    )

    # headless_engine runs the renderer on the null device from the shader index conjure_shaders writes
    # (the same arrangement as tests/renderer); without the shader target the renderer stays disabled.
    if(TARGET conjure_shaders)
        add_dependencies(${target} conjure_shaders)
        get_target_property(shaderOutputDir conjure_shaders CONJURE_SHADER_OUTPUT_DIR)
        target_compile_definitions(${target} PRIVATE
            CONJURE_TEST_SHADER_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${shaderOutputDir}>"
        )
    endif()

    # The real .NET host (enableScripting without useNullScriptHost) loads the managed SDK this tree built;
    # the DOTNET_RUNTIME precondition probe looks for Conjure.Managed.dll there and never holds without it.
    if(TARGET conjure_managed_sdk AND CONJURE_MANAGED_SDK_DIR)
        add_dependencies(${target} conjure_managed_sdk)
        target_compile_definitions(${target} PRIVATE
            CONJURE_TEST_MANAGED_DIR="$<PATH:CMAKE_PATH,NORMALIZE,${CONJURE_MANAGED_SDK_DIR}>"
        )
    endif()

    # Registered like a module's public headers so conjure_header_check() compiles each one alone.
    set(absoluteHeaders "")
    foreach(header IN LISTS CONJURE_TEST_SUPPORT_ENGINE_HEADERS)
        list(APPEND absoluteHeaders "${CMAKE_CURRENT_SOURCE_DIR}/${header}")
    endforeach()
    set_property(TARGET ${target} PROPERTY CONJURE_PUBLIC_HEADERS "${absoluteHeaders}")

    set_target_properties(${target} PROPERTIES FOLDER "Tests")
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}"
        FILES ${CONJURE_TEST_SUPPORT_ENGINE_HEADERS} ${CONJURE_TEST_SUPPORT_ENGINE_SOURCES}
    )
endfunction()

conjure_add_test_support_engine_library(conjure_test_support_engine conjure::test_support_engine
    conjure::test_support
    conjure::engine
    conjure::assets
    conjure::ecs
    conjure::platform
    conjure::rhi
    conjure::scene
    ${CONJURE_TEST_SUPPORT_ENGINE_OPTIONAL_DEPS}
)
conjure_header_check(conjure_test_support_engine INCLUDE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/include")

if(TARGET conjure::shared AND TARGET conjure::test_support_shared)
    conjure_add_test_support_engine_library(conjure_test_support_engine_shared conjure::test_support_engine_shared
        conjure::test_support_shared
        conjure::shared
    )
endif()
