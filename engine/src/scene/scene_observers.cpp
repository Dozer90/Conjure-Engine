#include "conjure/scene/scene.h"

#include "conjure/scene/log_category.h"

#include "conjure/core/log.h"

#include <algorithm>

// The scene's ECS observers (docs/design/13_scene.md 3.6 - 3.8). They run synchronously inside structural
// calls on the owner thread and only enqueue into the deferred buffer or push into scene-side queues. While
// the scene itself mutates (mInOperation) they skip their bookkeeping; the guid observers always index.

namespace conjure::scene {

void scene::register_observers() {
    mObservers[0] = mWorld->observe_destroyed([this](ecs::world &, ecs::entity target, ecs::command_buffer &deferred) {
        on_entity_destroyed(target, deferred);
    });
    mObservers[1] = mWorld->observe(
      mIds.parent,
      ecs::component_event::ADDED,
      [this](ecs::world &, ecs::entity target, ecs::component_id, ecs::command_buffer &) {
          on_parent_added(target);
      }
    );
    mObservers[2] = mWorld->observe(
      mIds.parent,
      ecs::component_event::REMOVED,
      [this](ecs::world &, ecs::entity target, ecs::component_id, ecs::command_buffer &) {
          on_parent_removed(target);
      }
    );
    mObservers[3] = mWorld->observe(
      mIds.guid,
      ecs::component_event::ADDED,
      [this](ecs::world &, ecs::entity target, ecs::component_id, ecs::command_buffer &) {
          on_guid_added(target);
      }
    );
    mObservers[4] = mWorld->observe(
      mIds.guid,
      ecs::component_event::REMOVED,
      [this](ecs::world &, ecs::entity target, ecs::component_id, ecs::command_buffer &) {
          on_guid_removed(target);
      }
    );
    mObservers[5] = mWorld->observe(
      mIds.localTransform,
      ecs::component_event::ADDED,
      [this](ecs::world &, ecs::entity target, ecs::component_id, ecs::command_buffer &deferred) {
          on_local_transform_added(target, deferred);
      }
    );
}

void scene::on_entity_destroyed(ecs::entity target, ecs::command_buffer &deferred) {
    // The cascade: every child dies through the deferred buffer, which the world drains until empty.
    for (const ecs::entity child : mTree.children_of(target)) {
        deferred.destroy_entity(child);
    }
    if (mInOperation) {
        return;
    }
    // A raw world.destroy_entity: the parent REMOVED observer has just queued a repair for an entity
    // that will not exist; take it back so the raw-edit WARN stays honest, and forget a destroyed root.
    if (!mRepairQueue.empty() && mRepairQueue.back().entity == target) {
        mRepairQueue.pop_back();
        if (mRawEdits > 0) {
            --mRawEdits;
        }
    }
    erase_root(target);
}

void scene::on_parent_added(ecs::entity target) {
    if (mInOperation) {
        return;
    }
    if (mExpectedRepairs > 0) {
        --mExpectedRepairs;
        return;
    }
    ++mRawEdits;
    // A root that gains an unusable parent stays a root of the scene it belonged to.
    queue_repair(target, root_owner(target).value_or(scene_id{}));
}

void scene::on_parent_removed(ecs::entity target) {
    if (mInOperation) {
        return;
    }
    // The link is still readable here (REMOVED fires before the column goes): unlink now and remember
    // the owner of the tree the entity leaves, so a raw detach keeps it in its loaded scene.
    const parent *link = mWorld->get<parent>(target);
    scene_id owner = {};
    if (link != nullptr && mWorld->is_alive(link->value)) {
        owner = owner_of_tree(link->value);
        unlink_child(target, link->value);
    }
    if (mExpectedRepairs > 0) {
        --mExpectedRepairs;
        return;
    }
    ++mRawEdits;
    queue_repair(target, owner);
}

void scene::on_guid_added(ecs::entity target) {
    const guid *identity = mWorld->get<guid>(target);
    if (identity == nullptr || identity->value.is_null()) {
        return;
    }
    if (mGuids.insert(identity->value, target)) {
        return;
    }
    const ecs::entity existing = mGuids.find(identity->value);
    if (!(existing == target) && !mInOperation) {
        CONJURE_LOG_WARN(LOG_SCENE, "guid {} already maps to entity {}; entity {} is not indexed", identity->value,
          existing, target);
    }
}

void scene::on_guid_removed(ecs::entity target) {
    const guid *identity = mWorld->get<guid>(target);
    if (identity == nullptr) {
        return;
    }
    mGuids.erase(identity->value, target);
}

void scene::on_local_transform_added(ecs::entity target, ecs::command_buffer &deferred) {
    if (mInOperation) {
        return;
    }
    const ecs::component_mask mask = mWorld->mask_of(target);
    if (!mask.test(mIds.worldTransform)) {
        deferred.add_raw(target, mIds.worldTransform);
    }
    if (!mask.test(mIds.renderWorldTransform)) {
        deferred.add_raw(target, mIds.renderWorldTransform);
    }
    if (!mask.test(mIds.transformState)) {
        deferred.add_raw(target, mIds.transformState);
    }
    if (!mask.test(mIds.transformDirty)) {
        deferred.add_raw(target, mIds.transformDirty);
    }
}

void scene::queue_repair(ecs::entity target, scene_id owner) {
    repair_entry entry = {};
    entry.entity = target;
    entry.owner = owner;
    mRepairQueue.push_back(entry);
}
}
