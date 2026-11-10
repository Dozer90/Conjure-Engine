# conjure_physics - Jolt Physics behind physics_world, plain reflected body / collider / character
# components, 32 named layers, entity-keyed queries, sorted contact events and the
# conjure.physics.collision_mesh asset type (docs/design/14_physics.md). Depends publicly on
# conjure::core, conjure::ecs, conjure::scene and conjure::assets; Jolt and cgltf stay private and never
# reach a public header (CONVENTIONS 2).

if(NOT CONJURE_WITH_PHYSICS)
    message(STATUS "physics: CONJURE_WITH_PHYSICS is OFF, module skipped")
    return()
endif()
if(NOT TARGET conjure::ecs OR NOT TARGET conjure::scene OR NOT TARGET conjure::assets)
    message(STATUS "physics: conjure::ecs, conjure::scene or conjure::assets is not available, module skipped")
    return()
endif()
if(NOT TARGET conjure::thirdparty::jolt)
    message(FATAL_ERROR "physics: conjure::thirdparty::jolt is missing; third_party/CMakeLists.txt declares it")
endif()

# The Jolt feature defines of each configuration (JPH_DEBUG_RENDERER / JPH_ENABLE_ASSERTS in Development)
# travel PUBLIC on the Jolt target from third_party/CMakeLists.txt, so this module and the library agree.

set(CONJURE_PHYSICS_PUBLIC_HEADERS
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/physics/character_controller.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/physics/collision_mesh_asset.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/physics/components.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/physics/contact_events.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/physics/log_category.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/physics/physics.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/physics/physics_debug_draw.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/physics/physics_error.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/physics/physics_layers.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/physics/physics_settings.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/physics/physics_world.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/physics/queries.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/physics/register.h
)

conjure_add_module(physics
    PUBLIC_HEADERS
        ${CONJURE_PHYSICS_PUBLIC_HEADERS}
    SOURCES
        body_record.h
        collision_mesh_asset.cpp
        collision_mesh_importer.cpp
        collision_mesh_importer.h
        collision_mesh_internal.h
        collision_mesh_loader.cpp
        collision_mesh_loader.h
        contact_collector.cpp
        contact_collector.h
        contact_events.cpp
        jolt_conversions.h
        jolt_include.h
        jolt_job_system.cpp
        jolt_job_system.h
        jolt_layers.cpp
        jolt_layers.h
        jolt_memory.cpp
        jolt_memory.h
        jolt_runtime.cpp
        jolt_runtime.h
        log_category.cpp
        physics_debug_draw.cpp
        physics_error.cpp
        physics_layers.cpp
        physics_settings.cpp
        physics_systems.cpp
        physics_world.cpp
        physics_world_bodies.cpp
        physics_world_characters.cpp
        physics_world_contacts.cpp
        physics_world_debug.cpp
        physics_world_internal.h
        physics_world_queries.cpp
        physics_world_sync.cpp
        shape_builder.cpp
        shape_builder.h
        shape_cache.cpp
        shape_cache.h
    PUBLIC_DEPS
        conjure::core
        conjure::ecs
        conjure::scene
        conjure::assets
    PRIVATE_DEPS
        conjure::thirdparty::jolt
        conjure::thirdparty::cgltf
)

# CONJURE_PHYSICS_CROSS_PLATFORM_DETERMINISTIC (14_physics.md P16): Jolt is built with
# CROSS_PLATFORM_DETERMINISTIC from third_party/CMakeLists.txt; the engine's objects that convert and
# integrate alongside it need the same floating-point contract (/fp:precise is MSVC's default, GCC and
# Clang need contraction off).
if(CONJURE_PHYSICS_CROSS_PLATFORM_DETERMINISTIC)
    target_compile_options(conjure_physics PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/fp:precise>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-ffp-contract=off>
    )
endif()
