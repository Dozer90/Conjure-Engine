#include "conjure/scene/transform_system.h"

#include "conjure/scene/register.h"

#include "conjure/core/string_id.h"

#include <utility>

namespace conjure::scene {

namespace {

    [[nodiscard]] ecs::query_desc level_query(const scene_component_ids &ids, u8 level) {
        ecs::query_desc desc = {};
        desc.with.set(ids.localTransform);
        if (level == 0) {
            desc.without.set(ids.parent);
        } else {
            desc.with.set(ids.depth[level]);
        }
        desc.optional.set(ids.parent);
        desc.optional.set(ids.children);
        desc.optional.set(ids.interpolatedTransform);
        desc.optional.set(ids.transformDirty);
        desc.optional.set(ids.worldTransform);
        desc.optional.set(ids.renderWorldTransform);
        desc.optional.set(ids.transformState);
        desc.writes.set(ids.worldTransform);
        desc.writes.set(ids.renderWorldTransform);
        desc.writes.set(ids.transformState);
        return desc;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

transform_system::transform_system(scene &target) :
    mScene(&target),
    mRootQuery(target.world(), level_query(target.ids(), 0)) {
    mLevelQueries.reserve(MAX_HIERARCHY_DEPTH - 1);
    for (u32 level = 1; level < MAX_HIERARCHY_DEPTH; ++level) {
        mLevelQueries.emplace_back(target.world(), level_query(target.ids(), static_cast<u8>(level)));
    }
}

transform_system::~transform_system() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

ecs::system_desc transform_system::describe() {
    const scene_component_ids &ids = mScene->ids();
    ecs::system_desc desc = {};
    desc.name = core::string_id(SYSTEM_NAME);
    desc.phase = ecs::system_phase::TRANSFORM;
    desc.flags = ecs::system_flags::RUNS_WHILE_PAUSED;
    desc.reads.set(ids.localTransform);
    desc.reads.set(ids.parent);
    desc.reads.set(ids.children);
    desc.reads.set(ids.interpolatedTransform);
    desc.reads.set(ids.transformDirty);
    desc.writes.set(ids.worldTransform);
    desc.writes.set(ids.renderWorldTransform);
    desc.writes.set(ids.transformState);
    desc.run = [this](ecs::system_context &context) {
        run(context);
    };
    return desc;
}


/*===========================================================================*/
/*===========================================================================*/
// Free functions

math::transform compute_world_transform(const scene &target, ecs::entity entity) {
    const ecs::world &world = target.world();
    const scene_component_ids &ids = target.ids();
    math::transform result = {};
    const local_transform *own = static_cast<const local_transform *>(world.get_raw(entity, ids.localTransform));
    if (own != nullptr) {
        result = own->to_transform();
    }
    ecs::entity current = target.tree().parent_of(entity);
    for (u32 step = 0; step < MAX_HIERARCHY_DEPTH && !current.is_null(); ++step) {
        const local_transform *local = static_cast<const local_transform *>(world.get_raw(current, ids.localTransform));
        if (local != nullptr) {
            result = local->to_transform() * result;
        }
        current = target.tree().parent_of(current);
    }
    return result;
}
}
