#pragma once

// Shared fixture of the physics tests (docs/design/14_physics.md 7, 10): a registry with the scene and
// physics components, an ecs::world, a physics_world built from test_settings() (small limits so overflow
// paths are testable), spawn helpers for boxes, spheres, ground, plates that follow their transform, static
// triggers and characters, run_steps() running push / step / pull / collect in order, for_each_job_mode()
// running a body with 0 and 4 workers, trigger_tally counting TRIGGER_ENTER / TRIGGER_EXIT over steps,
// scheduled_physics, the engine's view of the four systems (an ecs::scheduler with
// physics::register_systems, the PARALLEL pull fanned out over chunks), and check_pull_allocation_free(),
// the allocation gate of the scheduled pull at a given body count.

#include "conjure/testing/doctest.h"

#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/test_environment.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/math/transform.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/types.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/ecs/world.h"
#include "conjure/physics/character_controller.h"
#include "conjure/physics/components.h"
#include "conjure/physics/contact_events.h"
#include "conjure/physics/physics_settings.h"
#include "conjure/physics/physics_world.h"
#include "conjure/physics/register.h"
#include "conjure/scene/components.h"
#include "conjure/scene/register.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

namespace physics_test {

    /// <summary>
    /// Assertions recorded per contract violation: one in Debug and Development, none in Distribution.
    /// </summary>
    inline constexpr conjure::u32 ASSERTS_PER_VIOLATION = CONJURE_ASSERTS_ENABLED ? 1u : 0u;

    /// <summary>
    /// The fixed step every test runs at.
    /// </summary>
    inline constexpr conjure::f32 STEP = 1.0f / 60.0f;

    /// <summary>
    /// Standard gravity of physics_settings.
    /// </summary>
    inline constexpr conjure::f32 GRAVITY = 9.81f;

    /// <summary>
    /// Settings with limits small enough for overflow tests yet large enough for the 2,000-body stack.
    /// </summary>
    [[nodiscard]] inline conjure::physics::physics_settings test_settings() {
        conjure::physics::physics_settings settings = {};
        settings.maxBodies = 4096;
        settings.maxBodyPairs = 16384;
        settings.maxContactConstraints = 8192;
        settings.tempAllocatorBytes = 8 * conjure::MIB;
        settings.maxJobs = 512;
        settings.maxContactEventsPerStep = 4096;
        settings.maxQueuedCommands = 64;
        settings.maxCharacters = 16;
        return settings;
    }

    /// <summary>
    /// Registers the scene components (interpolated_transform among them) and the physics components.
    /// </summary>
    inline void register_physics_test_components(conjure::ecs::component_registry &registry) {
        CONJURE_REQUIRE_OK(conjure::scene::register_components(registry));
        CONJURE_REQUIRE_OK(conjure::physics::register_components(registry));
    }

    /// <summary>
    /// Worker count of the parallel job mode: at least 4, more when the environment allows it.
    /// </summary>
    [[nodiscard]] inline conjure::u32 parallel_worker_count() {
        return std::max<conjure::u32>(4, conjure::testing::environment().jobThreadCount);
    }

    /// <summary>
    /// Runs the body twice as SUBCASEs: "inline" with an unstarted job system (0 workers) and
    /// "parallel" with a started pool of at least four workers.
    /// </summary>
    template <typename Callable>
    void for_each_job_mode(Callable &&body) {
        SUBCASE("inline") {
            conjure::core::jobs::job_system jobs = {};
            body(jobs);
        }
        SUBCASE("parallel") {
            conjure::core::jobs::job_system jobs = {};
            conjure::core::jobs::job_system_desc desc = {};
            desc.workerCount = parallel_worker_count();
            REQUIRE(jobs.start(desc).has_value());
            body(jobs);
            jobs.stop();
        }
    }

    /// <summary>
    /// A local transform at a position.
    /// </summary>
    [[nodiscard]] inline conjure::scene::local_transform at(const conjure::math::vec3 &position) noexcept {
        conjure::scene::local_transform value = {};
        value.position = position;
        return value;
    }

    /// <summary>
    /// A world, its registry and a physics_world attached to it.
    /// </summary>
    class physics_fixture {
        public:
            conjure::ecs::component_registry registry = {};
            conjure::ecs::world world;                   // no default: built on the registry
            conjure::core::jobs::job_system *jobs = nullptr;
            conjure::physics::physics_world physics;     // no default: built from settings and jobs

        public:
            explicit physics_fixture(
              conjure::core::jobs::job_system &jobSystem,
              const conjure::physics::physics_settings &settings = test_settings()
            ) :
                world((register_physics_test_components(registry), registry)),
                jobs(&jobSystem),
                physics(settings, jobSystem) {
                CONJURE_REQUIRE_OK(physics.attach(world));
            }

            ~physics_fixture() {
                CONJURE_CHECK_OK(physics.validate());
                physics.detach();
                CONJURE_CHECK_OK(world.validate());
            }

            physics_fixture(const physics_fixture &) = delete;
            physics_fixture &operator=(const physics_fixture &) = delete;

        public:
            /// <summary>
            /// An entity with a local_transform at 'position'.
            /// </summary>
            [[nodiscard]] conjure::ecs::entity create_at(const conjure::math::vec3 &position) {
                const conjure::core::result<conjure::ecs::entity> created = world.create_entity();
                REQUIRE(created.has_value());
                CONJURE_REQUIRE_OK(world.add<conjure::scene::local_transform>(created.value(), at(position)));
                return created.value();
            }

            /// <summary>
            /// A box body (pending until the next push_transforms).
            /// </summary>
            [[nodiscard]] conjure::ecs::entity spawn_box(
              const conjure::math::vec3 &position,
              conjure::physics::motion_type motion = conjure::physics::motion_type::DYNAMIC,
              const conjure::math::vec3 &halfExtents = conjure::math::vec3(0.5f),
              conjure::u8 layer = 0
            ) {
                const conjure::ecs::entity entity = create_at(position);
                conjure::physics::rigid_body body = {};
                body.motionType = motion;
                body.layer = layer;
                CONJURE_REQUIRE_OK(world.add<conjure::physics::rigid_body>(entity, body));
                conjure::physics::box_collider collider = {};
                collider.halfExtents = halfExtents;
                CONJURE_REQUIRE_OK(world.add<conjure::physics::box_collider>(entity, collider));
                return entity;
            }

            /// <summary>
            /// A sphere body.
            /// </summary>
            [[nodiscard]] conjure::ecs::entity spawn_sphere(
              const conjure::math::vec3 &position,
              conjure::f32 radius = 0.5f,
              conjure::physics::motion_type motion = conjure::physics::motion_type::DYNAMIC
            ) {
                const conjure::ecs::entity entity = create_at(position);
                conjure::physics::rigid_body body = {};
                body.motionType = motion;
                CONJURE_REQUIRE_OK(world.add<conjure::physics::rigid_body>(entity, body));
                conjure::physics::sphere_collider collider = {};
                collider.radius = radius;
                CONJURE_REQUIRE_OK(world.add<conjure::physics::sphere_collider>(entity, collider));
                return entity;
            }

            /// <summary>
            /// A static 100 x 1 x 100 m slab whose top face is y = 0.
            /// </summary>
            [[nodiscard]] conjure::ecs::entity spawn_ground(conjure::u8 layer = 0) {
                return spawn_box(
                  conjure::math::vec3(0.0f, -0.5f, 0.0f),
                  conjure::physics::motion_type::STATIC,
                  conjure::math::vec3(50.0f, 0.5f, 50.0f),
                  layer
                );
            }

            /// <summary>
            /// A kinematic box that follows its local_transform (KINEMATIC_MOVES_WITH_TRANSFORM): a test moves
            /// it by writing the transform. spawn_box with motion_type::KINEMATIC is the plain kinematic one.
            /// </summary>
            [[nodiscard]] conjure::ecs::entity spawn_following_plate(
              const conjure::math::vec3 &position,
              const conjure::math::vec3 &halfExtents = conjure::math::vec3(0.5f)
            ) {
                const conjure::ecs::entity entity = create_at(position);
                conjure::physics::rigid_body body = {};
                body.motionType = conjure::physics::motion_type::KINEMATIC;
                body.flags = conjure::physics::rigid_body_flags::KINEMATIC_MOVES_WITH_TRANSFORM;
                CONJURE_REQUIRE_OK(world.add<conjure::physics::rigid_body>(entity, body));
                conjure::physics::box_collider collider = {};
                collider.halfExtents = halfExtents;
                CONJURE_REQUIRE_OK(world.add<conjure::physics::box_collider>(entity, collider));
                return entity;
            }

            /// <summary>
            /// A static trigger volume: a box_collider with isTrigger next to a STATIC rigid_body, or on its
            /// own (an implicit static body) when 'withRigidBody' is false.
            /// </summary>
            [[nodiscard]] conjure::ecs::entity spawn_static_trigger(
              const conjure::math::vec3 &position,
              const conjure::math::vec3 &halfExtents,
              bool withRigidBody = true
            ) {
                const conjure::ecs::entity entity = create_at(position);
                if (withRigidBody) {
                    conjure::physics::rigid_body body = {};
                    body.motionType = conjure::physics::motion_type::STATIC;
                    CONJURE_REQUIRE_OK(world.add<conjure::physics::rigid_body>(entity, body));
                }
                conjure::physics::box_collider volume = {};
                volume.halfExtents = halfExtents;
                volume.isTrigger = true;
                CONJURE_REQUIRE_OK(world.add<conjure::physics::box_collider>(entity, volume));
                return entity;
            }

            /// <summary>
            /// A character with its feet at 'position'.
            /// </summary>
            [[nodiscard]] conjure::ecs::entity spawn_character(
              const conjure::math::vec3 &position,
              const conjure::physics::character_controller &controller = {}
            ) {
                const conjure::ecs::entity entity = create_at(position);
                CONJURE_REQUIRE_OK(world.add<conjure::physics::character_controller>(entity, controller));
                return entity;
            }

            /// <summary>
            /// One fixed step: push, step, pull, collect.
            /// </summary>
            void run_step(conjure::f32 dt = STEP) {
                physics.push_transforms(dt);
                physics.step(dt);
                physics.pull_transforms();
                physics.collect_contact_events();
            }

            /// <summary>
            /// 'count' fixed steps.
            /// </summary>
            void run_steps(conjure::u32 count, conjure::f32 dt = STEP) {
                for (conjure::u32 index = 0; index < count; ++index) {
                    run_step(dt);
                }
            }

            /// <summary>
            /// True when the body exists and is asleep.
            /// </summary>
            [[nodiscard]] bool settled(conjure::ecs::entity entity) const noexcept {
                return physics.has_body(entity) && physics.is_sleeping(entity);
            }

            /// <summary>
            /// The world position straight from Jolt (REQUIREs a body).
            /// </summary>
            [[nodiscard]] conjure::math::vec3 position_of(conjure::ecs::entity entity) const {
                const std::optional<conjure::math::transform> pose = physics.body_pose(entity);
                REQUIRE(pose.has_value());
                return pose->position;
            }

            /// <summary>
            /// The local_transform position the pull wrote.
            /// </summary>
            [[nodiscard]] conjure::math::vec3 transform_position(conjure::ecs::entity entity) const {
                const conjure::scene::local_transform *local = world.get<conjure::scene::local_transform>(entity);
                REQUIRE(local != nullptr);
                return local->position;
            }
    };

    /// <summary>
    /// True when the event is between the two entities, in either order.
    /// </summary>
    [[nodiscard]] inline bool involves(
      const conjure::physics::contact_event &event,
      conjure::ecs::entity a,
      conjure::ecs::entity b
    ) noexcept {
        return (event.a == a && event.b == b) || (event.a == b && event.b == a);
    }

    /// <summary>
    /// TRIGGER_ENTER and TRIGGER_EXIT events summed over steps: those between one trigger and one other body,
    /// or those of every pair while both entities are left null.
    /// </summary>
    struct trigger_tally {
        conjure::ecs::entity trigger = {};
        conjure::ecs::entity other = {};
        conjure::u32 enters = 0;
        conjure::u32 exits = 0;

        /// <summary>
        /// True when the tally counts the event's pair.
        /// </summary>
        [[nodiscard]] bool counts(const conjure::physics::contact_event &event) const noexcept {
            return (trigger.is_null() && other.is_null()) || involves(event, trigger, other);
        }

        /// <summary>
        /// Runs 'steps' fixed steps and adds the counted trigger events of each.
        /// </summary>
        void run(physics_fixture &fixture, conjure::u32 steps) {
            for (conjure::u32 step = 0; step < steps; ++step) {
                fixture.run_step();
                for (const conjure::physics::contact_event &event : fixture.physics.contact_events()) {
                    if (!counts(event)) {
                        continue;
                    }
                    if (event.kind == conjure::physics::contact_kind::TRIGGER_ENTER) {
                        ++enters;
                    } else if (event.kind == conjure::physics::contact_kind::TRIGGER_EXIT) {
                        ++exits;
                    }
                }
            }
        }
    };

    /// <summary>
    /// The engine's view of the physics systems: an ecs::scheduler over a fixture's world with the four systems
    /// of physics::register_systems, stages inline (job_system::schedule would allocate its counter) and chunks
    /// parallel over the fixture's job system, which is how physics.pull_transforms runs in engine::application
    /// (14_physics.md 3.7 PARALLEL). 'minEntitiesPerJob' sets the grain; 1 makes every chunk its own job.
    /// </summary>
    class scheduled_physics {
        public:
            conjure::ecs::scheduler scheduler;   // no default: bound to the fixture's world
            conjure::ecs::frame_info frame = {};

        public:
            explicit scheduled_physics(physics_fixture &fixture, conjure::u32 minEntitiesPerJob = 1024) :
                scheduler(
                  fixture.world,
                  fixture.jobs->worker_count() == 0 ? nullptr : fixture.jobs,
                  make_desc(minEntitiesPerJob)
                ) {
                CONJURE_REQUIRE_OK(conjure::physics::register_systems(scheduler, fixture.physics));
                CONJURE_REQUIRE_OK(scheduler.finalise());
                frame.deltaTime = STEP;
                frame.fixedDeltaTime = STEP;
                frame.inFixedStep = true;
            }

            ~scheduled_physics() {}

            scheduled_physics(const scheduled_physics &) = delete;
            scheduled_physics &operator=(const scheduled_physics &) = delete;

        public:
            /// <summary>
            /// One fixed step as the engine runs it: PHYSICS_STEP (push, step) then FIXED_END (pull, contacts).
            /// </summary>
            void run_step() {
                ++frame.frameIndex;
                scheduler.run_phase(conjure::ecs::system_phase::PHYSICS_STEP, frame);
                scheduler.run_phase(conjure::ecs::system_phase::FIXED_END, frame);
            }

            void run_steps(conjure::u32 count) {
                for (conjure::u32 index = 0; index < count; ++index) {
                    run_step();
                }
            }

            /// <summary>
            /// FIXED_END alone: the PARALLEL pull followed by the contact collection.
            /// </summary>
            void run_fixed_end() {
                ++frame.frameIndex;
                scheduler.run_phase(conjure::ecs::system_phase::FIXED_END, frame);
            }

        private:
            [[nodiscard]] static conjure::ecs::scheduler_desc make_desc(conjure::u32 minEntitiesPerJob) noexcept {
                conjure::ecs::scheduler_desc desc = {};
                desc.parallelSystems = false;
                desc.parallelChunks = true;
                desc.minEntitiesPerJob = minEntitiesPerJob;
                return desc;
            }
    };

    /// <summary>
    /// Closed form of Jolt's semi-implicit Euler free fall: after n steps of dt from rest the body has
    /// dropped g * dt^2 * n * (n + 1) / 2 (velocity is updated before the position every step).
    /// </summary>
    [[nodiscard]] inline conjure::f32 free_fall_drop(conjure::u32 steps, conjure::f32 dt = STEP) noexcept {
        const conjure::f32 n = static_cast<conjure::f32>(steps);
        return GRAVITY * dt * dt * n * (n + 1.0f) * 0.5f;
    }

    /// <summary>
    /// The design's allocation gate (08_testing.md 7.1, physics.sync_10k: 0) on the path the engine takes: the
    /// PARALLEL pull through a scheduler, then the serial fallback, over 'bodyCount' boxes falling freely (no
    /// ground, so no contacts, and every step writes every row). Step 1 creates the bodies and makes the first
    /// pull, step 2 warms the scheduler's scratch, the job pool and the query's match list; the FIXED_END and
    /// the pull that follow must allocate nothing. Shared by the 2,000-body case of the default run and the
    /// 10,000-body [slow] case of conjure_physics_slow_tests.
    /// </summary>
    inline void check_pull_allocation_free(conjure::core::jobs::job_system &jobs, conjure::u32 bodyCount) {
        conjure::physics::physics_settings settings = {};
        settings.allowSleeping = false;
        physics_fixture fixture(jobs, settings);
        scheduled_physics schedule(fixture);
        std::vector<conjure::ecs::entity> boxes = {};
        boxes.reserve(bodyCount);
        for (conjure::u32 index = 0; index < bodyCount; ++index) {
            const conjure::f32 x = static_cast<conjure::f32>(index % 100) * 2.0f - 99.0f;
            const conjure::f32 z = static_cast<conjure::f32>(index / 100) * 2.0f - 99.0f;
            boxes.push_back(fixture.spawn_box(conjure::math::vec3(x, 50.0f, z)));
        }
        schedule.run_steps(2);
        REQUIRE(fixture.physics.body_count() == bodyCount);

        {
            conjure::testing::allocation_scope scope = {};
            schedule.run_fixed_end();
            const conjure::u64 allocations = scope.allocation_count();
            CHECK(allocations == 0);
        }
        {
            conjure::testing::allocation_scope scope = {};
            fixture.physics.pull_transforms();
            const conjure::u64 allocations = scope.allocation_count();
            CHECK(allocations == 0);
        }
        // Every row was written: the boxes fell for two steps.
        const conjure::f32 expected = 50.0f - free_fall_drop(2);
        CHECK(fixture.transform_position(boxes.front()).y == doctest::Approx(expected).epsilon(1.0e-5));
        CHECK(fixture.transform_position(boxes.back()).y == doctest::Approx(expected).epsilon(1.0e-5));
        const conjure::physics::rigid_body *pulled = fixture.world.get<conjure::physics::rigid_body>(boxes.back());
        REQUIRE(pulled != nullptr);
        CHECK(pulled->linearVelocity.y < 0.0f);
        CHECK_FALSE(pulled->isSleeping);
    }
}
