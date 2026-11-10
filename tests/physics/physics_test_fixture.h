#pragma once

// Shared fixture of the physics tests (docs/design/14_physics.md 7, 10): a registry with the scene and
// physics components, an ecs::world, a physics_world built from test_settings() (small limits so overflow
// paths are testable), spawn helpers for boxes, spheres, ground and characters, run_steps() running
// push / step / pull / collect in order, and for_each_job_mode() running a body with 0 and 4 workers.

#include "conjure/testing/doctest.h"

#include "conjure/testing/test_environment.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/math/transform.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/types.h"
#include "conjure/ecs/command_buffer.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/entity.h"
#include "conjure/ecs/world.h"
#include "conjure/physics/character_controller.h"
#include "conjure/physics/components.h"
#include "conjure/physics/physics_settings.h"
#include "conjure/physics/physics_world.h"
#include "conjure/physics/register.h"
#include "conjure/scene/components.h"
#include "conjure/scene/register.h"

#include <algorithm>
#include <memory>
#include <optional>

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
    /// Closed form of Jolt's semi-implicit Euler free fall: after n steps of dt from rest the body has
    /// dropped g * dt^2 * n * (n + 1) / 2 (velocity is updated before the position every step).
    /// </summary>
    [[nodiscard]] inline conjure::f32 free_fall_drop(conjure::u32 steps, conjure::f32 dt = STEP) noexcept {
        const conjure::f32 n = static_cast<conjure::f32>(steps);
        return GRAVITY * dt * dt * n * (n + 1.0f) * 0.5f;
    }
}
