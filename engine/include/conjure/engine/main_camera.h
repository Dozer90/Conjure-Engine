#pragma once

#include "conjure/core/api.h"
#include "conjure/ecs/entity.h"

// The main-camera rule (docs/design/02_renderer.md, 06_engine_loop.md): which camera the game view shows. The
// renderer's extraction keeps its own loop because it needs every camera, not only the first; the two agree on
// which camera comes first.

namespace conjure::ecs {
    class world;
}

namespace conjure::engine {

    /// <summary>
    /// The camera extraction shows first: among the enabled entities (neither scene.disabled nor scene.inactive)
    /// holding renderer::camera_component and scene::render_world_transform, a main camera (isMain) before any
    /// other, then the highest priority, then the first in world order (archetype creation order, then row).
    /// NULL_ENTITY when there is none, when the world has never registered those components, and always in a
    /// build without the renderer. Reads the world without changing it; the caller keeps it from changing
    /// structurally meanwhile (main thread, outside a world flush).
    /// </summary>
    [[nodiscard]] CONJURE_API ecs::entity find_main_camera(const ecs::world &world);
}
