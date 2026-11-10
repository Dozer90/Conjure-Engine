#pragma once

#include "conjure/physics/character_controller.h"
#include "conjure/physics/contact_events.h"
#include "conjure/physics/physics_debug_draw.h"
#include "conjure/physics/physics_layers.h"
#include "conjure/physics/physics_settings.h"
#include "conjure/physics/queries.h"

#include "conjure/assets/asset_types.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/math/aabb.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/math/ray.h"
#include "conjure/core/math/transform.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/system.h"
#include "conjure/ecs/world.h"

#include <memory>
#include <optional>
#include <span>

// The physics world (docs/design/14_physics.md 3, 4.2): one JPH::PhysicsSystem fully encapsulated (P1),
// Jolt jobs on core::jobs (P2), Jolt memory through core::memory (P3), bodies created by ECS observers at
// the next push_transforms (P8), every gameplay mutation a queued command (P9), allocation-free steady
// state (P15) and deterministic for any worker count (P16).
//
//   physics_world world(settings, jobs);
//   CONJURE_REQUIRE_OK(world.attach(ecsWorld));
//   world.push_transforms(dt); world.step(dt); world.pull_transforms(); world.collect_contact_events();
//   for (const contact_event &event : world.contact_events()) { ... }

namespace conjure::physics {

    /// <summary>
    /// A queued body mutation (P9). Opaque to users.
    /// </summary>
    struct body_command;

    /// <summary>
    /// Counters for the profiler and tests.
    /// </summary>
    struct physics_stats {
        u32 bodies = 0;
        u32 activeBodies = 0;
        u32 characters = 0;
        u32 bodiesWithoutShape = 0;           // pending entities waiting for colliders or assets
        u32 contactEvents = 0;                // events of the last collect_contact_events
        u32 droppedContactEvents = 0;         // cumulative
        u32 droppedCommands = 0;              // cumulative
        u32 shapeCacheEntries = 0;
        u32 updateErrors = 0;                 // cumulative EPhysicsUpdateError != None
        u64 stepMicroseconds = 0;
        u64 pushMicroseconds = 0;
        u64 pullMicroseconds = 0;
    };

    /// <summary>
    /// One Jolt world bound to one ecs::world. Non-copyable and non-movable: the Jolt listeners hold
    /// its address. Owner-thread calls (the thread that constructed the world) are attach, detach, the
    /// step pipeline and the queued body API; queries run from any thread while no step runs.
    /// </summary>
    class physics_world {
        private:
            struct state;

            std::unique_ptr<state> mState = nullptr;   // every Jolt object and the record tables

        public:
            /// <summary>
            /// Builds the Jolt system from validated settings (physics_settings::validate first; an invalid
            /// value is clamped with one ERROR) and sizes every buffer from them. Never fails.
            /// </summary>
            physics_world(const physics_settings &settings, core::jobs::job_system &jobs);

            /// <summary>
            /// Detaches when attached, destroys every body and releases the process-wide Jolt state.
            /// </summary>
            ~physics_world();

            physics_world(const physics_world &) = delete;
            physics_world &operator=(const physics_world &) = delete;
            physics_world(physics_world &&) = delete;
            physics_world &operator=(physics_world &&) = delete;

        public:
            /*---------------------------------------------------------------*/
            // ECS binding (owner thread of the ecs::world).

            /// <summary>
            /// Installs the component observers and queues every entity that already carries a body
            /// component. ALREADY_ATTACHED; INVALID_STATE when the registry lacks the physics or scene
            /// components (register_components / scene::register_components first).
            /// </summary>
            [[nodiscard]] core::result<void> attach(ecs::world &target);

            /// <summary>
            /// Removes the observers and destroys every body; pending synthesised exits are discarded.
            /// </summary>
            void detach();

            [[nodiscard]] bool is_attached() const noexcept;

            /*---------------------------------------------------------------*/
            // Step pipeline. The systems of register_systems call exactly these; tests call them directly.

            /// <summary>
            /// Clears the previous step's events (R6), creates pending bodies, applies rebuilds, pushes
            /// changed components and transforms into Jolt and applies the queued commands (3.4).
            /// 'deferred' receives the interpolated_transform add/remove of bodies whose motion type
            /// changed; nullptr uses an internal buffer flushed when the world is unlocked.
            /// </summary>
            void push_transforms(f32 fixedDeltaTime, ecs::command_buffer *deferred = nullptr);

            /// <summary>
            /// Updates the characters, then PhysicsSystem::Update once (3.5). Owner thread.
            /// </summary>
            void step(f32 fixedDeltaTime);

            /// <summary>
            /// Writes Jolt poses and velocities into local_transform, interpolated_transform, rigid_body
            /// and character_controller (3.7). With a system_context the chunks run through
            /// parallel_for_chunks; nullptr runs the loop inline on the caller.
            /// </summary>
            void pull_transforms(ecs::system_context *context = nullptr);

            /// <summary>
            /// Merges and sorts the step's raw contacts into contact_events() (3.6).
            /// </summary>
            void collect_contact_events();

            /// <summary>
            /// The events of the last step, valid until the next push_transforms (R6).
            /// </summary>
            [[nodiscard]] std::span<const contact_event> contact_events() const noexcept;

            /*---------------------------------------------------------------*/
            // Bodies (queued, applied at the next push_transforms). Owner thread. NO_BODY when the entity
            // has no body and is not pending; WRONG_THREAD off the owner thread; COMMAND_QUEUE_FULL.

            [[nodiscard]] bool has_body(ecs::entity target) const noexcept;
            [[nodiscard]] core::result<void> add_force(ecs::entity target, const math::vec3 &force);
            [[nodiscard]] core::result<void> add_force_at(
              ecs::entity target,
              const math::vec3 &force,
              const math::vec3 &point
            );
            [[nodiscard]] core::result<void> add_impulse(ecs::entity target, const math::vec3 &impulse);
            [[nodiscard]] core::result<void> add_impulse_at(
              ecs::entity target,
              const math::vec3 &impulse,
              const math::vec3 &point
            );
            [[nodiscard]] core::result<void> add_torque(ecs::entity target, const math::vec3 &torque);
            [[nodiscard]] core::result<void> add_angular_impulse(ecs::entity target, const math::vec3 &impulse);
            [[nodiscard]] core::result<void> set_linear_velocity(ecs::entity target, const math::vec3 &velocity);
            [[nodiscard]] core::result<void> set_angular_velocity(ecs::entity target, const math::vec3 &velocity);

            /// <summary>
            /// Moves the body instantly; scale is ignored, velocities and the interpolated pose are reset.
            /// The entity's local_transform follows: at the next pull for bodies the pull writes, at the
            /// push that applies the command for static and KINEMATIC_MOVES_WITH_TRANSFORM bodies.
            /// </summary>
            [[nodiscard]] core::result<void> teleport(ecs::entity target, const math::transform &pose);

            /// <summary>
            /// KINEMATIC bodies: MoveKinematic towards the pose over the next fixed step (R49).
            /// </summary>
            [[nodiscard]] core::result<void> move_kinematic(ecs::entity target, const math::transform &pose);

            /// <summary>
            /// Queued PhysicsSystem::SetGravity at the next push (R49).
            /// </summary>
            [[nodiscard]] core::result<void> set_gravity(const math::vec3 &gravity);

            /// <summary>
            /// The gravity in effect (after the last applied set_gravity, else settings().gravity).
            /// </summary>
            [[nodiscard]] math::vec3 gravity() const noexcept;

            [[nodiscard]] core::result<void> wake(ecs::entity target);
            [[nodiscard]] core::result<void> sleep(ecs::entity target);

            /// <summary>
            /// True when the body is inactive; false when the entity has no body.
            /// </summary>
            [[nodiscard]] bool is_sleeping(ecs::entity target) const noexcept;

            /// <summary>
            /// Sets the desired velocity of a character for the next step (it replaces the component's
            /// velocity for that step; the pull then writes the achieved velocity back). NOT_A_CHARACTER.
            /// </summary>
            [[nodiscard]] core::result<void> character_move(ecs::entity target, const math::vec3 &velocity);

            /// <summary>
            /// The ground state of a character, nullopt for anything else.
            /// </summary>
            [[nodiscard]] std::optional<ground_state> character_ground(ecs::entity target) const noexcept;

            /*---------------------------------------------------------------*/
            // Queries (3.8). Any thread while none of push_transforms, step and pull_transforms runs
            // (those mutate the row tables; STEP_IN_PROGRESS asserts and yields nothing meanwhile). In
            // the engine that means outside the PHYSICS_STEP and FIXED_END phases.

            /// <summary>
            /// Closest hit along the ray within maxDistance.
            /// </summary>
            [[nodiscard]] std::optional<raycast_hit> raycast(
              const math::ray &ray,
              f32 maxDistance,
              const query_filter &filter = {}
            ) const;

            /// <summary>
            /// Every hit along the ray sorted by distance, at most out.size(); returns the count written.
            /// </summary>
            [[nodiscard]] u32 raycast_all(
              const math::ray &ray,
              f32 maxDistance,
              std::span<raycast_hit> out,
              const query_filter &filter = {}
            ) const;

            /// <summary>
            /// Entities whose bodies overlap the sphere, one entry per body, at most out.size().
            /// </summary>
            [[nodiscard]] u32 overlap_sphere(
              const math::vec3 &center,
              f32 radius,
              std::span<ecs::entity> out,
              const query_filter &filter = {}
            ) const;

            /// <summary>
            /// Entities whose bodies overlap the oriented box, one entry per body, at most out.size().
            /// </summary>
            [[nodiscard]] u32 overlap_box(
              const math::vec3 &center,
              const math::vec3 &halfExtents,
              const math::quat &rotation,
              std::span<ecs::entity> out,
              const query_filter &filter = {}
            ) const;

            /// <summary>
            /// Sweeps the shape from start along direction (which includes the length); the closest hit.
            /// </summary>
            [[nodiscard]] std::optional<shape_cast_hit> shape_cast(
              const cast_shape_desc &shape,
              const math::transform &start,
              const math::vec3 &direction,
              const query_filter &filter = {}
            ) const;

            /*---------------------------------------------------------------*/
            // Assets, shapes, introspection.

            /// <summary>
            /// Marks every body whose colliders reference the asset for a shape rebuild.
            /// </summary>
            void on_asset_reloaded(assets::asset_id id);

            /// <summary>
            /// Drops cached shapes no body references.
            /// </summary>
            void collect_shapes();

            [[nodiscard]] const physics_settings &settings() const noexcept;
            [[nodiscard]] const physics_layer_table &layers() const noexcept;
            [[nodiscard]] physics_stats stats() const noexcept;
            [[nodiscard]] u32 body_count() const noexcept;
            [[nodiscard]] u32 active_body_count() const noexcept;

            /// <summary>
            /// The body's pose straight from Jolt (unit scale), nullopt without a body or while a push,
            /// step or pull runs.
            /// </summary>
            [[nodiscard]] std::optional<math::transform> body_pose(ecs::entity target) const noexcept;

            /// <summary>
            /// Bounds of every body (PhysicsSystem::GetBounds; invalid when empty).
            /// </summary>
            [[nodiscard]] math::aabb bounds() const noexcept;

            /// <summary>
            /// Draws the bodies into the sink (3.10). Draws nothing when Jolt was built without its debug
            /// renderer (Distribution).
            /// </summary>
            void debug_draw(physics_debug_draw &sink, physics_debug_draw_flags flags) const;

            /// <summary>
            /// Checks that rows, entities and Jolt agree (14_physics.md 6); the message names the first
            /// violated invariant. UNKNOWN_LAYER for a body whose component names a missing layer.
            /// </summary>
            [[nodiscard]] core::result<void> validate() const;
    };
}
