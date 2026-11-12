#pragma once

#include "conjure/scene/components.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_serializer.h"

#include "conjure/assets/asset_ref.h"
#include "conjure/assets/asset_types.h"
#include "conjure/core/api.h"
#include "conjure/core/json/value.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/entity_archive.h"

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Prefabs (docs/design/13_scene.md 3.12, 3.13, 4.6): fully materialised instances marked with
// prefab_instance on the root and prefab_link on every entity (S9). Overrides are computed by diff, not
// stored; apply, revert and unpack are archive operations. prefab_builder is the type-agnostic way the
// engine turns a model into a prefab document.

namespace conjure::scene {

    /// <summary>
    /// The published payload of the conjure.scene.prefab asset type.
    /// </summary>
    struct prefab_asset {
        std::shared_ptr<const ecs::entity_archive> archive = nullptr;
        std::string name = {};
        core::uuid rootGuid = {};                          // guid of entry 0 in the file
        std::vector<assets::asset_id> dependencies = {};   // sorted, from the blob header
        assets::asset_id id = {};                          // the asset's own uuid (bound into new instances)
    };
}

CONJURE_DECLARE_ASSET_TYPE(conjure::scene::prefab_asset, "conjure.scene.prefab");

namespace conjure::scene {

    /// <summary>
    /// How an instance entity differs from its prefab entry.
    /// </summary>
    enum class entity_diff_kind : u8 {
        ADDED,      // an instance entity without a matching prefab entry
        REMOVED,    // a prefab entry without a matching instance entity
        MODIFIED    // components differ (names listed)
    };

    /// <summary>
    /// One difference between a prefab instance and its asset.
    /// </summary>
    struct entity_diff {
        core::uuid assetGuid = {};                         // null for an ADDED entity without a link
        ecs::entity instanceEntity = {};                   // NULL_ENTITY for REMOVED
        entity_diff_kind kind = entity_diff_kind::MODIFIED;
        std::vector<core::string_id> components = {};      // MODIFIED only
    };

    /// <summary>
    /// The differences of one instance.
    /// </summary>
    struct prefab_diff {
        std::vector<entity_diff> entries = {};

        [[nodiscard]] bool empty() const noexcept {
            return entries.empty();
        }
    };

    namespace prefab {

        /// <summary>
        /// Captures the subtree as a prefab document (external references become null, single root).
        /// </summary>
        [[nodiscard]] CONJURE_API core::result<prefab_document> build_document(
          const scene &source,
          ecs::entity root,
          std::string_view name
        );

        /// <summary>
        /// Marks a subtree as an instance of 'asset': prefab_instance on the root, prefab_link with the
        /// current guid on every entity (guids are generated where missing).
        /// </summary>
        [[nodiscard]] CONJURE_API core::result<void> mark_instance(
          scene &target,
          ecs::entity root,
          assets::asset_ref<prefab_asset> asset
        );

        /// <summary>
        /// Instantiates the prefab (every guid regenerated) and returns the root, which carries a
        /// prefab_instance bound to the asset; entities without a prefab_link receive their entry's guid.
        /// </summary>
        [[nodiscard]] CONJURE_API core::result<ecs::entity> instantiate(
          scene &target,
          const prefab_asset &asset,
          const instantiate_options &options = {}
        );

        /// <summary>
        /// 'count' instances in one batch; outRoots receives the roots in order.
        /// </summary>
        [[nodiscard]] CONJURE_API core::result<void> instantiate_many(
          scene &target,
          const prefab_asset &asset,
          u32 count,
          const instantiate_options &options,
          std::span<ecs::entity> outRoots
        );

        /// <summary>
        /// The nearest ancestor (the entity included) carrying prefab_instance, or NULL_ENTITY.
        /// </summary>
        [[nodiscard]] CONJURE_API ecs::entity instance_root_of(const scene &source, ecs::entity entity) noexcept;

        /// <summary>
        /// Compares the instance with its loaded asset entry by entry (matched through prefab_link).
        /// NOT_A_PREFAB_INSTANCE, PREFAB_NOT_LOADED.
        /// </summary>
        [[nodiscard]] CONJURE_API core::result<prefab_diff> diff(const scene &source, ecs::entity root);

        /// <summary>
        /// Replaces the instance with a fresh one at the same place, keeping the root's guid, name and
        /// local transform. NOT_A_PREFAB_INSTANCE, PREFAB_NOT_LOADED, and PREFAB_MULTIPLE_ROOTS or
        /// HIERARCHY_TOO_DEEP when the current asset no longer fits at the instance's place; the instance is
        /// left untouched on these errors.
        /// </summary>
        [[nodiscard]] CONJURE_API core::result<void> revert(scene &target, ecs::entity root);

        /// <summary>
        /// The instance as a new prefab document: prefab_link guids become entry guids (fresh ones for
        /// new entities and for a link guid an earlier entity already used) and the root loses its
        /// prefab_instance. NOT_A_PREFAB_INSTANCE.
        /// </summary>
        [[nodiscard]] CONJURE_API core::result<prefab_document> apply(const scene &source, ecs::entity root);

        /// <summary>
        /// Reverts every instance of the prefab (only the unmodified ones by default). Returns how
        /// many were reverted.
        /// </summary>
        [[nodiscard]] CONJURE_API u32 refresh_instances(
          scene &target,
          const assets::asset_id &prefab,
          bool onlyUnmodified = true
        );

        /// <summary>
        /// Removes the prefab markers of the subtree, stopping at nested instance roots unless
        /// recursive. NOT_A_PREFAB_INSTANCE.
        /// </summary>
        [[nodiscard]] CONJURE_API core::result<void> unpack(scene &target, ecs::entity root, bool recursive = false);
    }

    /// <summary>
    /// Builds a prefab document node by node without naming any component type (3.13): the engine's
    /// model converter adds renderer components by registered name.
    /// </summary>
    class CONJURE_API prefab_builder {
        public:
            static constexpr u32 NO_PARENT = 0xFFFFFFFFu;

        private:
            struct node_component {
                ecs::component_id id = {};
                std::vector<std::byte> bytes = {};         // object image owned by the builder
            };

            struct node {
                u32 parentIndex = NO_PARENT;
                core::string_id name = {};
                local_transform transform = {};
                std::vector<node_component> components = {};
                std::vector<ecs::component_id> tags = {};
            };

            const ecs::component_registry *mRegistry = nullptr;
            scene_component_ids mIds = {};
            std::vector<node> mNodes = {};

        public:
            explicit prefab_builder(const ecs::component_registry &registry);

            /// <summary>
            /// Destroys the component images.
            /// </summary>
            ~prefab_builder();

            prefab_builder(const prefab_builder &) = delete;
            prefab_builder &operator=(const prefab_builder &) = delete;

        public:
            /// <summary>
            /// Adds a node and returns its index. parentIndex is NO_PARENT for the root; an unknown
            /// parent makes build() fail with BUILDER_INVALID.
            /// </summary>
            [[nodiscard]] u32 add_node(u32 parentIndex, std::string_view name, const local_transform &transform = {});

            /// <summary>
            /// Adds a component by registered name from JSON. UNKNOWN_COMPONENT for an unknown name,
            /// SERIALIZATION_FAILED for malformed JSON, BUILDER_INVALID for an unknown node or a
            /// component the builder writes itself (parent, children, guid, prefab_link, name,
            /// local_transform, depth tags, companions, inactive): name and transform come from add_node.
            /// </summary>
            [[nodiscard]] core::result<void> add_component_json(
              u32 node,
              core::string_id component,
              const core::json::value &value
            );

            /// <summary>
            /// Adds a component from an object image of exactly the registered size.
            /// </summary>
            [[nodiscard]] core::result<void> add_component_raw(
              u32 node,
              ecs::component_id component,
              std::span<const std::byte> bytes
            );

            /// <summary>
            /// Adds a tag component. BUILDER_INVALID when the id is not a tag.
            /// </summary>
            [[nodiscard]] core::result<void> add_tag(u32 node, ecs::component_id component);

            /// <summary>
            /// Number of nodes added.
            /// </summary>
            [[nodiscard]] u32 node_count() const noexcept {
                return static_cast<u32>(mNodes.size());
            }

            /// <summary>
            /// Produces the document: guids generated, parent and siblingIndex from insertion order,
            /// prefab_link on every entry, entries in hierarchy order. BUILDER_INVALID unless exactly one
            /// root exists and every parent index names an earlier-or-later node without a cycle.
            /// </summary>
            [[nodiscard]] core::result<prefab_document> build(std::string_view name) const;

        private:
            [[nodiscard]] bool is_reserved(ecs::component_id id) const noexcept;
            void destroy_images() noexcept;
    };
}
