#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/assert_capture.h"
#include "conjure/testing/doctest.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/memory_guard.h"
#include "physics_test_fixture.h"

#include "conjure/core/jobs/dedicated_thread.h"
#include "conjure/core/random.h"
#include "conjure/physics/physics_error.h"
#include "conjure/physics/physics_world.h"

#include <atomic>
#include <memory>
#include <random>
#include <vector>

using namespace conjure;
using namespace physics_test;

TEST_SUITE("physics/physics_world") {

    TEST_CASE("physics_world - constructing and destroying ten worlds leaves no allocation behind") {
        core::jobs::job_system jobs = {};
        // The first world initialises process-wide Jolt state and the reflection registry; warm up once.
        {
            physics::physics_world warmup(test_settings(), jobs);
            CONJURE_CHECK_OK(warmup.validate());
        }
        testing::memory_guard guard = {};
        for (u32 index = 0; index < 10; ++index) {
            physics::physics_world world(test_settings(), jobs);
            CHECK_FALSE(world.is_attached());
            CHECK(world.body_count() == 0);
            CHECK(world.active_body_count() == 0);
            CHECK(world.settings().maxBodies == test_settings().maxBodies);
            CHECK(world.layers().count() == 1);
            CHECK(world.gravity() == math::vec3(0.0f, -9.81f, 0.0f));
            CHECK_FALSE(world.bounds().is_valid());
            CONJURE_CHECK_OK(world.validate());
        }
        guard.check();
    }

    TEST_CASE("physics_world - invalid settings are replaced by defaults with an ERROR") {
        core::jobs::job_system jobs = {};
        testing::log_capture logs = {};
        logs.allow_errors();
        physics::physics_settings settings = test_settings();
        settings.maxJobs = 1;
        physics::physics_world world(settings, jobs);
        CHECK(logs.count(core::log_level::ERR) == 1);
        CHECK(world.settings().maxJobs == physics::physics_settings().maxJobs);
        CONJURE_CHECK_OK(world.validate());
    }

    TEST_CASE("physics_world - Jolt allocations are visible to allocation_scope") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        for (u32 index = 0; index < 100; ++index) {
            (void)fixture.spawn_box(math::vec3(
              static_cast<f32>(index % 10) * 2.0f,
              2.0f + static_cast<f32>(index / 10) * 2.0f,
              0.0f
            ));
        }
        testing::allocation_scope scope = {};
        fixture.physics.push_transforms(STEP);
        CHECK(scope.allocation_count() > 0);
        CHECK(fixture.physics.body_count() == 100);
    }

    TEST_CASE("physics_world - 600 empty steps allocate nothing after the first") {
        for_each_job_mode([](core::jobs::job_system &jobs) {
            physics_fixture fixture(jobs);
            fixture.run_steps(2);
            testing::allocation_scope scope = {};
            fixture.run_steps(600);
            const u64 allocations = scope.allocation_count();
            CHECK(allocations == 0);
            CHECK(fixture.physics.contact_events().empty());
        });
    }

    TEST_CASE("physics_world - attach twice is ALREADY_ATTACHED and detach restores the world") {
        core::jobs::job_system jobs = {};
        ecs::component_registry registry = {};
        register_physics_test_components(registry);
        ecs::world world(registry);
        physics::physics_world physics(test_settings(), jobs);
        CONJURE_REQUIRE_OK(physics.attach(world));
        CHECK(physics.is_attached());
        CONJURE_CHECK_ERROR(physics.attach(world), physics::physics_error::ALREADY_ATTACHED);

        const core::result<ecs::entity> created = world.create_entity();
        REQUIRE(created.has_value());
        CONJURE_REQUIRE_OK(world.add<scene::local_transform>(created.value(), at(math::vec3(0.0f, 1.0f, 0.0f))));
        CONJURE_REQUIRE_OK(world.add<physics::rigid_body>(created.value()));
        CONJURE_REQUIRE_OK(world.add<physics::box_collider>(created.value()));
        physics.push_transforms(STEP);
        CHECK(physics.body_count() == 1);
        CHECK(world.has<scene::interpolated_transform>(created.value()));

        physics.detach();
        CHECK_FALSE(physics.is_attached());
        CHECK(physics.body_count() == 0);
        CHECK_FALSE(physics.has_body(created.value()));
        physics.detach();
        // Detaching is idempotent and a second attach works (existing bodies are picked up).
        CONJURE_REQUIRE_OK(physics.attach(world));
        physics.push_transforms(STEP);
        CHECK(physics.body_count() == 1);
        physics.detach();
        CONJURE_CHECK_OK(world.validate());
    }

    TEST_CASE("physics_world - attach needs the physics and scene components registered") {
        core::jobs::job_system jobs = {};
        ecs::component_registry registry = {};
        CONJURE_REQUIRE_OK(physics::register_components(registry));
        ecs::world world(registry);
        physics::physics_world physics(test_settings(), jobs);
        CONJURE_CHECK_ERROR(physics.attach(world), core::error_code::INVALID_STATE);
        CHECK_FALSE(physics.is_attached());
    }

    TEST_CASE("physics_world - the pipeline is a no-op before attach") {
        core::jobs::job_system jobs = {};
        physics::physics_world physics(test_settings(), jobs);
        physics.push_transforms(STEP);
        physics.step(STEP);
        physics.pull_transforms();
        physics.collect_contact_events();
        CHECK(physics.contact_events().empty());
        CHECK(physics.stats().bodies == 0);
        CONJURE_CHECK_OK(physics.validate());
    }

    TEST_CASE("physics_world - validate passes after random create and destroy") {
        for_each_job_mode([](core::jobs::job_system &jobs) {
            physics_fixture fixture(jobs);
            std::mt19937_64 random(testing::environment().seed);
            std::vector<ecs::entity> alive = {};
            for (u32 round = 0; round < 20; ++round) {
                for (u32 index = 0; index < 10; ++index) {
                    const f32 x = static_cast<f32>(random() % 40) - 20.0f;
                    const f32 z = static_cast<f32>(random() % 40) - 20.0f;
                    alive.push_back(fixture.spawn_box(math::vec3(x, 5.0f + static_cast<f32>(index), z)));
                }
                fixture.run_step();
                for (u32 index = 0; index < 4 && !alive.empty(); ++index) {
                    const usize victim = static_cast<usize>(random() % alive.size());
                    CONJURE_REQUIRE_OK(fixture.world.destroy_entity(alive[victim]));
                    alive[victim] = alive.back();
                    alive.pop_back();
                }
                CONJURE_CHECK_OK(fixture.physics.validate());
                CHECK(fixture.physics.body_count() == alive.size());
            }
            fixture.run_steps(5);
            CONJURE_CHECK_OK(fixture.physics.validate());
        });
    }

    TEST_CASE("physics_world - body_pose equals Jolt truth and the pulled transform") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const ecs::entity box = fixture.spawn_box(math::vec3(1.0f, 5.0f, -2.0f));
        CHECK_FALSE(fixture.physics.body_pose(box).has_value());
        fixture.run_steps(5);
        const std::optional<math::transform> pose = fixture.physics.body_pose(box);
        REQUIRE(pose.has_value());
        CHECK(pose->position.x == doctest::Approx(1.0f));
        CHECK(pose->position.z == doctest::Approx(-2.0f));
        CHECK(pose->position.y < 5.0f);
        CHECK(pose->position == fixture.transform_position(box));
        CHECK(pose->scale == math::vec3(1.0f));
        const math::aabb bounds = fixture.physics.bounds();
        CHECK(bounds.is_valid());
        CHECK(bounds.contains(pose->position));
    }

    TEST_CASE("physics_world - stats count bodies characters events and pending shapes") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.allowSleeping = false;
        physics_fixture fixture(jobs, settings);
        (void)fixture.spawn_ground();
        (void)fixture.spawn_box(math::vec3(0.0f, 0.5f, 0.0f));
        (void)fixture.spawn_character(math::vec3(4.0f, 0.0f, 0.0f));
        // A rigid_body without any collider stays pending.
        const ecs::entity shapeless = fixture.create_at(math::vec3(9.0f, 1.0f, 0.0f));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(shapeless));
        fixture.run_steps(5);
        const physics::physics_stats stats = fixture.physics.stats();
        CHECK(stats.bodies == 3);
        CHECK(stats.characters == 1);
        CHECK(stats.bodiesWithoutShape == 1);
        CHECK(stats.activeBodies >= 1);
        CHECK(stats.contactEvents == fixture.physics.contact_events().size());
        CHECK(stats.contactEvents > 0);
        CHECK(stats.droppedCommands == 0);
        CHECK(stats.droppedContactEvents == 0);
        CHECK(stats.updateErrors == 0);
        CHECK(stats.shapeCacheEntries >= 2);
        CHECK_FALSE(fixture.physics.has_body(shapeless));
        // Giving it a collider creates the body at the next push.
        CONJURE_REQUIRE_OK(fixture.world.add<physics::sphere_collider>(shapeless));
        fixture.run_step();
        CHECK(fixture.physics.has_body(shapeless));
        CHECK(fixture.physics.stats().bodiesWithoutShape == 0);
    }

    TEST_CASE("physics_world - off-thread body commands assert and return WRONG_THREAD") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 5.0f, 0.0f));
        fixture.run_step();
        testing::expect_assert guard = {};
        std::atomic<bool> wrongThread = false;
        {
            core::jobs::dedicated_thread helper("physics-test", [&](std::stop_token) {
                const core::result<void> outcome = fixture.physics.add_force(box, math::vec3(0.0f, 1.0f, 0.0f));
                wrongThread = !outcome.has_value() && outcome.get_error().is(physics::physics_error::WRONG_THREAD);
            });
            helper.join();
        }
        CHECK(wrongThread.load());
        CHECK(guard.count() == ASSERTS_PER_VIOLATION);
    }

    TEST_CASE("physics_world - body commands on an unknown entity are NO_BODY and pending ones are kept") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const ecs::entity nobody = fixture.create_at(math::vec3(0.0f));
        CONJURE_CHECK_ERROR(fixture.physics.add_force(nobody, math::vec3(1.0f)), physics::physics_error::NO_BODY);
        CONJURE_CHECK_ERROR(fixture.physics.teleport(nobody, {}), physics::physics_error::NO_BODY);
        CONJURE_CHECK_ERROR(fixture.physics.wake(nobody), physics::physics_error::NO_BODY);
        CONJURE_CHECK_ERROR(fixture.physics.sleep(nobody), physics::physics_error::NO_BODY);
        CONJURE_CHECK_ERROR(
          fixture.physics.character_move(nobody, math::vec3(1.0f)),
          physics::physics_error::NOT_A_CHARACTER
        );
        CHECK_FALSE(fixture.physics.is_sleeping(nobody));
        CHECK_FALSE(fixture.physics.character_ground(nobody).has_value());

        const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 20.0f, 0.0f));
        // The body does not exist yet: the command is accepted and applied after creation.
        CONJURE_CHECK_OK(fixture.physics.set_linear_velocity(box, math::vec3(5.0f, 0.0f, 0.0f)));
        fixture.run_step();
        const physics::rigid_body *body = fixture.world.get<physics::rigid_body>(box);
        REQUIRE(body != nullptr);
        CHECK(body->linearVelocity.x == doctest::Approx(5.0f).epsilon(0.01));
        CHECK(fixture.physics.stats().droppedCommands == 0);
    }

    TEST_CASE("physics_world - body components removed again before the push leave nothing behind") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const ecs::entity entity = fixture.create_at(math::vec3(0.0f, 1.0f, 0.0f));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(entity));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(entity));
        CONJURE_REQUIRE_OK(fixture.world.remove<physics::box_collider>(entity));
        CONJURE_REQUIRE_OK(fixture.world.remove<physics::rigid_body>(entity));
        // Withdrawn: a command on it is NO_BODY and the push creates nothing.
        CONJURE_CHECK_ERROR(fixture.physics.add_force(entity, math::vec3(1.0f)), physics::physics_error::NO_BODY);
        fixture.run_step();
        CHECK(fixture.physics.body_count() == 0);
        CHECK_FALSE(fixture.world.has<scene::interpolated_transform>(entity));
        CONJURE_CHECK_OK(fixture.physics.validate());

        // Withdrawn and re-added before the push: created once.
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(entity));
        CONJURE_REQUIRE_OK(fixture.world.remove<physics::rigid_body>(entity));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(entity));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(entity));
        CONJURE_CHECK_OK(fixture.physics.add_force(entity, math::vec3(1.0f)));
        fixture.run_step();
        CHECK(fixture.physics.body_count() == 1);
        CHECK(fixture.physics.has_body(entity));
        CHECK(fixture.physics.stats().droppedCommands == 0);
        // A pending entity destroyed before the push is dropped as well.
        const ecs::entity doomed = fixture.spawn_box(math::vec3(5.0f, 1.0f, 0.0f));
        CONJURE_REQUIRE_OK(fixture.world.destroy_entity(doomed));
        fixture.run_step();
        CHECK(fixture.physics.body_count() == 1);
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("physics_world - a full command queue reports COMMAND_QUEUE_FULL and counts the drop") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.maxQueuedCommands = 2;
        physics_fixture fixture(jobs, settings);
        testing::log_capture logs = {};
        const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 20.0f, 0.0f));
        CONJURE_CHECK_OK(fixture.physics.add_force(box, math::vec3(1.0f)));
        CONJURE_CHECK_OK(fixture.physics.add_torque(box, math::vec3(1.0f)));
        CONJURE_CHECK_ERROR(
          fixture.physics.add_impulse(box, math::vec3(1.0f)),
          physics::physics_error::COMMAND_QUEUE_FULL
        );
        CHECK(fixture.physics.stats().droppedCommands == 1);
        CHECK(logs.count(core::log_level::WARN) == 1);
        fixture.run_step();
        CONJURE_CHECK_OK(fixture.physics.add_impulse(box, math::vec3(1.0f)));
    }

    TEST_CASE("physics_world - set_gravity is applied at the next push and reported by gravity()") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 10.0f, 0.0f));
        CONJURE_CHECK_OK(fixture.physics.set_gravity(math::vec3(0.0f, 0.0f, 0.0f)));
        CHECK(fixture.physics.gravity() == math::vec3(0.0f, -9.81f, 0.0f));
        fixture.run_steps(10);
        CHECK(fixture.physics.gravity() == math::vec3(0.0f));
        CHECK(fixture.position_of(box).y == doctest::Approx(10.0f));
        CONJURE_CHECK_OK(fixture.physics.set_gravity(math::vec3(0.0f, -1.0f, 0.0f)));
        fixture.run_steps(60);
        CHECK(fixture.physics.gravity() == math::vec3(0.0f, -1.0f, 0.0f));
        CHECK(fixture.position_of(box).y < 10.0f);
        CHECK(fixture.position_of(box).y > 9.0f);
    }

    TEST_CASE("physics_world - collect_shapes drops cached shapes nobody references") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 1.0f, 0.0f));
        const ecs::entity sphere = fixture.spawn_sphere(math::vec3(3.0f, 1.0f, 0.0f));
        fixture.run_step();
        CHECK(fixture.physics.stats().shapeCacheEntries == 2);
        CONJURE_REQUIRE_OK(fixture.world.destroy_entity(sphere));
        fixture.physics.collect_shapes();
        CHECK(fixture.physics.stats().shapeCacheEntries == 1);
        CHECK(fixture.physics.has_body(box));
        fixture.physics.on_asset_reloaded(core::uuid());
        fixture.physics.on_asset_reloaded(core::uuid(1, 2));
        fixture.run_step();
        CHECK(fixture.physics.body_count() == 1);
    }
}
