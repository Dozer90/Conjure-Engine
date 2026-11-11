#pragma once

#include "conjure/scene/scene.h"

#include "conjure/core/api.h"
#include "conjure/core/json/document.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity_archive.h"

#include <string>
#include <vector>

// In-memory play/stop snapshots (docs/design/13_scene.md 2.5, 3.14): a typed entity_archive of every
// entity in hierarchy order (TRANSIENT columns excluded) plus the root order, root owners, settings, name
// and structure version. No file form; restore clears the scene and instantiates with kept guids.

namespace conjure::scene {

    /// <summary>
    /// Everything needed to put a scene back the way it was. Move-only; owns its bytes.
    /// </summary>
    struct CONJURE_API scene_snapshot {
        ecs::entity_archive archive;                   // no default: bound to the registry in the constructor
        std::vector<u32> rootEntries = {};             // archive indices of the roots, in root order
        std::vector<scene_id> rootOwners = {};         // parallel to rootEntries
        core::json::document settings = core::json::document::object();
        std::string name = {};
        u64 structureVersion = 0;

        explicit scene_snapshot(const ecs::component_registry &registry) noexcept;
        ~scene_snapshot();

        scene_snapshot(const scene_snapshot &) = delete;
        scene_snapshot &operator=(const scene_snapshot &) = delete;
        scene_snapshot(scene_snapshot &&) noexcept = default;
        scene_snapshot &operator=(scene_snapshot &&) noexcept = default;

        /// <summary>
        /// Entities held by the snapshot.
        /// </summary>
        [[nodiscard]] u32 entity_count() const noexcept {
            return archive.entity_count();
        }
    };

    /// <summary>
    /// Captures every entity of the tree, the root order and owners, the settings and the name.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<scene_snapshot> capture(const scene &source);

    /// <summary>
    /// Clears the scene and instantiates the snapshot with kept guids, restores settings, name, root
    /// order and owners and binds asset references. On failure the scene is left empty.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<void> restore(scene &target, const scene_snapshot &snapshot);
}
