#pragma once

#include "conjure/ecs/archetype.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/observer.h"
#include "conjure/ecs/query.h"
#include "conjure/ecs/view.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <atomic>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

// The world (docs/design/01_ecs.md 2, 3.1, 3.3, 3.4, 4.3, 5, 6).
//
//   component_registry registry = {};
//   CONJURE_REQUIRE_OK(registry.register_component<position>());
//   world world(registry);
//   core::result<entity> player = world.create_entity();
//   core::result<position *> placed = world.add<position>(player.value(), position{0.0f, 1.0f, 0.0f});
//
// A world has one owner thread (captured at construction). Structural operations (create, destroy,
// add, remove, flush, observer registration) run there while the world is unlocked; elsewhere or while
// an iteration_scope is alive they assert and return WRONG_THREAD / STRUCTURE_LOCKED. Reads
// (is_alive, has, get const, mask_of) are safe from any thread while the world is locked. Operations on
// dead or stale entities return ENTITY_NOT_ALIVE and never assert.

namespace conjure::ecs {

    class chunk;
    class chunk_pool;
    class entity_allocator;
    class entity_archive;
    class entity_handle;
    class observer_table;
    class world;

    /// <summary>
    /// Creation parameters of a world.
    /// </summary>
    struct world_desc {
        std::string name = "world";
        u32 initialEntityCapacity = 1024;      // records reserved up front
        u32 chunksPerSlab = 64;                // chunk blocks per pool slab (1 MiB per slab)
        u32 maxChunkSlabs = 0;                 // slabs the pool may grow to; 0 = unlimited (OUT_OF_MEMORY beyond)
    };

    /// <summary>
    /// Snapshot of a world's size.
    /// </summary>
    struct world_stats {
        u32 aliveEntities = 0;
        u32 archetypes = 0;
        u32 chunks = 0;
        u64 chunkBytes = 0;
        u64 version = 0;
    };

    /// <summary>
    /// RAII token that marks a world locked for iteration: structural calls are refused while any
    /// scope is alive. Movable; the moved-from scope releases nothing. Scopes may be taken from any
    /// thread (an atomic depth counter).
    /// </summary>
    class CONJURE_API iteration_scope {
        private:
            world *mWorld = nullptr;

        public:
            /// <summary>
            /// A scope that locks nothing.
            /// </summary>
            iteration_scope() noexcept = default;

            /// <summary>
            /// Locks 'target' until destruction. Used by world::lock_iteration().
            /// </summary>
            explicit iteration_scope(world &target) noexcept;

            ~iteration_scope();

            iteration_scope(const iteration_scope &) = delete;
            iteration_scope &operator=(const iteration_scope &) = delete;

            iteration_scope(iteration_scope &&other) noexcept;
            iteration_scope &operator=(iteration_scope &&other) noexcept;

        public:
            /// <summary>
            /// Unlocks now; later destruction does nothing.
            /// </summary>
            void release() noexcept;

            /// <summary>
            /// True while this scope holds a lock.
            /// </summary>
            [[nodiscard]] bool is_active() const noexcept {
                return mWorld != nullptr;
            }
    };

    class CONJURE_API world {
        private:
            friend class command_buffer;
            friend class component_registry;
            friend class chunk_view;
            friend class entity_archive;
            friend class query;
            friend class scheduler;
            friend class iteration_scope;

            enum class entity_state : u8 {
                FREE,
                ALIVE
            };

            /// <summary>
            /// Where an entity lives. The generation is the current occupant's (ALIVE) or the next one
            /// to be issued for the index (FREE), which is how flush recognises reserved ids.
            /// </summary>
            struct entity_record {
                archetype *owner = nullptr;
                u32 chunkIndex = 0;
                u32 row = 0;
                u32 generation = 0;
                entity_state state = entity_state::FREE;
            };

            /// <summary>
            /// One entry of the flush-stamped slot table (01_ecs.md 3.4): the plan index of an entity,
            /// valid only while 'stamp' equals the current flush's stamp, so the table is never
            /// cleared between flushes.
            /// </summary>
            struct flush_slot {
                u32 stamp = 0;
                u32 plan = 0;
            };

            const component_registry *mRegistry = nullptr;
            std::string mName = {};
            std::thread::id mOwnerThread = {};
            std::vector<entity_record> mRecords = {};
            std::unique_ptr<entity_allocator> mAllocator = {};
            std::unique_ptr<chunk_pool> mChunkPool = {};
            std::vector<std::unique_ptr<archetype>> mArchetypes = {};
            std::vector<archetype *> mArchetypePointers = {};
            std::unordered_map<component_mask, archetype *, component_mask::hasher> mArchetypeByMask = {};
            archetype *mEmptyArchetype = nullptr;
            std::atomic<i32> mLockDepth = 0;
            std::atomic<u64> mVersion = 1;
            bool mInObserver = false;
            u32 mStructuralDepth = 0;                      // nesting of structural calls (flush inside drain)
            std::unique_ptr<observer_table> mObservers = {};
            std::unique_ptr<command_buffer> mDeferred = {};             // observers enqueue here
            std::unique_ptr<command_buffer> mDeferredFlushing = {};     // swapped in while draining
            u32 mAliveCount = 0;
            std::vector<flush_slot> mFlushSlots = {};      // indexed by entity index, grown lazily
            u32 mFlushStamp = 0;                           // the stamp of the flush in progress

        public:
            explicit world(const component_registry &registry, world_desc desc = {});

            /// <summary>
            /// Destroys every entity without firing observers and returns every chunk.
            /// </summary>
            ~world();

            world(const world &) = delete;
            world &operator=(const world &) = delete;
            world(world &&) = delete;
            world &operator=(world &&) = delete;

        public:
            /*---------------------------------------------------------------*/
            // Entities. Owner thread only, world unlocked.

            /// <summary>
            /// Creates one entity in the archetype of 'mask' (default-constructed components) and fires
            /// ADDED observers. ARCHETYPE_TOO_LARGE, OUT_OF_MEMORY, ENTITY_LIMIT_REACHED, STRUCTURE_LOCKED,
            /// WRONG_THREAD, UNKNOWN_COMPONENT.
            /// </summary>
            [[nodiscard]] core::result<entity> create_entity(const component_mask &mask = {});

            /// <summary>
            /// Creates out.size() entities of one archetype chunk by chunk and writes their ids.
            /// </summary>
            [[nodiscard]] core::result<void> create_entities(const component_mask &mask, std::span<entity> out);

            /// <summary>
            /// Fires REMOVED and destroyed observers, destroys the components and frees the id. The
            /// deferred buffer observers filled is drained before returning. ENTITY_NOT_ALIVE for a
            /// dead or stale entity. Never cascades.
            /// </summary>
            [[nodiscard]] core::result<void> destroy_entity(entity target);

            /// <summary>
            /// True when the entity is alive with this generation. Any thread while locked.
            /// </summary>
            [[nodiscard]] bool is_alive(entity target) const noexcept;

            /// <summary>
            /// Number of alive entities.
            /// </summary>
            [[nodiscard]] u32 entity_count() const noexcept {
                return mAliveCount;
            }

            /// <summary>
            /// The component set of an entity; empty for a dead one.
            /// </summary>
            [[nodiscard]] component_mask mask_of(entity target) const noexcept;

            /*---------------------------------------------------------------*/
            // Typed components. Pointers are valid until the next structural change.

            /// <summary>
            /// Adds a component with an initial value and fires ADDED observers once the value is in
            /// place. Returns the stored component (nullptr for a tag). ENTITY_NOT_ALIVE,
            /// COMPONENT_ALREADY_PRESENT, UNKNOWN_COMPONENT, structural errors.
            /// </summary>
            template <typename ComponentType>
            [[nodiscard]] core::result<ComponentType *> add(entity target, ComponentType value = {}) {
                using plain_type = std::remove_cvref_t<ComponentType>;
                const component_id id = mRegistry->id_of<plain_type>();
                CONJURE_TRY_ASSIGN(void *slot, add_storage(target, id));
                if constexpr (!std::is_empty_v<plain_type>) {
                    plain_type *object = static_cast<plain_type *>(slot);
                    if constexpr (std::is_trivially_copyable_v<plain_type>) {
                        *object = value;
                    } else {
                        std::destroy_at(object);
                        std::construct_at(object, std::move(value));
                    }
                }
                finish_add(target, id);
                return static_cast<ComponentType *>(get_raw(target, id, false));
            }

            /// <summary>
            /// Removes a component after firing REMOVED observers. COMPONENT_NOT_PRESENT when absent.
            /// </summary>
            template <typename ComponentType>
            [[nodiscard]] core::result<void> remove(entity target) {
                return remove_raw(target, mRegistry->id_of<std::remove_cvref_t<ComponentType>>());
            }

            /// <summary>
            /// Replaces the value of an existing component and stamps its column. Not structural.
            /// ENTITY_NOT_ALIVE, COMPONENT_NOT_PRESENT.
            /// </summary>
            template <typename ComponentType>
            [[nodiscard]] core::result<void> set(entity target, ComponentType value) {
                using plain_type = std::remove_cvref_t<ComponentType>;
                const component_id id = mRegistry->id_of<plain_type>();
                if (!id.is_valid()) {
                    return make_error(ecs_error::UNKNOWN_COMPONENT, "component type is not registered");
                }
                if (!is_alive(target)) {
                    return make_error(ecs_error::ENTITY_NOT_ALIVE, "set on a dead entity");
                }
                if (!has_raw(target, id)) {
                    return make_error(ecs_error::COMPONENT_NOT_PRESENT, "set on a missing component");
                }
                if constexpr (!std::is_empty_v<plain_type>) {
                    plain_type *object = static_cast<plain_type *>(get_raw(target, id, true));
                    if constexpr (std::is_trivially_copyable_v<plain_type>) {
                        *object = value;
                    } else {
                        *object = std::move(value);
                    }
                }
                return core::result<void>::success();
            }

            /// <summary>
            /// The component for writing: stamps its chunk column. nullptr when dead or absent.
            /// </summary>
            template <typename ComponentType>
            [[nodiscard]] ComponentType *get(entity target) noexcept {
                return static_cast<ComponentType *>(get_raw(target, mRegistry->id_of<ComponentType>(), true));
            }

            /// <summary>
            /// The component for reading. nullptr when dead or absent. Never stamps.
            /// </summary>
            template <typename ComponentType>
            [[nodiscard]] const ComponentType *get(entity target) const noexcept {
                return static_cast<const ComponentType *>(get_raw(target, mRegistry->id_of<ComponentType>()));
            }

            /// <summary>
            /// True when the entity is alive and holds the component.
            /// </summary>
            template <typename ComponentType>
            [[nodiscard]] bool has(entity target) const noexcept {
                return has_raw(target, mRegistry->id_of<std::remove_cvref_t<ComponentType>>());
            }

            /*---------------------------------------------------------------*/
            // Untyped components (bindings, serialisation).

            /// <summary>
            /// Adds a component from bytes (ops.copy or memcpy; empty = default value) and returns the
            /// stored data (nullptr for a tag). Same errors as add().
            /// </summary>
            [[nodiscard]] core::result<void *> add_raw(
              entity target,
              component_id id,
              std::span<const std::byte> initial = {}
            );

            /// <summary>
            /// Removes a component by id.
            /// </summary>
            [[nodiscard]] core::result<void> remove_raw(entity target, component_id id);

            /// <summary>
            /// Component data by id; forWrite stamps the column. nullptr when dead, absent or a tag.
            /// </summary>
            [[nodiscard]] void *get_raw(entity target, component_id id, bool forWrite) noexcept;

            /// <summary>
            /// Read-only component data by id. nullptr when dead, absent or a tag. Safe from grain jobs
            /// while the world is locked (an iteration_scope, a system stage, parallel_for_chunks): no
            /// structural change can move a row meanwhile, so a job may read another entity's component
            /// this way (the scene's transform pass reads parent columns through it, 01_ecs.md 5.2) as
            /// long as no grain is writing that column at the same time.
            /// </summary>
            [[nodiscard]] const void *get_raw(entity target, component_id id) const noexcept;

            /// <summary>
            /// True when the entity is alive and its archetype holds 'id'.
            /// </summary>
            [[nodiscard]] bool has_raw(entity target, component_id id) const noexcept;

            /// <summary>
            /// Number of alive entities holding 'id'.
            /// </summary>
            [[nodiscard]] u32 instance_count(component_id id) const noexcept;

            /// <summary>
            /// Removes 'id' from every entity holding it (module unload, dynamic type redefinition).
            /// </summary>
            [[nodiscard]] core::result<void> remove_from_all(component_id id);

            /*---------------------------------------------------------------*/
            // Queries.

            /// <summary>
            /// A typed view over this world (view.h).
            /// </summary>
            template <typename... ComponentTypes>
            [[nodiscard]] ecs::view<ComponentTypes...> view() {
                return ecs::view<ComponentTypes...>(*this);
            }

            /// <summary>
            /// An untyped query.
            /// </summary>
            [[nodiscard]] query create_query(const query_desc &desc);

            /// <summary>
            /// Every archetype in creation order. Pointers stay valid for the world's lifetime.
            /// </summary>
            [[nodiscard]] std::span<archetype *const> archetypes() const noexcept {
                return mArchetypePointers;
            }

            /*---------------------------------------------------------------*/
            // Deferred mutation, locking, versions.

            /// <summary>
            /// Applies a command buffer (01_ecs.md 3.4): one archetype move per touched entity,
            /// destroys last, observers for the mask delta only. Commands on dead or stale entities
            /// are skipped and counted. The buffer is empty afterwards. A structural violation leaves
            /// the buffer untouched and reports nothing applied.
            /// </summary>
            [[nodiscard]] flush_report flush(command_buffer &commands);

            /// <summary>
            /// Locks the world for iteration until the returned scope dies. Any thread.
            /// </summary>
            [[nodiscard]] iteration_scope lock_iteration() noexcept;

            /// <summary>
            /// True while at least one iteration_scope is alive.
            /// </summary>
            [[nodiscard]] bool is_locked() const noexcept {
                return mLockDepth.load(std::memory_order_acquire) > 0;
            }

            /// <summary>
            /// The current change-detection version (starts at 1, never decreases).
            /// </summary>
            [[nodiscard]] u64 version() const noexcept {
                return mVersion.load(std::memory_order_acquire);
            }

            /// <summary>
            /// Increments the version and returns the new value. Any thread.
            /// </summary>
            u64 bump_version() noexcept {
                return mVersion.fetch_add(1, std::memory_order_acq_rel) + 1;
            }

            /*---------------------------------------------------------------*/
            // Archives.

            /// <summary>
            /// Instantiates one copy of an archive and returns its root (entry 0). With a command buffer
            /// the copy is recorded and applied at flush; the root id is still returned now.
            /// </summary>
            [[nodiscard]] core::result<entity> instantiate(
              const std::shared_ptr<const entity_archive> &archive,
              command_buffer *commands = nullptr
            );

            /// <summary>
            /// Instantiates outRoots.size() copies of an archive, placing entities directly in their
            /// final archetypes and rewiring internal entity references. ARCHIVE_CORRUPT before anything
            /// is created when the archive does not validate against this world's registry. When given,
            /// outEntities receives every created id, copy-major in entry order (copies x entity_count()
            /// entries, entries without components included); with a command buffer those are the ids
            /// reserved for the flush. After a failed call outEntities is empty.
            /// </summary>
            [[nodiscard]] core::result<void> instantiate_batch(
              const std::shared_ptr<const entity_archive> &archive,
              std::span<entity> outRoots,
              command_buffer *commands = nullptr,
              std::vector<entity> *outEntities = nullptr
            );

            /*---------------------------------------------------------------*/
            // Observers. Owner thread only.

            /// <summary>
            /// Registers a component observer. Observers run on the owner thread inside the structural
            /// operation and may only enqueue into the deferred buffer they receive.
            /// </summary>
            [[nodiscard]] observer_id observe(component_id id, component_event event, component_observer_fn fn);

            /// <summary>
            /// Registers an observer called before an entity is destroyed.
            /// </summary>
            [[nodiscard]] observer_id observe_destroyed(entity_observer_fn fn);

            /// <summary>
            /// Removes an observer. Stale or unknown ids are ignored.
            /// </summary>
            void unobserve(observer_id id);

            /*---------------------------------------------------------------*/
            // Misc.

            /// <summary>
            /// An OOP handle to an entity (entity_handle.h). Defined in entity_handle.h.
            /// </summary>
            [[nodiscard]] entity_handle handle(entity target) noexcept;

            /// <summary>
            /// The registry this world was created on.
            /// </summary>
            [[nodiscard]] const component_registry &registry() const noexcept {
                return *mRegistry;
            }

            /// <summary>
            /// The world's name from world_desc.
            /// </summary>
            [[nodiscard]] const std::string &name() const noexcept {
                return mName;
            }

            /// <summary>
            /// The thread structural operations must run on.
            /// </summary>
            [[nodiscard]] std::thread::id owner_thread() const noexcept {
                return mOwnerThread;
            }

            /// <summary>
            /// Hands ownership to another thread. Only while unlocked (STRUCTURE_LOCKED otherwise).
            /// </summary>
            [[nodiscard]] core::result<void> set_owner_thread(std::thread::id thread);

            /// <summary>
            /// Size snapshot.
            /// </summary>
            [[nodiscard]] world_stats stats() const noexcept;

            /// <summary>
            /// Full invariant check: records and rows agree, chunk counts sum to archetype counts, only
            /// the last chunk of an archetype is partial, free-list indices are FREE, every archetype
            /// mask maps back to itself, column offsets are aligned and inside the block. The error
            /// message names the first violated invariant.
            /// </summary>
            [[nodiscard]] core::result<void> validate() const;

            /// <summary>
            /// Destroys every entity (firing observers) and returns every chunk. Archetypes, observers
            /// and the id space are kept: destroyed ids come back with a bumped generation, so handles
            /// into the cleared world stay stale and ids reserved by command buffers stay valid.
            /// Owner thread, unlocked.
            /// </summary>
            void clear();

        private:
            /*---------------------------------------------------------------*/
            // Guards and lookups (world.cpp)

            [[nodiscard]] core::result<void> check_structural() const;
            [[nodiscard]] entity_record *find_alive(entity target) noexcept;
            [[nodiscard]] const entity_record *find_alive(entity target) const noexcept;
            [[nodiscard]] bool is_reserved(entity target) const noexcept;
            [[nodiscard]] core::result<void> ensure_records(u32 highestIndex);
            [[nodiscard]] core::result<archetype *> archetype_for(const component_mask &mask);
            [[nodiscard]] core::result<archetype *> archetype_with(archetype &source, component_id id, bool added);
            void relayout_archetypes(component_id id);
            void begin_structural() noexcept;
            void end_structural();

            /*---------------------------------------------------------------*/
            // Rows (world_structural.cpp)

            [[nodiscard]] core::result<void> place_rows(archetype &target, std::span<const entity> ids);
            void discard_placed(entity target);
            void release_reserved(entity target);
            void remove_row(archetype &owner, u32 chunkIndex, u32 row);
            [[nodiscard]] core::result<void> move_entity(entity target, entity_record &record, archetype &destination);
            void destroy_alive(entity target, entity_record &record, bool fireObservers);
            [[nodiscard]] core::result<void *> add_storage(entity target, component_id id);
            void finish_add(entity target, component_id id);
            void fire_added(entity target, const component_mask &added);
            void fire_removed(entity target, const component_mask &removed);
            void fire_destroyed(entity target);
            void drain_deferred();
            void construct_column_default(const component_info &info, void *destination, u32 count) noexcept;
            void destruct_column(const component_info &info, void *object, u32 count) noexcept;
            void stamp_chunk(archetype &owner, u32 chunkIndex) noexcept;

            /*---------------------------------------------------------------*/
            // Flush and archives (world_structural.cpp, world_archive.cpp)

            [[nodiscard]] flush_report flush_internal(command_buffer &commands);
            [[nodiscard]] core::result<void> instantiate_now(
              const entity_archive &archive,
              std::span<const entity> ids,
              u32 copies
            );
    };
}
