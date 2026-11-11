#pragma once

#include "conjure/scene/scene_manager.h"

#include "conjure/assets/asset_importer.h"
#include "conjure/assets/asset_type_registry.h"
#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/scheduler.h"

#include <string_view>

// Registration entry points the engine's scene_subsystem calls (docs/design/13_scene.md 4.7): the
// component types (startup step 10), the two asset types with their importers, and the four systems.

namespace conjure::scene {

    inline constexpr std::string_view SYSTEM_APPLY_PENDING_LOADS = "scene.apply_pending_loads";
    inline constexpr std::string_view SYSTEM_APPLY_HIERARCHY_CHANGES = "scene.apply_hierarchy_changes";
    inline constexpr std::string_view SYSTEM_PROPAGATE_TRANSFORMS = "scene.propagate_transforms";
    inline constexpr std::string_view SYSTEM_DESTROY_PENDING = "scene.destroy_pending";
    inline constexpr std::string_view SCENE_IMPORTER_ID = "conjure.scene.scene_importer";
    inline constexpr std::string_view PREFAB_IMPORTER_ID = "conjure.scene.prefab_importer";

    /// <summary>
    /// Registers every scene component in a fixed order (idempotent per registry) and verifies the
    /// asset_ref layout contract of the bind pass. Errors are the registry's (NAME_CONFLICT, ...).
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<void> register_components(ecs::component_registry &registry);

    /// <summary>
    /// Registers conjure.scene.scene and conjure.scene.prefab (version 1) with loaders that parse on the
    /// worker against 'registry', and the .scene.json / .prefab.json importers.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<void> register_asset_types(
      assets::asset_type_registry &types,
      assets::importer_registry &importers,
      const ecs::component_registry &registry
    );

    /// <summary>
    /// Adds scene.apply_pending_loads (FRAME_BEGIN), scene.apply_hierarchy_changes and
    /// scene.propagate_transforms (TRANSFORM) and scene.destroy_pending (FRAME_END, after
    /// scripting.frame_end), all RUNS_WHILE_PAUSED. The manager must outlive the scheduler.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<void> register_systems(ecs::scheduler &scheduler, scene_manager &scenes);
}
