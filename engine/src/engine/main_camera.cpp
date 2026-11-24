#include "conjure/engine/main_camera.h"

#include "engine/subsystems/renderer_components.h"

#include "conjure/core/platform_detect.h"
#include "conjure/core/types.h"
#include "conjure/ecs/archetype.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/world.h"
#include "conjure/scene/components.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    #include "conjure/renderer/components.h"
#endif

#include <span>

using namespace conjure;

ecs::entity conjure::engine::find_main_camera(const ecs::world &world) {
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    const ecs::component_registry &registry = world.registry();
    const ecs::component_id cameraId = registry.id_of<renderer::camera_component>();
    const ecs::component_id transformId = registry.id_of<scene::render_world_transform>();
    if (!cameraId.is_valid() || !transformId.is_valid()) {
        return ecs::NULL_ENTITY;
    }
    // An unregistered tag cannot be on any entity, and component_mask::test is false for an invalid id.
    const ecs::component_id disabledId = registry.id_of<scene::disabled>();
    const ecs::component_id inactiveId = registry.id_of<scene::inactive>();

    ecs::entity best = ecs::NULL_ENTITY;
    bool bestIsMain = false;
    i32 bestPriority = 0;
    for (const ecs::archetype *archetype : world.archetypes()) {
        const bool holdsCamera = archetype->has(cameraId) && archetype->has(transformId);
        if (!holdsCamera || archetype->has(disabledId) || archetype->has(inactiveId)) {
            continue;
        }
        for (u32 chunk = 0; chunk < archetype->chunk_count(); ++chunk) {
            const std::span<const ecs::entity> entities = archetype->chunk_entities(chunk);
            const void *column = archetype->chunk_column(chunk, cameraId);
            const auto *cameras = static_cast<const renderer::camera_component *>(column);
            if (cameras == nullptr) {
                continue;
            }
            for (usize row = 0; row < entities.size(); ++row) {
                const renderer::camera_component &camera = cameras[row];
                // Strictly better only: an equal candidate later in world order never replaces an earlier one.
                const bool better = best.is_null() || (camera.isMain && !bestIsMain)
                  || (camera.isMain == bestIsMain && camera.priority > bestPriority);
                if (!better) {
                    continue;
                }
                best = entities[row];
                bestIsMain = camera.isMain;
                bestPriority = camera.priority;
            }
        }
    }
    return best;
#else
    CONJURE_UNUSED(world);
    return ecs::NULL_ENTITY;
#endif
}
