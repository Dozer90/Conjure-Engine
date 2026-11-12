#pragma once

#include "conjure/scene/components.h"
#include "conjure/scene/guid_index.h"
#include "conjure/scene/hierarchy.h"
#include "conjure/scene/scene_error.h"

#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_types.h"
#include "conjure/core/api.h"
#include "conjure/core/event.h"
#include "conjure/core/json/document.h"
#include "conjure/core/math/aabb.h"
#include "conjure/core/math/transform.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/entity_archive.h"
#include "conjure/ecs/observer.h"
#include "conjure/ecs/world.h"

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// One authored scene over one ecs::world (docs/design/13_scene.md 2.2, 3, 4.3): roots in order, the
// hierarchy and transform operations, names, tags, layers, guids, enabled state, the destroy queue and
// the archive instantiation shared by scene files, prefabs, duplication and snapshots. Every method is
// owner-thread only (the world's rules apply); observers registered in the constructor keep the
// children cache, the guid index and the transform companions consistent when scripts reach the raw ECS.

namespace conjure::scene {

    class scene;

    namespace detail {
        struct archive_shape;
    }

    /// <summary>
    /// Identity of a loaded scene asset inside a scene_manager. Generation 0 means "none".
    /// </summary>
    struct scene_id {
        u32 index = 0;
        u32 generation = 0;

        /// <summary>
        /// False for the default id.
        /// </summary>
        [[nodiscard]] constexpr bool is_valid() const noexcept {
            return generation != 0;
        }

        [[nodiscard]] constexpr bool operator==(const scene_id &) const noexcept = default;
    };

    /// <summary>
    /// The implicit owner of entities created by hand (not loaded from a scene asset).
    /// </summary>
    inline constexpr scene_id SCENE_ID_AUTHORED = scene_id{0, 1};

    /// <summary>
    /// One root of the scene and the loaded scene asset it belongs to.
    /// </summary>
    struct root_entry {
        ecs::entity entity = {};
        scene_id owner = SCENE_ID_AUTHORED;

        [[nodiscard]] bool operator==(const root_entry &) const noexcept = default;
    };

    /// <summary>
    /// What happens to the guids of an archive when it is instantiated.
    /// </summary>
    enum class guid_policy : u8 {
        KEEP,                     // every guid kept (a null one stays null); a collision is DUPLICATE_GUID
        REGENERATE_ON_CONFLICT,   // colliding entries get fresh guids (one WARN per call), null guids too
        REGENERATE_ALL            // every entity gets a fresh guid (prefabs, duplicates, count > 1)
    };

    /// <summary>
    /// Placement and guid handling of an archive instantiation.
    /// </summary>
    struct instantiate_options {
        ecs::entity parent = ecs::NULL_ENTITY;            // archive roots attach here (null = scene roots)
        u32 index = APPEND_CHILD;                          // sibling / root position of the first root
        scene_id owner = {};                               // {} = inherit from the parent or SCENE_ID_AUTHORED
        guid_policy guids = guid_policy::REGENERATE_ALL;
        std::optional<core::string_id> rootName = std::nullopt;
        std::optional<local_transform> rootTransform = std::nullopt;
        bool bindAssets = true;
    };

    /// <summary>
    /// What an instantiation produced.
    /// </summary>
    struct instantiate_report {
        u32 entityCount = 0;
        u32 unknownComponents = 0;                         // preserved unresolved components, every copy
        std::vector<assets::asset_id> assetIds = {};       // every bound reference, sorted, deduplicated
        std::vector<ecs::entity> entities = {};            // every created entity, copy-major, archive order
    };

    class CONJURE_API scene {
        private:
            /// <summary>
            /// One hierarchy repair scheduled for scene.apply_hierarchy_changes (3.7). A deferred_set_parent
            /// entry carries the parent the caller's command buffer installs at its flush (hasIntent): the
            /// repair waits until the live parent component agrees, since the flush may land after the
            /// TRANSFORM phase that first sees the entry.
            /// </summary>
            struct repair_entry {
                ecs::entity entity = {};
                ecs::entity previousParent = {};           // whose children list may still hold the entity
                ecs::entity intendedParent = {};           // deferred_set_parent: the parent after the flush
                scene_id owner = {};                       // kept when the entity becomes a root ({} = from the tree)
                u32 rootIndex = APPEND_CHILD;              // root position when the entity becomes a root
                bool hasIntent = false;                    // intendedParent is meaningful
            };

            /// <summary>
            /// Marks a scene operation: observers skip their bookkeeping while one is running.
            /// </summary>
            class CONJURE_API operation_scope {
                private:
                    scene *mOwner = nullptr;
                    bool mPrevious = false;

                public:
                    explicit operation_scope(scene &owner) noexcept;
                    ~operation_scope();

                    operation_scope(const operation_scope &) = delete;
                    operation_scope &operator=(const operation_scope &) = delete;
            };

            ecs::world *mWorld = nullptr;
            assets::asset_manager *mAssets = nullptr;
            scene_component_ids mIds = {};
            hierarchy mTree;                               // no default: bound to mWorld and mIds in the constructor
            std::vector<root_entry> mRoots = {};
            guid_index mGuids = {};
            core::json::document mSettings = core::json::document::object();
            std::string mName = {};
            u64 mStructureVersion = 1;
            std::vector<ecs::entity> mDestroyQueue = {};
            std::vector<repair_entry> mRepairQueue = {};
            std::vector<repair_entry> mRepairSpare = {};   // the repair pass swaps the queue in; both keep capacity
            ecs::command_buffer mScratch;                  // no default: bound to the world in the constructor
            std::vector<ecs::entity> mScratchEntities = {};
            u8 mMaxDepthInUse = 0;
            u32 mExpectedRepairs = 0;                      // deferred_set_parent calls whose observers stay silent
            u32 mRawEdits = 0;                             // raw parent edits seen since the last repair pass
            bool mInOperation = false;
            std::array<ecs::observer_id, 6> mObservers = {};
            core::event<> mOnStructureChanged = {};
            core::event<> mOnSettingsChanged = {};

        public:
            /// <summary>
            /// Binds the scene to a world whose registry carries the scene components (register_components).
            /// 'assets' may be null in pure ECS tests: the bind pass is then skipped.
            /// </summary>
            scene(ecs::world &world, assets::asset_manager *assets);

            /// <summary>
            /// Removes the observers. The world and its entities are left as they are.
            /// </summary>
            ~scene();

            scene(const scene &) = delete;
            scene &operator=(const scene &) = delete;
            scene(scene &&) = delete;
            scene &operator=(scene &&) = delete;

        public:
            /*---------------------------------------------------------------*/
            // Access

            [[nodiscard]] ecs::world &world() noexcept {
                return *mWorld;
            }

            [[nodiscard]] const ecs::world &world() const noexcept {
                return *mWorld;
            }

            /// <summary>
            /// The asset manager used by the bind pass, or nullptr.
            /// </summary>
            [[nodiscard]] assets::asset_manager *assets() const noexcept {
                return mAssets;
            }

            [[nodiscard]] const scene_component_ids &ids() const noexcept {
                return mIds;
            }

            [[nodiscard]] const hierarchy &tree() const noexcept {
                return mTree;
            }

            [[nodiscard]] const guid_index &guids() const noexcept {
                return mGuids;
            }

            /// <summary>
            /// Incremented once per structural operation (create, destroy, reparent, reorder, load, ...).
            /// </summary>
            [[nodiscard]] u64 structure_version() const noexcept {
                return mStructureVersion;
            }

            [[nodiscard]] std::string_view name() const noexcept {
                return mName;
            }

            /// <summary>
            /// Renames the scene (the file header's "name"); not a structural change.
            /// </summary>
            void set_name(std::string_view value);

            /// <summary>
            /// The opaque scene settings document (ambient, gravity, ... are owned by engine subsystems).
            /// </summary>
            [[nodiscard]] core::json::document &settings() noexcept {
                return mSettings;
            }

            [[nodiscard]] const core::json::document &settings() const noexcept {
                return mSettings;
            }

            /// <summary>
            /// Replaces the settings (a non-object root becomes an empty object) and emits on_settings_changed.
            /// </summary>
            void set_settings(core::json::document value);

            /*---------------------------------------------------------------*/
            // Entities (owner thread, world unlocked)

            /// <summary>
            /// Creates an entity with guid, name, local_transform and its companions in its final
            /// archetype, as a root or as the child of 'parent' at 'index'. INVALID_ENTITY for a dead
            /// parent, HIERARCHY_TOO_DEEP below depth 31.
            /// </summary>
            [[nodiscard]] core::result<ecs::entity> create_entity(
              core::string_id name,
              ecs::entity parent = ecs::NULL_ENTITY,
              u32 index = APPEND_CHILD
            );

            /// <summary>
            /// Creates 'count' copies of the template's subtree under 'parent' (fresh guids). A null
            /// template creates plain entities exactly like create_entity, as one structural operation
            /// (R55). 'out' receives the roots and must hold at least 'count' entries. Fails atomically: on
            /// an error no entity created by the call remains.
            /// </summary>
            [[nodiscard]] core::result<instantiate_report> create_entities(
              u32 count,
              ecs::entity templateEntity,
              ecs::entity parent,
              std::span<ecs::entity> out
            );

            /// <summary>
            /// Copies a subtree (fresh guids, scripts deep-copied by their own ops) under 'parent' (the
            /// root's own parent when null) at 'index' and returns the new root.
            /// </summary>
            [[nodiscard]] core::result<ecs::entity> duplicate_subtree(
              ecs::entity root,
              ecs::entity parent = ecs::NULL_ENTITY,
              u32 index = APPEND_CHILD
            );

            /// <summary>
            /// Queues the entity (and therefore its subtree) for destroy_pending. Queuing is O(1): an
            /// entity queued twice is listed twice and dies once (the second request finds it dead).
            /// </summary>
            void destroy(ecs::entity target);

            /// <summary>
            /// Destroys the entity and its whole subtree now, through the ECS deferred buffer (no
            /// recursion). INVALID_ENTITY for a dead entity.
            /// </summary>
            [[nodiscard]] core::result<void> destroy_now(ecs::entity target);

            /// <summary>
            /// Destroys every entity of the world, empties the roots, the guid index and the queues.
            /// Settings and name are kept.
            /// </summary>
            void clear();

            /// <summary>
            /// True when the entity is alive in this scene's world.
            /// </summary>
            [[nodiscard]] bool contains(ecs::entity target) const noexcept;

            /// <summary>
            /// Destroy requests waiting for destroy_pending (one per destroy() call on a live entity).
            /// </summary>
            [[nodiscard]] u32 pending_destroy_count() const noexcept {
                return static_cast<u32>(mDestroyQueue.size());
            }

            /*---------------------------------------------------------------*/
            // Hierarchy

            /// <summary>
            /// Reparents 'child' under 'newParent' (null = root) at 'index'. keepWorld preserves the
            /// world transform (ignored for a transformless child). SELF_PARENT, HIERARCHY_CYCLE,
            /// HIERARCHY_TOO_DEEP and INVALID_ENTITY leave everything unchanged.
            /// </summary>
            [[nodiscard]] core::result<void> set_parent(
              ecs::entity child,
              ecs::entity newParent,
              bool keepWorld = true,
              u32 index = APPEND_CHILD
            );

            /// <summary>
            /// The same contract recorded into a command buffer while the world is locked (R55): the
            /// parent change is applied at the flush, children and depth tags are repaired by the first
            /// scene.apply_hierarchy_changes that runs after that flush, without a WARN. The repair entry
            /// waits (pending_repair_count) while the buffer is still unflushed.
            /// </summary>
            [[nodiscard]] core::result<void> deferred_set_parent(
              ecs::command_buffer &commands,
              ecs::entity child,
              ecs::entity newParent,
              bool keepWorld = true,
              u32 index = APPEND_CHILD
            );

            /// <summary>
            /// Moves the entity among its siblings (or among the roots); an index past the end is
            /// clamped to the last position. INVALID_ENTITY for a dead entity, INVALID_SIBLING_INDEX
            /// when the parent is dead.
            /// </summary>
            [[nodiscard]] core::result<void> set_sibling_index(ecs::entity target, u32 index);

            /// <summary>
            /// The roots in order with their owning scene ids.
            /// </summary>
            [[nodiscard]] std::span<const root_entry> roots() const noexcept {
                return mRoots;
            }

            /// <summary>
            /// The owner of a root; nullopt for an entity that is not a root.
            /// </summary>
            [[nodiscard]] std::optional<scene_id> root_owner(ecs::entity root) const noexcept;

            /// <summary>
            /// Reassigns the owner of a root. INVALID_ENTITY when the entity is not a root.
            /// </summary>
            [[nodiscard]] core::result<void> set_root_owner(ecs::entity root, scene_id owner);

            /// <summary>
            /// Puts the listed roots, in the given order, into the positions they collectively occupy;
            /// roots that are not listed keep their relative order. INVALID_ENTITY when an entry is not a
            /// root or appears twice.
            /// </summary>
            [[nodiscard]] core::result<void> set_root_order(std::span<const ecs::entity> ordered);

            /// <summary>
            /// The deepest level any entity has carried since the last clear() (0 = roots only).
            /// </summary>
            [[nodiscard]] u8 max_depth_in_use() const noexcept {
                return mMaxDepthInUse;
            }

            /// <summary>
            /// Resolves "Root/Child/Leaf" by names, first among the roots then among children. NULL_ENTITY
            /// when a segment does not match.
            /// </summary>
            [[nodiscard]] ecs::entity find_path(std::string_view path) const;

            /*---------------------------------------------------------------*/
            // Transforms

            /// <summary>
            /// Adds local_transform and its companions (world, render, state) plus transform_dirty in
            /// one move. A present local_transform is overwritten. INVALID_ENTITY for a dead entity.
            /// </summary>
            [[nodiscard]] core::result<void> add_transform(ecs::entity target, const local_transform &value = {});

            /// <summary>
            /// Writes the local transform (stamping the chunk). NO_TRANSFORM when the entity has none.
            /// </summary>
            [[nodiscard]] core::result<void> set_local_transform(ecs::entity target, const local_transform &value);

            /// <summary>
            /// Sets the local transform so that the world transform equals 'value' (fresh ancestor walk).
            /// </summary>
            [[nodiscard]] core::result<void> set_world_transform(ecs::entity target, const math::transform &value);

            /// <summary>
            /// The local transform; nullopt when absent.
            /// </summary>
            [[nodiscard]] std::optional<math::transform> local_transform_of(ecs::entity target) const noexcept;

            /// <summary>
            /// The exact world transform from a fresh ancestor walk; nullopt when the entity has no
            /// local_transform.
            /// </summary>
            [[nodiscard]] std::optional<math::transform> world_transform_of(ecs::entity target) const;

            /// <summary>
            /// local_bounds transformed by the world matrix; nullopt without bounds or transform.
            /// </summary>
            [[nodiscard]] std::optional<math::aabb> world_bounds(ecs::entity target) const;

            /// <summary>
            /// Union of the world bounds of the subtree; nullopt when nothing in it has bounds.
            /// </summary>
            [[nodiscard]] std::optional<math::aabb> subtree_bounds(ecs::entity target) const;

            /*---------------------------------------------------------------*/
            // Identity and attributes

            /// <summary>
            /// The entity's guid, generated and indexed when it has none. INVALID_ENTITY for a dead entity.
            /// </summary>
            [[nodiscard]] core::result<core::uuid> ensure_guid(ecs::entity target);

            /// <summary>
            /// Replaces the entity's guid (adding the component when needed) and rebinds the index.
            /// DUPLICATE_GUID when another entity carries it.
            /// </summary>
            [[nodiscard]] core::result<void> set_guid(ecs::entity target, const core::uuid &value);

            /// <summary>
            /// The entity's guid; nullopt when it has none (or a null one).
            /// </summary>
            [[nodiscard]] std::optional<core::uuid> guid_of(ecs::entity target) const noexcept;

            /// <summary>
            /// The entity carrying the guid, or NULL_ENTITY.
            /// </summary>
            [[nodiscard]] ecs::entity find_guid(const core::uuid &id) const noexcept;

            /// <summary>
            /// The live entity carrying the guid; GUID_NOT_FOUND when none does (the editor's entity
            /// arguments report this instead of a silent null).
            /// </summary>
            [[nodiscard]] core::result<ecs::entity> resolve_guid(const core::uuid &id) const;

            /// <summary>
            /// The name, or the null id when the entity has none.
            /// </summary>
            [[nodiscard]] core::string_id name_of(ecs::entity target) const noexcept;

            /// <summary>
            /// The tag, or the null id when the entity has none.
            /// </summary>
            [[nodiscard]] core::string_id tag_of(ecs::entity target) const noexcept;

            /// <summary>
            /// The layer index, 0 when the entity has no layer component.
            /// </summary>
            [[nodiscard]] u8 layer_of(ecs::entity target) const noexcept;

            /// <summary>
            /// Sets the name (adding the component when needed). INVALID_ENTITY for a dead entity.
            /// </summary>
            [[nodiscard]] core::result<void> set_name(ecs::entity target, core::string_id value);

            /// <summary>
            /// Sets the tag (adding the component when needed). INVALID_ENTITY for a dead entity.
            /// </summary>
            [[nodiscard]] core::result<void> set_tag(ecs::entity target, core::string_id value);

            /// <summary>
            /// Sets the layer (0..31; larger values are OUT_OF_RANGE).
            /// </summary>
            [[nodiscard]] core::result<void> set_layer(ecs::entity target, u8 value);

            /// <summary>
            /// Toggles the entity's own `disabled` tag and the `inactive` tag of its descendants.
            /// </summary>
            [[nodiscard]] core::result<void> set_enabled(ecs::entity target, bool enabled);

            /// <summary>
            /// True when the entity is alive and not disabled itself.
            /// </summary>
            [[nodiscard]] bool is_enabled(ecs::entity target) const noexcept;

            /// <summary>
            /// True when the entity and every ancestor are enabled.
            /// </summary>
            [[nodiscard]] bool is_active(ecs::entity target) const noexcept;

            /// <summary>
            /// Appends every entity whose name equals 'value' (world order, a full component scan).
            /// </summary>
            void find_by_name(core::string_id value, std::vector<ecs::entity> &out) const;

            /// <summary>
            /// Appends every entity whose tag equals 'value'.
            /// </summary>
            void find_by_tag(core::string_id value, std::vector<ecs::entity> &out) const;

            /// <summary>
            /// Appends every entity carrying a layer component whose index equals 'value' (world order, a scan
            /// of the layer column). Entities that never received a layer are not listed, although layer_of()
            /// reports 0 for them.
            /// </summary>
            void find_by_layer(u8 value, std::vector<ecs::entity> &out) const;

            /// <summary>
            /// Emitted after every structural change (main thread).
            /// </summary>
            [[nodiscard]] core::event<> &on_structure_changed() noexcept {
                return mOnStructureChanged;
            }

            /// <summary>
            /// Emitted by set_settings so engine subsystems re-read the keys they own (main thread).
            /// </summary>
            [[nodiscard]] core::event<> &on_settings_changed() noexcept {
                return mOnSettingsChanged;
            }

            /*---------------------------------------------------------------*/
            // Systems and serializer support (owner thread)

            /// <summary>
            /// Body of scene.apply_hierarchy_changes: repairs raw and deferred parent edits (3.7), one
            /// flush per repaired entity so every retag reads live depth tags. Costs nothing when no
            /// repair is queued.
            /// </summary>
            void apply_hierarchy_changes();

            /// <summary>
            /// Body of scene.destroy_pending: destroy_now for every queued entity.
            /// </summary>
            void destroy_pending();

            /// <summary>
            /// Number of hierarchy repairs waiting for apply_hierarchy_changes.
            /// </summary>
            [[nodiscard]] u32 pending_repair_count() const noexcept {
                return static_cast<u32>(mRepairQueue.size());
            }

            /// <summary>
            /// Instantiates 'count' copies of an archive (3.9): validates guids, creates every entity in
            /// its final archetype, links children, depth tags, companions, dirty and inactive tags in one
            /// flush, applies the root overrides and binds asset references. 'outRoots' receives the root
            /// (entry 0) of every copy and must hold at least 'count' entries. Nothing is created on
            /// failure (DUPLICATE_GUID, HIERARCHY_TOO_DEEP, HIERARCHY_CYCLE, BAD_FORMAT, ARCHIVE_CORRUPT).
            /// </summary>
            [[nodiscard]] core::result<instantiate_report> instantiate_archive(
              const std::shared_ptr<const ecs::entity_archive> &archive,
              u32 count,
              const instantiate_options &options,
              std::span<ecs::entity> outRoots
            );

            /// <summary>
            /// Checks every invariant of section 6: parent/children agreement, root uniqueness, depth
            /// tags, inactive tags and the guid index bijection. A parentless entity must be a listed
            /// root only when it carries at least one scene component; raw ECS entities that never
            /// entered the scene are ignored. The message names the first violation.
            /// </summary>
            [[nodiscard]] core::result<void> validate() const;

        private:
            /*---------------------------------------------------------------*/
            // Observers (scene_observers.cpp)

            void register_observers();
            void on_entity_destroyed(ecs::entity target, ecs::command_buffer &deferred);
            void on_parent_added(ecs::entity target);
            void on_parent_removed(ecs::entity target);
            void on_guid_added(ecs::entity target);
            void on_guid_removed(ecs::entity target);
            void on_local_transform_added(ecs::entity target, ecs::command_buffer &deferred);

            /*---------------------------------------------------------------*/
            // Tree bookkeeping (scene.cpp)

            [[nodiscard]] usize root_index_of(ecs::entity target) const noexcept;
            [[nodiscard]] scene_id owner_of_tree(ecs::entity target) const noexcept;
            void insert_root(ecs::entity target, u32 index, scene_id owner);
            void erase_root(ecs::entity target);
            void link_child(ecs::entity child, ecs::entity parentEntity, u32 index);
            void unlink_child(ecs::entity child, ecs::entity parentEntity);
            void renumber_children(ecs::entity parentEntity, usize from);
            void retag_subtree(ecs::entity root, u8 newDepth);
            void refresh_inactive_subtree(ecs::entity root, bool ancestorInactive);
            void add_dirty(ecs::entity target);
            void add_companions(ecs::entity target, const ecs::component_mask &mask);
            void flush_scratch();
            void bump_structure();
            [[nodiscard]] bool parent_chain_inactive(ecs::entity parentEntity) const noexcept;
            [[nodiscard]] core::result<void> check_reparent(ecs::entity child, ecs::entity newParent) const;
            [[nodiscard]] core::result<ecs::entity> create_entity_unannounced(
              core::string_id entityName,
              ecs::entity parentEntity,
              u32 index
            );
            [[nodiscard]] core::result<void> destroy_unannounced(ecs::entity target);

            /*---------------------------------------------------------------*/
            // Repair (scene_link_pass.cpp)

            void queue_repair(ecs::entity target, scene_id owner);
            void repair_entity(const repair_entry &entry);
            void link_instantiated(
              const ecs::entity_archive &archive,
              const detail::archive_shape &shape,
              std::span<const ecs::entity> entities,
              u32 copies,
              const instantiate_options &options
            );
    };
}
