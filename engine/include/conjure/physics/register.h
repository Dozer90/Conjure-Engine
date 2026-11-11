#pragma once

#include "conjure/physics/physics_world.h"

#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_type_registry.h"
#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/scheduler.h"

// Registration entry points the engine's physics_subsystem calls (docs/design/14_physics.md 4.3): the
// eight component types (startup step 12, after scene::register_components), the collision mesh asset
// type with its importer, and the four systems.

namespace conjure::physics {

    inline constexpr core::string_id SYSTEM_PUSH_TRANSFORMS{"physics.push_transforms"};
    inline constexpr core::string_id SYSTEM_STEP{"physics.step"};
    inline constexpr core::string_id SYSTEM_PULL_TRANSFORMS{"physics.pull_transforms"};
    inline constexpr core::string_id SYSTEM_CONTACT_EVENTS{"physics.contact_events"};

    /// <summary>
    /// Registers rigid_body, the five colliders, physics_material and character_controller in a fixed
    /// order. Idempotent per registry; errors are the registry's (NAME_CONFLICT, ...).
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<void> register_components(ecs::component_registry &registry);

    /// <summary>
    /// Adds physics.push_transforms and physics.step (PHYSICS_STEP, MAIN_THREAD) and
    /// physics.pull_transforms (FIXED_END, PARALLEL) and physics.contact_events (FIXED_END, MAIN_THREAD).
    /// None runs while paused (R13). NOT_ATTACHED before physics_world::attach. The world must outlive
    /// the scheduler.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<void> register_systems(ecs::scheduler &scheduler, physics_world &world);

    /// <summary>
    /// Registers conjure.physics.collision_mesh (version 1) with its CPU-only loader and the
    /// .gltf / .glb / .obj importer.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<void> register_asset_types(
      assets::asset_type_registry &types,
      assets::importer_registry &importers
    );
}
