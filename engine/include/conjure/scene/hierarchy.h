#pragma once

#include "conjure/scene/components.h"

#include "conjure/core/api.h"
#include "conjure/core/containers/small_vector.h"
#include "conjure/core/types.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/world.h"

#include <span>
#include <utility>
#include <vector>

// Read-only tree queries over a world's parent / children / depth_tag components (docs/design/13_scene.md
// 4.2). Non-owning; a scene owns one and exposes it through scene::tree(). Every traversal is iterative
// (explicit stack), never recursive, and every query answers NULL / empty / 0 for a dead entity.

namespace conjure::scene {

    class CONJURE_API hierarchy {
        private:
            ecs::world *mWorld = nullptr;
            const scene_component_ids *mIds = nullptr;

        public:
            hierarchy(ecs::world &world, const scene_component_ids &ids) noexcept;
            ~hierarchy();

            hierarchy(const hierarchy &) = default;
            hierarchy &operator=(const hierarchy &) = default;

        public:
            /// <summary>
            /// The parent entity; NULL_ENTITY for roots and dead entities.
            /// </summary>
            [[nodiscard]] ecs::entity parent_of(ecs::entity target) const noexcept;

            /// <summary>
            /// The children in sibling order; empty for leaves and dead entities. The span is valid
            /// until the next structural change.
            /// </summary>
            [[nodiscard]] std::span<const ecs::entity> children_of(ecs::entity target) const noexcept;

            /// <summary>
            /// Position among the siblings (or among the roots for a root); 0 for a dead entity.
            /// </summary>
            [[nodiscard]] u32 sibling_index(ecs::entity target) const noexcept;

            /// <summary>
            /// Hierarchy depth from the depth tag: 0 for roots and dead entities.
            /// </summary>
            [[nodiscard]] u8 depth_of(ecs::entity target) const noexcept;

            /// <summary>
            /// True when 'ancestor' is a strict ancestor of 'target'.
            /// </summary>
            [[nodiscard]] bool is_ancestor_of(ecs::entity ancestor, ecs::entity target) const noexcept;

            /// <summary>
            /// The topmost ancestor (the entity itself for a root); NULL_ENTITY for a dead entity.
            /// </summary>
            [[nodiscard]] ecs::entity root_of(ecs::entity target) const noexcept;

            /// <summary>
            /// Number of levels below the entity: 0 for a leaf, 1 when it has children without
            /// grandchildren, and so on. 0 for a dead entity.
            /// </summary>
            [[nodiscard]] u8 subtree_height(ecs::entity target) const noexcept;

            /// <summary>
            /// True when the entity is alive in the world.
            /// </summary>
            [[nodiscard]] bool is_alive(ecs::entity target) const noexcept {
                return mWorld->is_alive(target);
            }

            /// <summary>
            /// Invokes callable(entity) for the entity and every descendant, depth first in sibling
            /// order, with an explicit stack (never recursive). Nothing happens for a dead entity.
            /// </summary>
            template <typename Callable>
            void for_each_in_subtree(ecs::entity target, Callable &&callable) const {
                if (!mWorld->is_alive(target)) {
                    return;
                }
                core::small_vector<ecs::entity, 64> stack = {};
                stack.push_back(target);
                while (!stack.empty()) {
                    const ecs::entity current = stack.back();
                    stack.pop_back();
                    callable(current);
                    const std::span<const ecs::entity> list = children_of(current);
                    for (usize index = list.size(); index > 0; --index) {
                        stack.push_back(list[index - 1]);
                    }
                }
            }

            /// <summary>
            /// Appends the entity and every descendant to 'out', depth first in sibling order.
            /// </summary>
            void collect_subtree(ecs::entity target, std::vector<ecs::entity> &out) const;

            /// <summary>
            /// Number of entities in the subtree, the entity included; 0 for a dead entity.
            /// </summary>
            [[nodiscard]] u32 subtree_size(ecs::entity target) const noexcept;

            /// <summary>
            /// The world the tree lives in.
            /// </summary>
            [[nodiscard]] ecs::world &world() const noexcept {
                return *mWorld;
            }

            /// <summary>
            /// The component ids the tree reads.
            /// </summary>
            [[nodiscard]] const scene_component_ids &ids() const noexcept {
                return *mIds;
            }
    };
}
