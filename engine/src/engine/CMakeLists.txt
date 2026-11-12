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
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/imgui_layer.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/layer.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/layer_stack.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/engine/log_category.h
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
    main_thread_queue.cpp
    main_thread_queue.h
    project.cpp
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
)
