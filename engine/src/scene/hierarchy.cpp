#include "conjure/scene/hierarchy.h"

#include <algorithm>

using namespace conjure;
using namespace conjure::scene;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

hierarchy::hierarchy(ecs::world &world, const scene_component_ids &ids) noexcept :
    mWorld(&world),
    mIds(&ids) {}

hierarchy::~hierarchy() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

ecs::entity hierarchy::parent_of(ecs::entity target) const noexcept {
    const parent *link = static_cast<const parent *>(mWorld->get_raw(target, mIds->parent));
    if (link == nullptr) {
        return ecs::NULL_ENTITY;
    }
    return link->value;
}

std::span<const ecs::entity> hierarchy::children_of(ecs::entity target) const noexcept {
    const children *list = static_cast<const children *>(mWorld->get_raw(target, mIds->children));
    if (list == nullptr) {
        return {};
    }
    return list->entities;
}

u32 hierarchy::sibling_index(ecs::entity target) const noexcept {
    const parent *link = static_cast<const parent *>(mWorld->get_raw(target, mIds->parent));
    if (link == nullptr) {
        return 0;
    }
    return link->siblingIndex;
}

u8 hierarchy::depth_of(ecs::entity target) const noexcept {
    if (!mWorld->is_alive(target)) {
        return 0;
    }
    return mIds->depth_of(mWorld->mask_of(target));
}

bool hierarchy::is_ancestor_of(ecs::entity ancestor, ecs::entity target) const noexcept {
    if (ancestor.is_null() || !mWorld->is_alive(target)) {
        return false;
    }
    ecs::entity current = parent_of(target);
    for (u32 step = 0; step < MAX_HIERARCHY_DEPTH && !current.is_null(); ++step) {
        if (current == ancestor) {
            return true;
        }
        current = parent_of(current);
    }
    return false;
}

ecs::entity hierarchy::root_of(ecs::entity target) const noexcept {
    if (!mWorld->is_alive(target)) {
        return ecs::NULL_ENTITY;
    }
    ecs::entity current = target;
    for (u32 step = 0; step < MAX_HIERARCHY_DEPTH; ++step) {
        const ecs::entity above = parent_of(current);
        if (above.is_null() || !mWorld->is_alive(above)) {
            return current;
        }
        current = above;
    }
    return current;
}

u8 hierarchy::subtree_height(ecs::entity target) const noexcept {
    if (!mWorld->is_alive(target)) {
        return 0;
    }
    struct frame {
        ecs::entity entity = {};
        u8 depth = 0;
    };
    core::small_vector<frame, 64> stack = {};
    stack.push_back(frame{target, 0});
    u8 height = 0;
    while (!stack.empty()) {
        const frame current = stack.back();
        stack.pop_back();
        height = std::max(height, current.depth);
        for (const ecs::entity child : children_of(current.entity)) {
            if (current.depth < 255) {
                stack.push_back(frame{child, static_cast<u8>(current.depth + 1)});
            }
        }
    }
    return height;
}

void hierarchy::collect_subtree(ecs::entity target, std::vector<ecs::entity> &out) const {
    for_each_in_subtree(target, [&out](ecs::entity entity) {
        out.push_back(entity);
    });
}

u32 hierarchy::subtree_size(ecs::entity target) const noexcept {
    u32 total = 0;
    for_each_in_subtree(target, [&total](ecs::entity) {
        ++total;
    });
    return total;
}
