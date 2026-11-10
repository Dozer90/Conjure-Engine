#pragma once

#include "physics/body_record.h"
#include "physics/contact_collector.h"
#include "physics/jolt_include.h"
#include "physics/jolt_job_system.h"
#include "physics/jolt_layers.h"
#include "physics/jolt_runtime.h"
#include "physics/shape_builder.h"
#include "physics/shape_cache.h"

#include "conjure/physics/physics_world.h"

#include "conjure/core/containers/flat_hash_map.h"
#include "conjure/core/math/transform.h"
#include "conjure/core/types.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/component.h"
#include "conjure/ecs/observer.h"
#include "conjure/ecs/query.h"
#include "conjure/ecs/world.h"

#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <atomic>
#include <optional>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

// The private state of physics_world (docs/design/14_physics.md 2.6) and the operations the
// physics_world_*.cpp files share. Every container is reserved at construction (P15).

namespace conjure::physics {

    /// <summary>
    /// A queued gameplay mutation (P9), applied in call order at the next push_transforms.
    /// </summary>
    struct body_command {
        enum class kind : u8 {
            ADD_FORCE,
            ADD_FORCE_AT,
            ADD_IMPULSE,
            ADD_IMPULSE_AT,
            ADD_TORQUE,
            ADD_ANGULAR_IMPULSE,
            SET_LINEAR_VELOCITY,
            SET_ANGULAR_VELOCITY,
            TELEPORT,
            MOVE_KINEMATIC,
            SET_GRAVITY,
            ACTIVATE,
            DEACTIVATE,
            CHARACTER_MOVE
        };

        kind type = kind::ADD_FORCE;
        bool kept = false;                        // already carried over one step for a pending entity
        ecs::entity target = {};
        math::vec3 vectorA = math::vec3(0.0f);    // force / impulse / velocity / position
        math::vec3 vectorB = math::vec3(0.0f);    // application point
        math::quat rotation = math::quat(1.0f, 0.0f, 0.0f, 0.0f);
    };

    /// <summary>
    /// The component ids the world observes and reads, resolved at attach.
    /// </summary>
    struct physics_component_ids {
        ecs::component_id rigidBody = {};
        ecs::component_id box = {};
        ecs::component_id sphere = {};
        ecs::component_id capsule = {};
        ecs::component_id convex = {};
        ecs::component_id mesh = {};
        ecs::component_id material = {};
        ecs::component_id character = {};
        ecs::component_id parent = {};
        ecs::component_id localTransform = {};
        ecs::component_id interpolatedTransform = {};

        [[nodiscard]] bool is_complete() const noexcept;
        [[nodiscard]] bool is_collider(ecs::component_id id) const noexcept;
        [[nodiscard]] bool is_body_defining(ecs::component_id id) const noexcept;
    };

    /// <summary>
    /// Per body pair bookkeeping of the contact table (3.6).
    /// </summary>
    struct contact_pair_state {
        u16 subShapePairs = 0;
        bool trigger = false;
    };

    /// <summary>
    /// Rate limiter of a repeated warning: the first few occurrences log, later ones every 1024th.
    /// </summary>
    struct warn_limiter {
        u32 occurrences = 0;

        [[nodiscard]] bool should_log() noexcept {
            ++occurrences;
            return occurrences <= 8 || (occurrences % 1024) == 0;
        }
    };

    struct physics_world::state {
        /// <summary>
        /// Marker in rowOfEntity for an entity waiting in pendingCreate.
        /// </summary>
        static constexpr u32 PENDING_ROW = 0xFFFFFFFFu;

        /// <summary>
        /// Marker in rowOfEntity for a pendingCreate entry whose body components went away before the
        /// push that would have created it: the next flush drops the entry (no O(n) erase from the
        /// vector), and a component added again before then simply turns it back into PENDING_ROW.
        /// </summary>
        static constexpr u32 WITHDRAWN_ROW = 0xFFFFFFFEu;

        /// <summary>
        /// True for the two markers above (an entry that has no row).
        /// </summary>
        [[nodiscard]] static constexpr bool is_marker(u32 rowIndex) noexcept {
            return rowIndex >= WITHDRAWN_ROW;
        }

        /// <summary>
        /// Rows are grown in blocks of this many entries.
        /// </summary>
        static constexpr u32 ROW_BLOCK = 1024;

        /// <summary>
        /// Position / rotation difference a push treats as a teleport.
        /// </summary>
        static constexpr f32 TELEPORT_EPSILON = 1.0e-4f;

        physics_settings settings = {};
        core::jobs::job_system *jobs = nullptr;
        std::thread::id ownerThread = {};

        // Jolt. The runtime guard is first so it is released last.
        jolt_runtime::guard runtime = {};
        jolt_job_system jobSystem = {};
        std::unique_ptr<JPH::TempAllocatorImpl> tempAllocator = nullptr;
        std::unique_ptr<jolt::layer_tables> layerTables = nullptr;
        std::unique_ptr<JPH::PhysicsSystem> physicsSystem = nullptr;
        contact_collector contacts = {};
        activation_collector activation = {};
        std::unique_ptr<std::atomic<u8>[]> awake = nullptr;     // one flag per body index

        // ECS.
        ecs::world *world = nullptr;
        physics_component_ids ids = {};
        std::vector<ecs::observer_id> observers = {};
        // interpolated_transform edits made outside a system
        std::unique_ptr<ecs::command_buffer> ownDeferred = nullptr;
        // Persistent queries (P15): a query caches its archetype matches, so building one per step would
        // allocate; these are built at attach and refreshed in place. pullQuery matches rigid_body +
        // interpolated_transform + local_transform; the change queries match rigid_body (or
        // character_controller) + local_transform with a changed filter over both.
        std::optional<ecs::query> pullQuery = std::nullopt;
        std::optional<ecs::query> bodyChangeQuery = std::nullopt;
        std::optional<ecs::query> characterChangeQuery = std::nullopt;

        // Records.
        std::vector<body_record> records = {};
        core::flat_hash_map<ecs::entity, u32> rowOfEntity = {};
        std::vector<ecs::entity> pendingCreate = {};
        std::vector<ecs::entity> pendingScratch = {};
        std::vector<ecs::entity> pendingRebuild = {};   // rows with dirty != NONE (the row is the truth)
        u32 liveRows = 0;                                // rows holding a body or character
        std::vector<body_command> commands = {};
        std::vector<body_command> commandsScratch = {};
        std::vector<JPH::BodyID> activateBatch = {};
        std::vector<JPH::BodyID> sleepBatch = {};
        shape_cache shapes = {};
        u32 characterCount = 0;

        // Contacts.
        core::flat_hash_map<u64, contact_pair_state> activePairs = {};
        std::vector<contact_event> events = {};
        std::vector<u64> eventKeys = {};
        std::vector<contact_event> eventScratch = {};
        std::vector<raw_contact> rawMerged = {};
        std::vector<u64> sortKeys = {};
        std::vector<u64> sortScratchKeys = {};
        std::vector<u32> sortValues = {};
        std::vector<u32> sortScratchValues = {};
        std::vector<u64> pairScratch = {};
        // synthesised at destruction, reported by the next collect (3.3)
        std::vector<contact_event> pendingExits = {};
        std::vector<u64> pendingExitKeys = {};

        // Bookkeeping.
        physics_stats stats = {};
        // Raised for the whole of push_transforms, step and pull_transforms: every mutation of the row
        // tables and of Jolt happens inside one of them, so a query from another thread (3.8) is refused
        // with STEP_IN_PROGRESS while any of them runs.
        std::atomic<bool> busy = false;
        math::vec3 gravity = math::vec3(0.0f);
        u64 sinceVersion = 0;
        u32 addedSinceOptimise = 0;
        bool bodyLimitLoggedThisStep = false;
        warn_limiter commandQueueWarnings = {};
        warn_limiter contactBufferWarnings = {};
        warn_limiter updateErrorWarnings = {};
        warn_limiter eventBufferWarnings = {};

        state(const physics_settings &settings, core::jobs::job_system &jobs);
        ~state();

        state(const state &) = delete;
        state &operator=(const state &) = delete;

        /*---------------------------------------------------------------*/
        // Threads and rows (physics_world.cpp)

        [[nodiscard]] bool is_owner_thread() const noexcept;

        /// <summary>
        /// Reads a component without stamping its chunk (the const world overload).
        /// </summary>
        template <typename ComponentType>
        [[nodiscard]] const ComponentType *read(ecs::entity target) const noexcept {
            return std::as_const(*world).get<ComponentType>(target);
        }

        /// <summary>
        /// A component for writing: stamps its chunk.
        /// </summary>
        template <typename ComponentType>
        [[nodiscard]] ComponentType *write(ecs::entity target) noexcept {
            return world->get<ComponentType>(target);
        }

        [[nodiscard]] const body_record *find_row(ecs::entity target) const noexcept;
        [[nodiscard]] body_record *find_row(ecs::entity target) noexcept;
        [[nodiscard]] bool is_pending(ecs::entity target) const noexcept;
        [[nodiscard]] body_record *row_of_body(u32 bodyIdValue) noexcept;
        [[nodiscard]] const body_record *row_of_body(u32 bodyIdValue) const noexcept;
        [[nodiscard]] bool is_awake(const body_record &row) const noexcept;
        [[nodiscard]] core::result<void> queue(body_command command);

        /*---------------------------------------------------------------*/
        // Bodies (physics_world_bodies.cpp)

        void install_observers();
        void remove_observers();
        void on_component_added(ecs::entity target, ecs::component_id id, ecs::command_buffer &deferred);
        void on_component_removed(ecs::entity target, ecs::component_id id, ecs::command_buffer &deferred);
        void on_entity_destroyed(ecs::entity target, ecs::command_buffer &deferred);
        void mark_pending_create(ecs::entity target);
        void withdraw_pending(ecs::entity target);
        void mark_rebuild(ecs::entity target, record_dirty_flags flags);
        void flush_pending_creates(ecs::command_buffer &deferred);
        void apply_rebuilds(ecs::command_buffer &deferred);
        [[nodiscard]] bool create_body(ecs::entity target, ecs::command_buffer &deferred);
        void destroy_body(u32 row, ecs::command_buffer *deferred, bool synthesiseExits);
        void destroy_all_bodies();
        [[nodiscard]] body_record &claim_row(const JPH::BodyID &bodyId, ecs::entity target);
        [[nodiscard]] shape_build_input gather_colliders(
          ecs::entity target,
          motion_type motion,
          const math::vec3 &scale
        ) const;
        void apply_body_properties(body_record &row, const rigid_body &body);
        void apply_mass_properties(body_record &row, const rigid_body &body);
        void apply_motion_type(body_record &row, const rigid_body &body, ecs::command_buffer &deferred);
        void add_interpolated(ecs::entity target, ecs::command_buffer &deferred);
        void remove_interpolated(ecs::entity target, ecs::command_buffer &deferred);
        [[nodiscard]] math::transform compose_world_pose(ecs::entity target) const noexcept;
        [[nodiscard]] u8 resolve_layer(ecs::entity target, u8 requested) const;

        /*---------------------------------------------------------------*/
        // Characters (physics_world_characters.cpp)

        [[nodiscard]] bool create_character(
          ecs::entity target,
          const character_controller &controller,
          ecs::command_buffer &deferred
        );
        void rebuild_character(body_record &row, const character_controller &controller);
        void step_characters(f32 fixedDeltaTime);
        void pull_characters();

        /*---------------------------------------------------------------*/
        // Sync (physics_world_sync.cpp)

        void push_changed_components(f32 fixedDeltaTime, ecs::command_buffer &deferred);
        void push_hierarchy_rows(f32 fixedDeltaTime);
        void apply_commands(f32 fixedDeltaTime);

        /// <summary>
        /// Moves a body or character to the pose at once (SetPositionAndRotation), records it as the
        /// last pulled pose and resets the interpolated pose so nothing interpolates across the jump.
        /// </summary>
        void teleport_row(body_record &row, const math::transform &pose);

        /// <summary>
        /// Writes a world pose into the entity's local_transform: as is for a root, relative to the
        /// parent's composed pose otherwise (the serial tail of 3.7). Scale is left untouched.
        /// </summary>
        void write_local_pose(ecs::entity target, const math::transform &worldPose);
        void pull_chunk(ecs::chunk_view &chunk);
        void pull_parented_rows();

        /*---------------------------------------------------------------*/
        // Contacts (physics_world_contacts.cpp)

        void collect_events();
        void synthesise_exits(const body_record &row);
        void emit_event(const contact_event &event, u64 key);
        void sort_events();
    };
}
