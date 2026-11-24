# conjure_engine - the application, frame loop, subsystem adapters, layers, project file, export pipeline
# and module consumers (docs/design/06_engine_loop.md). Depends publicly on core, ecs, platform, assets,
# scene and modules (engine_context and application are module services, so module_service.h is in the
# public headers); rhi and the optional renderer, physics, audio and scripting libraries stay private and
# compile in under their CONJURE_WITH_* option. imgui_layer.cpp is compiled only in editor builds
# (CONJURE_BUILD_EDITOR, never Distribution); debug_overlay_layer.cpp outside Distribution.

if(NOT TARGET conjure::scene OR NOT TARGET conjure::modules OR NOT TARGET conjure::rhi)
    message(STATUS "engine: scene, modules or rhi is not available, module skipped")
    return()
endif()

set(CONJURE_ENGINE_PUBLIC_HEADERS
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/application.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/application_desc.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/command_line.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/debug_overlay_layer.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/diagnostics/check_registry.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/engine_config.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/engine_context.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/engine_error.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/engine_phase.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/engine_subsystem.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/engine_time.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/engine_version.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/exit_code.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/export.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/fixed_step_clock.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/frame_memory.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/frame_stats.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/game_layer.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/game_screen.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/imgui_layer.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/layer.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/layer_stack.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/log_category.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/main_camera.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/project.h
)

set(CONJURE_ENGINE_SOURCES
    application.cpp
    application_desc.cpp
    builtin_modules.cpp
    builtin_modules.h
    check_registry.cpp
    command_line.cpp
    engine_config.cpp
    engine_error.cpp
    engine_phase.cpp
    engine_services.cpp
    engine_services.h
    engine_subsystem.cpp
    engine_time.cpp
    engine_version.cpp
    executable_path.cpp
    executable_path.h
    exit_code.cpp
    export_commands.cpp
    export_commands.h
    export_pipeline.cpp
    fixed_step_clock.cpp
    frame_loop.cpp
    frame_loop.h
    frame_memory.cpp
    frame_pacer.cpp
    frame_pacer.h
    frame_stats.cpp
    game_layer.cpp
    layer.cpp
    layer_stack.cpp
    log_category.cpp
    main_camera.cpp
    main_thread_queue.cpp
    main_thread_queue.h
    project.cpp
    scripting_request.cpp
    scripting_request.h
    module_consumers/assets_module_consumer.cpp
    module_consumers/assets_module_consumer.h
    module_consumers/ecs_module_consumer.cpp
    module_consumers/ecs_module_consumer.h
    module_consumers/module_content_file_system.cpp
    module_consumers/module_content_file_system.h
    module_consumers/scripting_module_consumer.cpp
    module_consumers/scripting_module_consumer.h
    subsystems/assets_subsystem.cpp
    subsystems/assets_subsystem.h
    subsystems/ecs_subsystem.cpp
    subsystems/ecs_subsystem.h
    subsystems/modules_subsystem.cpp
    subsystems/modules_subsystem.h
    subsystems/platform_subsystem.cpp
    subsystems/platform_subsystem.h
    subsystems/rhi_subsystem.cpp
    subsystems/rhi_subsystem.h
    subsystems/scene_subsystem.cpp
    subsystems/scene_subsystem.h
)

set(CONJURE_ENGINE_PRIVATE_DEPS
    conjure::rhi
)

if(CONJURE_WITH_RENDERER AND TARGET conjure::renderer)
    list(APPEND CONJURE_ENGINE_SOURCES
        subsystems/model_prefab_converter.cpp
        subsystems/model_prefab_converter.h
        subsystems/renderer_components.h
        subsystems/renderer_subsystem.cpp
        subsystems/renderer_subsystem.h
    )
    list(APPEND CONJURE_ENGINE_PRIVATE_DEPS conjure::renderer)
endif()
if(CONJURE_WITH_PHYSICS AND TARGET conjure::physics)
    list(APPEND CONJURE_ENGINE_SOURCES
        subsystems/physics_subsystem.cpp
        subsystems/physics_subsystem.h
    )
    list(APPEND CONJURE_ENGINE_PRIVATE_DEPS conjure::physics)
endif()
if(CONJURE_WITH_AUDIO AND TARGET conjure::audio)
    list(APPEND CONJURE_ENGINE_SOURCES
        subsystems/audio_components.h
        subsystems/audio_subsystem.cpp
        subsystems/audio_subsystem.h
    )
    list(APPEND CONJURE_ENGINE_PRIVATE_DEPS conjure::audio)
endif()
if(CONJURE_WITH_SCRIPTING AND TARGET conjure::scripting)
    list(APPEND CONJURE_ENGINE_SOURCES
        subsystems/scripting_subsystem.cpp
        subsystems/scripting_subsystem.h
    )
    list(APPEND CONJURE_ENGINE_PRIVATE_DEPS conjure::scripting)
endif()

# The overlay compiles to nothing in Distribution (its header is empty there); the ImGui context owner exists
# in editor builds only (R9) and is listed when the editor is on and Dear ImGui was fetched.
list(APPEND CONJURE_ENGINE_SOURCES debug_overlay_layer.cpp)
if(CONJURE_BUILD_EDITOR AND TARGET conjure::thirdparty::imgui)
    list(APPEND CONJURE_ENGINE_SOURCES imgui_layer.cpp)
    list(APPEND CONJURE_ENGINE_PRIVATE_DEPS conjure::thirdparty::imgui)
endif()

# The .NET SDK the engine's managed code is built with (export.h, dotnet_sdk.cpp, owner decision OWN-INSTALL):
# global.json at the repository root names it, and the configure bakes it into the library so that an installed
# editor, which has no source tree, names it when no usable SDK is found. A changed pin re-runs the configure.
set(engineGlobalJson "${PROJECT_SOURCE_DIR}/global.json")
if(NOT EXISTS "${engineGlobalJson}")
    message(FATAL_ERROR "engine: ${engineGlobalJson} does not exist; it names the .NET SDK the managed code needs")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${engineGlobalJson}")
file(READ "${engineGlobalJson}" engineGlobalJsonText)
string(JSON engineDotnetSdkVersion ERROR_VARIABLE engineGlobalJsonError GET "${engineGlobalJsonText}" sdk version)
if(engineGlobalJsonError OR NOT engineDotnetSdkVersion MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR
        "engine: ${engineGlobalJson} names no SDK version of the form <major>.<minor>.<patch> in sdk.version "
        "(${engineGlobalJsonError}${engineDotnetSdkVersion})"
    )
endif()
list(APPEND CONJURE_ENGINE_SOURCES dotnet_sdk.cpp)

# export_source_roots.targets is the one copy of the MSBuild file every export build imports: export_commands.cpp
# embeds it through this generated header, and managed/CMakeLists.txt hands the file itself to the publish of the
# prebuilt SDK (conjure_managed_sdk_export). A raw string literal keeps it verbatim.
set(engineSourceRootsTargets "${CMAKE_CURRENT_SOURCE_DIR}/export_source_roots.targets")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${engineSourceRootsTargets}")
file(READ "${engineSourceRootsTargets}" CONJURE_EXPORT_SOURCE_ROOTS_TARGETS)
if(CONJURE_EXPORT_SOURCE_ROOTS_TARGETS MATCHES "\\)conjure_targets\"")
    message(FATAL_ERROR "engine: ${engineSourceRootsTargets} holds the raw string delimiter of its generated header")
endif()
set(engineGeneratedSourceDirectory "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(engineSourceRootsHeader "#pragma once\n\n")
string(APPEND engineSourceRootsHeader
    "// Generated by engine/src/engine/CMakeLists.txt from export_source_roots.targets. Do not edit.\n\n"
    "#include <string_view>\n\n"
    "namespace conjure::engine::detail {\n"
    "    inline constexpr std::string_view EXPORT_SOURCE_ROOTS_TARGETS =\n"
    "      R\"conjure_targets(@CONJURE_EXPORT_SOURCE_ROOTS_TARGETS@)conjure_targets\";\n"
    "}\n"
)
# Written only when the text changes, so an unchanged file recompiles nothing.
file(CONFIGURE
    OUTPUT "${engineGeneratedSourceDirectory}/engine/export_source_roots_targets.h"
    CONTENT "${engineSourceRootsHeader}"
    @ONLY
)

conjure_add_module(engine
    PUBLIC_HEADERS
        ${CONJURE_ENGINE_PUBLIC_HEADERS}
    SOURCES
        ${CONJURE_ENGINE_SOURCES}
    PUBLIC_DEPS
        conjure::core
        conjure::ecs
        conjure::platform
        conjure::assets
        conjure::scene
        conjure::modules
    PRIVATE_DEPS
        ${CONJURE_ENGINE_PRIVATE_DEPS}
    PRIVATE_DEFINES
        CONJURE_DOTNET_SDK_VERSION="${engineDotnetSdkVersion}"
)
target_include_directories(conjure_engine PRIVATE "${engineGeneratedSourceDirectory}")
