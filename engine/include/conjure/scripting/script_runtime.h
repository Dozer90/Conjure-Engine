#pragma once

#include "conjure/scripting/native_api.h"
#include "conjure/scripting/script_class_registry.h"
#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_host.h"

#include "conjure/core/api.h"
#include "conjure/core/containers/flat_map.h"
#include "conjure/core/containers/small_vector.h"
#include "conjure/core/delegate.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/memory/frame_allocator.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/observer.h"
#include "conjure/ecs/query.h"
#include "conjure/ecs/world.h"

#include <array>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// The native side of script execution (docs/design/03_scripting.md 3.2-3.9, 4, 5). One script_runtime
// per process (asserted; script_runtime::active() is what the component hooks and the core table entries
// reach). It owns the host, the class registry, the store, the frame block, the pending queues and the
// DOD query scopes; attach_world() wires the ADDED/REMOVED observers of script_component. Every method
// runs on the main thread.

namespace conjure::scripting {

    enum class script_mode : u8 {
        EDIT,       // JSON is authoritative, no managed instances exist
        PLAY        // live instances are authoritative, JSON is a snapshot
    };

    /// <summary>
    /// One contact the engine forwards from physics_world::contact_events() (R6); dispatched to both
    /// participants' scripts with the normal flipped for b.
    /// </summary>
    struct collision_event {
        ecs::entity a = {};
        ecs::entity b = {};
        math::vec3 point = math::vec3(0.0f);
        math::vec3 normal = math::vec3(0.0f);
        f32 impulse = 0.0f;
        collision_kind kind = collision_kind::COLLISION_ENTER;
    };

    struct script_runtime_config {
        script_host_config host = {};
        std::filesystem::path gameAssemblyPath = {};        // optional at initialise time
        script_mode initialMode = script_mode::EDIT;
    };

    /// <summary>
    /// Snapshot of the runtime's size for diagnostics and tests.
    /// </summary>
    struct script_runtime_stats {
        u32 liveInstances = 0;
        u32 pendingCreates = 0;
        u32 pendingDestroys = 0;
        u32 pendingEntityDestroys = 0;
        u32 openQueryScopes = 0;
        u32 reloadCount = 0;
        u32 zombieAssemblies = 0;
        u32 cachedQueries = 0;      // ecs::query objects kept per component set for the DOD scopes
    };

    class CONJURE_API script_runtime {
        public:
            /// <summary>
            /// Chunk views a query scope holds without touching the heap: 10,000 rows of a 12-byte component
            /// are 13 chunks of 819 rows. A larger scope spills its state once per size step; finished states
            /// are reused best-fit, so the spilled one keeps serving the large scope.
            /// </summary>
            static constexpr u32 QUERY_SCOPE_INLINE_CHUNKS = 16;

            /// <summary>
            /// Scope states (open and reusable) reserved up front: the nesting depth scripts realistically reach.
            /// </summary>
            static constexpr u32 QUERY_SCOPE_RESERVE = 4;

        private:
            struct pending_create {
                script_entry_id id = {};
            };

            /// <summary>
            /// A fault reported inside create_instances, before its slot was mapped (OnCreate threw).
            /// </summary>
            struct faulted_slot {
                i32 slot = -1;
                script_callback callback = script_callback::COUNT;
            };

            struct query_scope_state {
                u32 id = 0;
                ecs::iteration_scope lock = {};
                core::small_vector<conjure_chunk_view, QUERY_SCOPE_INLINE_CHUNKS> chunks = {};
                core::small_vector<ecs::component_id, static_cast<usize>(QUERY_MAX_COMPONENTS)> types = {};
            };

            /// <summary>
            /// The component ids of a scope sorted ascending, without repeats and padded with
            /// component_id::INVALID: the same set in any order, {a, a} included, finds the same cached query
            /// (columns still follow the request order).
            /// </summary>
            using query_cache_key = std::array<u16, static_cast<usize>(QUERY_MAX_COMPONENTS)>;

            std::unique_ptr<script_host> mHost = nullptr;
            conjure_native_api mNativeApi = {};
            std::unique_ptr<conjure_frame_block> mFrameBlock = nullptr;     // heap-allocated: 64-byte aligned (D4)
            script_class_registry mClasses = {};
            script_store mStore = {};
            std::vector<pending_create> mPendingCreates = {};
            std::vector<i32> mPendingDestroySlots = {};
            std::vector<ecs::entity> mPendingEntityDestroys = {};
            ecs::world *mWorld = nullptr;
            ecs::component_registry *mRegistry = nullptr;
            std::unique_ptr<ecs::command_buffer> mDeferredCommands = nullptr;   // query-scope structural requests (R5)
            // Scope states live on the heap because managed holds a pointer into their chunk views for the
            // scope's lifetime: open scopes (oldest first) and the finished states kept for reuse.
            std::vector<std::unique_ptr<query_scope_state>> mQueryScopes = {};
            std::vector<std::unique_ptr<query_scope_state>> mFreeQueryScopes = {};
            core::flat_map<query_cache_key, ecs::query> mQueryCache = {};      // one query per component set
            core::memory::frame_allocator mScratch;                           // no default: sized by the constructor
            std::vector<u8> mBlobBuffer = {};
            script_mode mMode = script_mode::EDIT;
            script_phase mCurrentPhase = script_phase::COUNT;
            script_host_status mStatus = script_host_status::UNINITIALISED;
            ecs::component_id mScriptComponentId = {};
            ecs::observer_id mAddedObserver = {};
            ecs::observer_id mRemovedObserver = {};
            ecs::observer_id mDestroyedObserver = {};       // forgets the entity names
            std::filesystem::path mAssemblyPath = {};
            bool mAssemblyLoaded = false;
            bool mInFlush = false;
            bool mCreatingInstances = false;    // inside managed create_instances (slots not yet mapped)
            u32 mNextScopeId = 1;
            u32 mReloadCount = 0;
            u32 mZombieAssemblies = 0;
            std::vector<u64> mWarnedMissingClasses = {};
            std::vector<u64> mWarnedMirrorNames = {};
            std::vector<faulted_slot> mFaultedBeforeMapped = {};            // faults inside create_instances
            std::vector<conjure_collision_record> mCollisionRecords = {};   // dispatch_collision_events' batch
            std::unordered_map<ecs::entity, std::string> mEntityNames = {};
            std::function<std::span<const collision_event>()> mCollisionSource = {};

        public:
            explicit script_runtime(std::unique_ptr<script_host> host);

            /// <summary>
            /// destroy_all(false) when instances are still alive, then shuts the host down.
            /// </summary>
            ~script_runtime();

            script_runtime(const script_runtime &) = delete;
            script_runtime &operator=(const script_runtime &) = delete;

        public:
            /*---------------------------------------------------------------*/
            // Lifetime

            /// <summary>
            /// Bindings must have filled native_api() before this call; the core entries are filled here.
            /// Initialises the host and loads config.gameAssemblyPath when set. A host start or table
            /// validation failure leaves the runtime DISABLED: entries keep their JSON with INVALID_SLOT and
            /// every phase is a no-op. A failure to load config.gameAssemblyPath is returned
            /// (ASSEMBLY_LOAD_FAILED) but leaves the runtime READY without an assembly (03_scripting.md 6:
            /// scenes still load); call load_game_assembly again once it is built.
            /// </summary>
            [[nodiscard]] core::result<void> initialise(const script_runtime_config &config);

            /// <summary>
            /// destroy_all(true), unload, host shutdown. The runtime may be initialised again.
            /// </summary>
            void shutdown();

            /// <summary>
            /// The world scripts act on (non-owning; nullptr detaches). Registers the script_component
            /// observers on it and adopts the script sets its entities already carry (queuing their
            /// creates in PLAY mode); the owner must destroy_all() before destroying the world. Swapping
            /// to another world destroys the live instances of the old one without OnDestroy (WARN when
            /// any exist): instances are bound to the ids of the world they were created on.
            /// </summary>
            void attach_world(ecs::world *world);

            /// <summary>
            /// The registry [EcsComponent] types register into (non-owning). Needed for
            /// component_register_managed and component_lookup_type; defaults to none.
            /// </summary>
            void attach_registry(ecs::component_registry *registry);

            /// <summary>
            /// The source scripting.collision_dispatch reads (the engine binds physics_world::contact_events).
            /// </summary>
            void set_collision_source(std::function<std::span<const collision_event>()> source);

            /*---------------------------------------------------------------*/
            // Assemblies (3.8)

            /// <summary>
            /// Loads the game assembly: new load context, classes staged then committed, every entry whose
            /// class now exists is created in PLAY mode. With an assembly already loaded this is a reload.
            /// NOT_INITIALISED, ASSEMBLY_LOAD_FAILED (nothing changes).
            /// </summary>
            [[nodiscard]] core::result<void> load_game_assembly(const std::filesystem::path &assemblyPath);

            /// <summary>
            /// Hot reload: load new -> collect fields -> destroy -> unload -> swap registries -> recreate
            /// (OnCreate again, OnStart not). A failed load leaves the running game untouched; a leaked load
            /// context is a WARN (ASSEMBLY_LEAKED is logged, not returned).
            /// </summary>
            [[nodiscard]] core::result<void> reload_game_assembly(const std::filesystem::path &assemblyPath);

            /*---------------------------------------------------------------*/
            // Frame flow (3.2-3.5)

            /// <summary>
            /// EDIT -> PLAY creates every instance of the attached world's entities (and flushes); PLAY -> EDIT
            /// destroys them with OnDestroy and clears their STARTED and FAULTED flags, so the next PLAY runs
            /// OnStart again (the started flag is for hot reload only, D8).
            /// </summary>
            void set_mode(script_mode mode);

            /// <summary>
            /// One managed call for the phase, then the open query scopes are force-closed (WARN), the
            /// deferred commands flushed and flush_pending() run while the phase is still current (so
            /// OnCreate/OnDestroy of that flush may open query scopes). Outside run_phase and
            /// dispatch_collision_events (FIXED_UPDATE) a query scope is refused with NOT_IN_PHASE: the
            /// flushes of frame_begin, frame_end and set_mode run no phase.
            /// </summary>
            void run_phase(script_phase phase, f32 deltaTime);

            /// <summary>
            /// Processes the pending destroys (OnDestroy, entity destruction) and creates (one
            /// create_instances call, OnCreate) until the queues are empty or MAX_FLUSH_ITERATIONS passed.
            /// </summary>
            void flush_pending();

            /// <summary>
            /// Builds one conjure_collision_record per participating entry whose class overrides the
            /// callback and sends them in one dispatch_collisions call. The record buffer is kept between
            /// calls, so a warm dispatch allocates nothing.
            /// </summary>
            void dispatch_collision_events(std::span<const collision_event> events);

            /// <summary>
            /// Destroys every live instance (OnDestroy when asked) and clears the pending queues; entries
            /// keep their JSON with INVALID_SLOT.
            /// </summary>
            void destroy_all(bool callOnDestroy);

            /*---------------------------------------------------------------*/
            // Scripts of an entity (3.3, 3.4, 3.6)

            /// <summary>
            /// Adds a script_component when missing, appends an entry with the given JSON fields (an object;
            /// invalid = defaults) and, in PLAY mode, queues its creation for the next flush. An invalid id
            /// when the entity is dead, the world detached or the entity already carries
            /// MAX_SCRIPTS_PER_ENTITY scripts, and (with a WARN) when the entity has no script_component yet
            /// while the world is locked for iteration: adding that component is a structural change.
            /// </summary>
            [[nodiscard]] script_entry_id add_script(
              ecs::entity entity,
              u64 classHash,
              const core::json::value &fields
            );

            /// <summary>
            /// Destroys the entry's instance now (OnDestroy, entity alive) and removes the entry. Ignored (DEBUG
            /// log) when the id does not belong to the entity's script set or no world is attached.
            /// </summary>
            void remove_script(ecs::entity entity, script_entry_id id);

            /// <summary>
            /// Rewrites one field's JSON; in PLAY mode also applies it to the live instance as a single-field
            /// blob. INSTANCE_NOT_FOUND, CLASS_NOT_FOUND, FIELD_NOT_FOUND, INVALID_ARGUMENT (type mismatch).
            /// </summary>
            [[nodiscard]] core::result<void> set_field(
              script_entry_id id,
              u64 fieldHash,
              const core::json::value &value
            );

            /// <summary>
            /// The entry's fields; for a live entry the JSON is refreshed from the instance first (unknown
            /// JSON keys preserved). INSTANCE_NOT_FOUND.
            /// </summary>
            [[nodiscard]] core::result<core::json::document> collect_entry_fields(script_entry_id id);

            /// <summary>
            /// {"scripts":[{"class","hash","fields"}]} of the entity's set (live fields refreshed first);
            /// {"scripts":[]} for an entity without scripts. ENTITY_NOT_ALIVE through the ecs category.
            /// </summary>
            [[nodiscard]] core::result<core::json::document> serialize_entity_scripts(ecs::entity entity);

            /// <summary>
            /// Replaces the entity's scripts from the serialize_entity_scripts form (live instances destroyed
            /// first, new ones queued in PLAY mode). INVALID_ARGUMENT for malformed JSON.
            /// </summary>
            [[nodiscard]] core::result<void> deserialize_entity_scripts(
              ecs::entity entity,
              const core::json::value &scripts
            );

            /// <summary>
            /// Rewrites every ENTITY field (and LIST of ENTITY) of the entity's scripts through 'remap' using
            /// the class schema and re-applies the fields when live.
            /// </summary>
            void remap_entity_fields(ecs::entity entity, const std::function<ecs::entity(ecs::entity)> &remap);

            /// <summary>
            /// Every entry id of the entity in set order (empty without scripts).
            /// </summary>
            [[nodiscard]] std::vector<script_entry_id> scripts_of(ecs::entity entity) const;

            /// <summary>
            /// The entry behind an id, or nullptr.
            /// </summary>
            [[nodiscard]] const script_entry *entry(script_entry_id id) const noexcept;

            /// <summary>
            /// The entry a managed slot belongs to, or an invalid id.
            /// </summary>
            [[nodiscard]] script_entry_id entry_of_slot(i32 slot) const noexcept;

            /// <summary>
            /// Called by the REMOVED observer on script_component (and by ops.destruct for sets freed with live
            /// instances): detaches the set's slots and defers their OnDestroy to the next flush.
            /// </summary>
            void on_component_removed(ecs::entity entity, const script_component &component);

            /// <summary>
            /// Called by the ADDED observer: adopts the set (owner = entity, creating an empty set for a
            /// zero handle) and queues creates in PLAY mode.
            /// </summary>
            void on_component_added(ecs::entity entity, script_component &component);

            /// <summary>
            /// Entity.Destroy() from C#: an entity with live scripts is destroyed at the next flush, after
            /// OnDestroy ran with the entity alive (even when requested inside a query scope); one without
            /// is destroyed now, or through deferred_commands() while the world is locked.
            /// </summary>
            void request_entity_destroy(ecs::entity entity);

            /// <summary>
            /// script_mark_faulted: records the FAULTED flag on the entry of a slot with a WARN (an OnCreate
            /// fault disables every callback of the instance, any other callback is disabled after three
            /// consecutive faults). A fault reported inside create_instances, before the slot is mapped, is
            /// applied and logged once the slot is.
            /// </summary>
            void mark_faulted(i32 slot, script_callback callback);

            /// <summary>
            /// The collision source set with set_collision_source(), or an empty function.
            /// </summary>
            [[nodiscard]] const std::function<std::span<const collision_event>()> &collision_source() const noexcept {
                return mCollisionSource;
            }

            /*---------------------------------------------------------------*/
            // Entity names of the core entity_* entries (the scene's name component is owned by the bindings,
            // which keep both in step; this table lets Entity.Create(name) / Entity.Find(name) work with ecs alone)

            /// <summary>
            /// Remembers a display name for an entity (empty erases). The name lives until the entity is
            /// destroyed, by any path (an observer of the attached world), or the world is detached.
            /// </summary>
            void set_entity_name(ecs::entity entity, std::string_view name);

            /// <summary>
            /// The remembered name of a live entity, or "".
            /// </summary>
            [[nodiscard]] std::string_view entity_name(ecs::entity entity) const noexcept;

            /// <summary>
            /// The first live entity remembered under 'name', or NULL_ENTITY.
            /// </summary>
            [[nodiscard]] ecs::entity find_entity_by_name(std::string_view name) const noexcept;

            /// <summary>
            /// Entities with a remembered name (diagnostics and tests).
            /// </summary>
            [[nodiscard]] usize entity_name_count() const noexcept {
                return mEntityNames.size();
            }

            /// <summary>
            /// component_lookup_type: true the first time a mirror name mismatches since the last assembly
            /// load, shutdown or world attach (the caller logs it then); false after that.
            /// </summary>
            [[nodiscard]] bool note_mirror_mismatch(u64 nameHash);

            /*---------------------------------------------------------------*/
            // DOD query scopes (3.9)

            /// <summary>
            /// Opens a scope over the archetypes holding every type: locks the world, fills the chunk views
            /// (columns in 'typeIds' order, null for an absent id), bumps the change versions of the written
            /// columns. WRONG_THREAD, NOT_IN_PHASE, NOT_INITIALISED, INVALID_ARGUMENT (no world, bad id;
            /// id 0 is never valid, R4). Allocation-free once warm: the ecs::query behind a component set is
            /// cached for the life of the attached world (cached_query_count()) and finished scope states
            /// are reused, so the scripting.update_10k_dod gate of 08_testing.md 7.1 holds.
            /// </summary>
            [[nodiscard]] core::result<u32> begin_query_scope(std::span<const u32> typeIds, conjure_query_scope &out);

            /// <summary>
            /// Closes a scope (unknown ids are ignored with a DEBUG log) and flushes deferred_commands()
            /// once no scope stays open.
            /// </summary>
            void end_query_scope(u32 scopeId);

            [[nodiscard]] u32 open_query_scope_count() const noexcept {
                return static_cast<u32>(mQueryScopes.size());
            }

            /// <summary>
            /// Distinct component sets begin_query_scope has cached a query for since the world was attached.
            /// </summary>
            [[nodiscard]] u32 cached_query_count() const noexcept {
                return static_cast<u32>(mQueryCache.size());
            }

            /*---------------------------------------------------------------*/
            // Accessors

            [[nodiscard]] conjure_native_api &native_api() noexcept {
                return mNativeApi;
            }

            [[nodiscard]] conjure_frame_block &frame_block() noexcept {
                return *mFrameBlock;
            }

            [[nodiscard]] const script_class_registry &classes() const noexcept {
                return mClasses;
            }

            /// <summary>
            /// The registry the native script_register_class/field entries fill (staging while a load runs).
            /// </summary>
            [[nodiscard]] script_class_registry &classes_mutable() noexcept {
                return mClasses;
            }

            [[nodiscard]] script_store &store() noexcept {
                return mStore;
            }

            [[nodiscard]] const script_store &store() const noexcept {
                return mStore;
            }

            [[nodiscard]] script_host_status status() const noexcept {
                return mStatus;
            }

            [[nodiscard]] script_mode mode() const noexcept {
                return mMode;
            }

            /// <summary>
            /// Phase being run, or COUNT outside a script phase (query_begin is refused with NOT_IN_PHASE then; the
            /// bindings entries check the thread only, 15_bindings.md 3.2; R54).
            /// </summary>
            [[nodiscard]] script_phase current_phase() const noexcept {
                return mCurrentPhase;
            }

            /// <summary>
            /// The buffer bindings record structural requests into while world::is_locked(); flushed at
            /// query_end and at the phase-end force-close (R5, R54). Valid while a world is attached.
            /// </summary>
            [[nodiscard]] ecs::command_buffer &deferred_commands() noexcept;

            [[nodiscard]] ecs::world *world() const noexcept {
                return mWorld;
            }

            [[nodiscard]] ecs::component_registry *registry() const noexcept {
                return mRegistry;
            }

            [[nodiscard]] script_host &host() noexcept {
                return *mHost;
            }

            [[nodiscard]] const script_host &host() const noexcept {
                return *mHost;
            }

            [[nodiscard]] ecs::component_id script_component_id() const noexcept {
                return mScriptComponentId;
            }

            [[nodiscard]] const std::filesystem::path &assembly_path() const noexcept {
                return mAssemblyPath;
            }

            [[nodiscard]] bool is_assembly_loaded() const noexcept {
                return mAssemblyLoaded;
            }

            [[nodiscard]] script_runtime_stats stats() const noexcept;

            /// <summary>
            /// Fires after a successful reload (editor refreshes inspectors).
            /// </summary>
            core::delegate<void()> on_reloaded = {};

            /// <summary>
            /// The single live runtime of the process, or nullptr.
            /// </summary>
            [[nodiscard]] static script_runtime *active() noexcept;

            /// <summary>
            /// Loops flush_pending() runs for creates and destroys requested from inside OnCreate/OnDestroy.
            /// </summary>
            static constexpr u32 MAX_FLUSH_ITERATIONS = 8;

            /*---------------------------------------------------------------*/
            // Component hooks (called by the script_component ops and serializer, R29)

            /// <summary>
            /// ops.copy: a deep copy of the set for a new component instance (owner unknown until ADDED).
            /// </summary>
            [[nodiscard]] u32 clone_set_for_copy(u32 handle);

            /// <summary>
            /// ops.destruct: frees the set; live slots are detached and their OnDestroy deferred.
            /// </summary>
            void free_set_from_destruct(u32 handle);

            /// <summary>
            /// serializer.writeJson: the {"scripts":[...]} object of a set (live fields refreshed first).
            /// </summary>
            void write_set_json(u32 handle, core::json::writer &out);

            /// <summary>
            /// serializer.readJson: replaces the entries of a set from the {"scripts":[...]} object; creates
            /// the set when 'handle' is INVALID_SET_HANDLE and returns the handle to store.
            /// </summary>
            [[nodiscard]] core::result<u32> read_set_json(u32 handle, const core::json::value &in);

            /// <summary>
            /// serializer.remapEntities: archive-local references become real through 'map'; real references
            /// that are targets of 'map' become archive-local (so the same hook serves capture and
            /// instantiation).
            /// </summary>
            void remap_set_entities(u32 handle, const ecs::entity_remap &map);

        private:
            /*---------------------------------------------------------------*/
            // Internals (script_runtime.cpp)

            [[nodiscard]] bool is_ready() const noexcept;
            [[nodiscard]] core::result<void> reload_internal(const std::filesystem::path &assemblyPath);
            void queue_create(script_entry_id id);
            void queue_creates_of_set(u32 handle);
            void adopt_world_sets(bool queueCreates);
            void enter_phase(script_phase phase) noexcept;
            void leave_phase() noexcept;
            void process_pending_destroys();
            void process_pending_creates();
            void destroy_slots(std::span<const i32> slots, bool callOnDestroy);
            void refresh_live_fields(script_entry &entryValue);
            [[nodiscard]] core::result<void> apply_entry_fields(
              script_entry &entryValue,
              const core::json::value &fields
            );
            [[nodiscard]] core::result<void> apply_entry_fields_with(
              script_entry &entryValue,
              const script_class_info &info,
              const core::json::value &fields
            );
            void mark_started_entries();
            [[nodiscard]] ecs::query &cached_query(const query_cache_key &key, const ecs::query_desc &desc);
            [[nodiscard]] std::unique_ptr<query_scope_state> take_scope_state(usize chunkCount);
            void force_close_query_scopes();
            void flush_deferred_commands();
            void warn_missing_class_once(u64 classHash);
            void log_faulted(i32 slot, script_callback callback, const script_entry &entryValue) const;
            void write_set_entries_json(script_set &set, core::json::writer &out);
            [[nodiscard]] core::result<void> replace_set_entries(u32 handle, const core::json::value &in);
    };

    /// <summary>
    /// Fills the core-owned entries (log_*, script_*, the five entity_*, component_*, query_*) of a table
    /// with functions that act on script_runtime::active(). Called by script_runtime::initialise.
    /// </summary>
    CONJURE_API void fill_core_native_entries(conjure_native_api &api) noexcept;
}
