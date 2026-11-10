#include "conjure/scene/components.h"

#include <utility>

using namespace conjure;
using namespace conjure::scene;

namespace {

    template <u8... Depths>
    void resolve_depth_tags(
      const ecs::component_registry &registry,
      std::array<ecs::component_id, MAX_HIERARCHY_DEPTH> &out,
      std::integer_sequence<u8, Depths...>
    ) noexcept {
        ((out[Depths + 1] = registry.id_of<depth_tag<static_cast<u8>(Depths + 1)>>()), ...);
    }
}

/*===========================================================================*/
/*===========================================================================*/
// local_transform

math::transform local_transform::to_transform() const noexcept {
    math::transform result = {};
    result.position = position;
    result.rotation = rotation;
    result.scale = scale;
    return result;
}

local_transform local_transform::from_transform(const math::transform &value) noexcept {
    local_transform result = {};
    result.position = value.position;
    result.rotation = value.rotation;
    result.scale = value.scale;
    return result;
}


/*===========================================================================*/
/*===========================================================================*/
// scene_component_ids

scene_component_ids scene_component_ids::resolve(const ecs::component_registry &registry) noexcept {
    scene_component_ids ids = {};
    ids.parent = registry.id_of<scene::parent>();
    ids.children = registry.id_of<scene::children>();
    ids.localTransform = registry.id_of<local_transform>();
    ids.worldTransform = registry.id_of<world_transform>();
    ids.renderWorldTransform = registry.id_of<render_world_transform>();
    ids.interpolatedTransform = registry.id_of<interpolated_transform>();
    ids.transformState = registry.id_of<transform_state>();
    ids.transformDirty = registry.id_of<transform_dirty>();
    resolve_depth_tags(registry, ids.depth, std::make_integer_sequence<u8, MAX_HIERARCHY_DEPTH - 1>{});
    ids.name = registry.id_of<scene::name>();
    ids.tag = registry.id_of<scene::tag>();
    ids.layer = registry.id_of<scene::layer>();
    ids.guid = registry.id_of<scene::guid>();
    ids.disabled = registry.id_of<scene::disabled>();
    ids.inactive = registry.id_of<scene::inactive>();
    ids.localBounds = registry.id_of<local_bounds>();
    ids.prefabInstance = registry.id_of<prefab_instance>();
    ids.prefabLink = registry.id_of<prefab_link>();
    return ids;
}

bool scene_component_ids::is_complete() const noexcept {
    const bool scalars = parent.is_valid() && children.is_valid() && localTransform.is_valid()
      && worldTransform.is_valid() && renderWorldTransform.is_valid() && interpolatedTransform.is_valid()
      && transformState.is_valid() && transformDirty.is_valid() && name.is_valid() && tag.is_valid()
      && layer.is_valid() && guid.is_valid() && disabled.is_valid() && inactive.is_valid()
      && localBounds.is_valid() && prefabInstance.is_valid() && prefabLink.is_valid();
    if (!scalars) {
        return false;
    }
    for (u32 level = 1; level < MAX_HIERARCHY_DEPTH; ++level) {
        if (!depth[level].is_valid()) {
            return false;
        }
    }
    return true;
}

u8 scene_component_ids::depth_of(const ecs::component_mask &mask) const noexcept {
    for (u32 level = 1; level < MAX_HIERARCHY_DEPTH; ++level) {
        if (mask.test(depth[level])) {
            return static_cast<u8>(level);
        }
    }
    return 0;
}

bool scene_component_ids::has_transform_companions(const ecs::component_mask &mask) const noexcept {
    return mask.test(worldTransform) && mask.test(renderWorldTransform) && mask.test(transformState);
}
