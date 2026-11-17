#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/assert_capture.h"
#include "conjure/testing/doctest.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/memory_guard.h"
#include "physics_test_fixture.h"

#include "conjure/core/jobs/dedicated_thread.h"
#include "conjure/core/math/transform.h"
#include "conjure/core/random.h"
#include "conjure/core/result.h"
#include "conjure/physics/components.h"
#include "conjure/physics/physics_error.h"
#include "conjure/physics/physics_world.h"

#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <vector>

using namespace conjure;
using namespace physics_test;

namespace {

    /// <summary>
    /// The entity-keyed commands of physics_world. character_move is the last one: it is refused with its own
    /// code (NOT_A_CHARACTER) where the others report WRONG_BODY_KIND.
    /// </summary>
    enum class command : u8 {
        ADD_FORCE,
        ADD_FORCE_AT,
        ADD_IMPULSE,
        ADD_IMPULSE_AT,
        ADD_TORQUE,
        ADD_ANGULAR_IMPULSE,
        SET_LINEAR_VELOCITY,
        SET_ANGULAR_VELOCITY,
        MOVE_KINEMATIC,
        TELEPORT,
        WAKE,
        SLEEP,
        CHARACTER_MOVE,
        COUNT
    };

    constexpr u32 COMMAND_COUNT = static_cast<u32>(command::COUNT);

    constexpr std::array<const char *, COMMAND_COUNT> COMMAND_NAMES = {
        "add_force",
        "add_force_at",
        "add_impulse",
        "add_impulse_at",
        "add_torque",
        "add_angular_impulse",
        "set_linear_velocity",
        "set_angular_velocity",
        "move_kinematic",
        "teleport",
        "wake",
        "sleep",
        "character_move",
    };

    [[nodiscard]] constexpr u32 bit_of(command which) noexcept {
        return 1u << static_cast<u32>(which);
    }

    constexpr u32 FORCES = bit_of(command::ADD_FORCE) | bit_of(command::ADD_FORCE_AT) | bit_of(command::ADD_IMPULSE)
      | bit_of(command::ADD_IMPULSE_AT) | bit_of(command::ADD_TORQUE) | bit_of(command::ADD_ANGULAR_IMPULSE);
    constexpr u32 VELOCITIES = bit_of(command::SET_LINEAR_VELOCITY) | bit_of(command::SET_ANGULAR_VELOCITY);
    // teleport moves every kind; wake and sleep succeed on every kind (no-ops where nothing sleeps).
    constexpr u32 EVERY_KIND = bit_of(command::TELEPORT) | bit_of(command::WAKE) | bit_of(command::SLEEP);
    constexpr u32 ALL_COMMANDS = (1u << COMMAND_COUNT) - 1u;

    [[nodiscard]] core::result<void> send(
      physics::physics_world &physics,
      command which,
      ecs::entity target,
      const math::transform &pose
    ) {
        const math::vec3 value = math::vec3(0.0f, 0.0f, 0.25f);
        switch (which) {
            case command::ADD_FORCE:
                return physics.add_force(target, value);
            case command::ADD_FORCE_AT:
                return physics.add_force_at(target, value, pose.position);
            case command::ADD_IMPULSE:
                return physics.add_impulse(target, value);
            case command::ADD_IMPULSE_AT:
                return physics.add_impulse_at(target, value, pose.position);
            case command::ADD_TORQUE:
                return physics.add_torque(target, value);
            case command::ADD_ANGULAR_IMPULSE:
                return physics.add_angular_impulse(target, value);
            case command::SET_LINEAR_VELOCITY:
                return physics.set_linear_velocity(target, value);
            case command::SET_ANGULAR_VELOCITY:
                return physics.set_angular_velocity(target, value);
            case command::MOVE_KINEMATIC:
                return physics.move_kinematic(target, pose);
            case command::TELEPORT:
                return physics.teleport(target, pose);
            case command::WAKE:
                return physics.wake(target);
            case command::SLEEP:
                return physics.sleep(target);
            case command::CHARACTER_MOVE:
                return physics.character_move(target, math::vec3(0.0f));
            case command::COUNT:
                break;
        }
        return core::error(core::error_code::INVALID_ARGUMENT, "not a command");
    }

    /// <summary>
    /// Sends every command to 'target' (poses name 'pose', so an applied teleport or move changes nothing) and
    /// checks that the ones in 'accepted' are queued and every other one is refused: with WRONG_BODY_KIND,
    /// character_move with NOT_A_CHARACTER.
    /// </summary>
    void send_every_command(physics_fixture &fixture, ecs::entity target, const math::transform &pose, u32 accepted) {
        for (u32 index = 0; index < COMMAND_COUNT; ++index) {
            CAPTURE(COMMAND_NAMES[index]);
            const command which = static_cast<command>(index);
            const core::result<void> outcome = send(fixture.physics, which, target, pose);
            if ((accepted & (1u << index)) != 0) {
                CONJURE_CHECK_OK(outcome);
            } else if (which == command::CHARACTER_MOVE) {
                CONJURE_CHECK_ERROR(outcome, physics::physics_error::NOT_A_CHARACTER);
            } else {
                CONJURE_CHECK_ERROR(outcome, physics::physics_error::WRONG_BODY_KIND);
            }
        }
    }

    /// <summary>
    /// Where the bodies of the command tables stand.
    /// </summary>
    [[nodiscard]] math::vec3 kind_position() noexcept {
        return math::vec3(0.0f, 5.0f, 0.0f);
    }

    /// <summary>
    /// A body of one kind for the command tables, pending until the next push: the entity and the commands
    /// that can act on that kind of body.
    /// </summary>
    struct body_kind {
        ecs::entity entity = {};
        u32 applies = 0;
    };

    [[nodiscard]] ecs::entity spawn_collider_only(physics_fixture &fixture) {
        const ecs::entity wall = fixture.create_at(kind_position());
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(wall));
        return wall;
    }

    /// <summary>
    /// One body of every kind a command can meet, each a SUBCASE of the calling case.
    /// </summary>
    [[nodiscard]] body_kind spawn_each_kind(physics_fixture &fixture) {
        body_kind kind = {};
        SUBCASE("a static body") {
            kind = {fixture.spawn_box(kind_position(), physics::motion_type::STATIC), EVERY_KIND};
        }
        SUBCASE("colliders without a rigid_body") {
            kind = {spawn_collider_only(fixture), EVERY_KIND};
        }
        SUBCASE("a kinematic body") {
            const ecs::entity plate = fixture.spawn_box(kind_position(), physics::motion_type::KINEMATIC);
            kind = {plate, EVERY_KIND | VELOCITIES | bit_of(command::MOVE_KINEMATIC)};
        }
        SUBCASE("a kinematic body that follows its transform") {
            kind = {fixture.spawn_following_plate(kind_position()), EVERY_KIND | VELOCITIES};
        }
        SUBCASE("a dynamic body") {
            kind = {fixture.spawn_box(kind_position()), EVERY_KIND | VELOCITIES | FORCES};
        }
        SUBCASE("a character") {
            kind = {fixture.spawn_character(kind_position()), EVERY_KIND | bit_of(command::CHARACTER_MOVE)};
        }
        return kind;
    }
}

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
            // The pool is started by for_each_job_mode and job_system::start() returns with every
            // worker running, so the only warm-up the gate needs is Jolt's own first step.
            physics_fixture fixture(jobs);
            fixture.run_steps(1);
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

    TEST_CASE("physics_world - detach removes the interpolated_transform physics added") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const ecs::entity ground = fixture.spawn_ground();
        const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 0.5f, 0.0f));
        const ecs::entity character = fixture.spawn_character(math::vec3(4.0f, 0.0f, 0.0f));
        // A dynamic body without a collider stays pending, yet already carries the component.
        const ecs::entity shapeless = fixture.create_at(math::vec3(9.0f, 1.0f, 0.0f));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(shapeless));
        // Got the component as a pending dynamic body, then switched to STATIC while still pending.
        const ecs::entity switched = fixture.create_at(math::vec3(12.0f, 1.0f, 0.0f));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(switched));
        const ecs::entity wall = fixture.spawn_box(math::vec3(-5.0f, 1.0f, 0.0f), physics::motion_type::STATIC);
        fixture.run_step();
        // Added by the user to a static body: physics never added it, so it never removes it.
        CONJURE_REQUIRE_OK(fixture.world.add<scene::interpolated_transform>(wall));
        REQUIRE(fixture.world.has<scene::interpolated_transform>(box));
        REQUIRE(fixture.world.has<scene::interpolated_transform>(character));
        REQUIRE(fixture.world.has<scene::interpolated_transform>(shapeless));
        REQUIRE(fixture.world.has<scene::interpolated_transform>(switched));
        REQUIRE_FALSE(fixture.world.has<scene::interpolated_transform>(ground));
        physics::rigid_body *switchedBody = fixture.world.get<physics::rigid_body>(switched);
        REQUIRE(switchedBody != nullptr);
        switchedBody->motionType = physics::motion_type::STATIC;

        fixture.physics.detach();
        CHECK_FALSE(fixture.world.has<scene::interpolated_transform>(box));
        CHECK_FALSE(fixture.world.has<scene::interpolated_transform>(character));
        CHECK_FALSE(fixture.world.has<scene::interpolated_transform>(shapeless));
        CHECK_FALSE(fixture.world.has<scene::interpolated_transform>(switched));
        CHECK_FALSE(fixture.world.has<scene::interpolated_transform>(ground));
        CHECK(fixture.world.has<scene::interpolated_transform>(wall));
        CONJURE_CHECK_OK(fixture.world.validate());
    }

    TEST_CASE("physics_world - attach off the owner thread asserts and returns WRONG_THREAD") {
        core::jobs::job_system jobs = {};
        ecs::component_registry registry = {};
        register_physics_test_components(registry);
        ecs::world world(registry);
        physics::physics_world physics(test_settings(), jobs);
        testing::expect_assert guard = {};
        std::atomic<bool> wrongThread = false;
        {
            core::jobs::dedicated_thread helper("physics-test", [&](std::stop_token) {
                const core::result<void> outcome = physics.attach(world);
                wrongThread = !outcome.has_value() && outcome.get_error().is(physics::physics_error::WRONG_THREAD);
            });
            helper.join();
        }
        CHECK(wrongThread.load());
        CHECK(guard.count() == ASSERTS_PER_VIOLATION);
        CHECK_FALSE(physics.is_attached());
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
        // The thread is checked before the entity is looked at (its kind is read from ECS components, which
        // only the owner thread may do): a command the box would take and one it would refuse both report
        // the thread.
        bool characterMove = false;
        SUBCASE("a command the body would take") {
            characterMove = false;
        }
        SUBCASE("a command the body would refuse") {
            characterMove = true;
        }
        testing::expect_assert guard = {};
        std::atomic<bool> wrongThread = false;
        {
            core::jobs::dedicated_thread helper("physics-test", [&](std::stop_token) {
                const math::vec3 value = math::vec3(0.0f, 1.0f, 0.0f);
                const core::result<void> outcome =
                  characterMove ? fixture.physics.character_move(box, value) : fixture.physics.add_force(box, value);
                wrongThread = !outcome.has_value() && outcome.get_error().is(physics::physics_error::WRONG_THREAD);
            });
            helper.join();
        }
        CHECK(wrongThread.load());
        CHECK(guard.count() == ASSERTS_PER_VIOLATION);
    }

    TEST_CASE("physics_world - body commands with a non-finite vector are INVALID_ARGUMENT and leave the body finite") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 5.0f, 0.0f));
        fixture.run_step();
        const f32 nan = std::numeric_limits<f32>::quiet_NaN();
        const f32 infinity = std::numeric_limits<f32>::infinity();
        CONJURE_CHECK_ERROR(
          fixture.physics.add_force(box, math::vec3(nan, 0.0f, 0.0f)),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          fixture.physics.add_impulse_at(box, math::vec3(0.0f, 1.0f, 0.0f), math::vec3(infinity, 0.0f, 0.0f)),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(
          fixture.physics.set_gravity(math::vec3(0.0f, -infinity, 0.0f)),
          core::error_code::INVALID_ARGUMENT
        );
        math::transform pose = {};
        pose.position = math::vec3(1.0f, 5.0f, 0.0f);
        pose.rotation = math::quat(nan, 0.0f, 0.0f, 0.0f);
        CONJURE_CHECK_ERROR(fixture.physics.teleport(box, pose), core::error_code::INVALID_ARGUMENT);
        // The value is checked before the kind of body: a NaN is INVALID_ARGUMENT whatever it was sent to.
        CONJURE_CHECK_ERROR(
          fixture.physics.character_move(box, math::vec3(nan, 0.0f, 0.0f)),
          core::error_code::INVALID_ARGUMENT
        );
        CONJURE_CHECK_ERROR(fixture.physics.move_kinematic(box, pose), core::error_code::INVALID_ARGUMENT);

        fixture.run_step();
        const math::vec3 position = fixture.position_of(box);
        CHECK(std::isfinite(position.x));
        CHECK(std::isfinite(position.y));
        CHECK(std::isfinite(position.z));
        CHECK(position.x == doctest::Approx(0.0f));
        CHECK(fixture.physics.gravity() == math::vec3(0.0f, -9.81f, 0.0f));
        CHECK(fixture.physics.stats().droppedCommands == 0);
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

    TEST_CASE("physics_world - a command the kind of body cannot take is WRONG_BODY_KIND at the call") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.gravity = math::vec3(0.0f);
        physics_fixture fixture(jobs, settings);
        const body_kind kind = spawn_each_kind(fixture);
        fixture.run_step();
        REQUIRE(fixture.physics.has_body(kind.entity));
        const std::optional<math::transform> pose = fixture.physics.body_pose(kind.entity);
        REQUIRE(pose.has_value());
        send_every_command(fixture, kind.entity, *pose, kind.applies);

        // What was refused was never queued, and what was queued applies (or is the no-op that wake and
        // sleep are where nothing sleeps): the push drops nothing.
        fixture.run_step();
        CHECK(fixture.physics.stats().droppedCommands == 0);
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("regression - code sweep F413 a command a pending body cannot take is dropped and counted") {
        // The entity has no body at the call, so there is no kind to refuse by: everything is accepted and the
        // push that creates the body drops what does not apply. Forces on a body that is not dynamic and
        // velocities on a static one used to vanish in Jolt without a count.
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.gravity = math::vec3(0.0f);
        physics_fixture fixture(jobs, settings);
        const body_kind kind = spawn_each_kind(fixture);
        REQUIRE_FALSE(fixture.physics.has_body(kind.entity));
        math::transform pose = {};
        pose.position = kind_position();
        send_every_command(fixture, kind.entity, pose, ALL_COMMANDS);
        CHECK(fixture.physics.stats().droppedCommands == 0);

        fixture.run_step();
        REQUIRE(fixture.physics.has_body(kind.entity));
        const u32 inapplicable = COMMAND_COUNT - static_cast<u32>(std::popcount(kind.applies));
        CHECK(fixture.physics.stats().droppedCommands == inapplicable);
        // Dropped once, not carried into the next push.
        fixture.run_step();
        CHECK(fixture.physics.stats().droppedCommands == inapplicable);
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("regression - code sweep F413 the kind of body is read from the components, not from the last push") {
        // The push applies a motion type written into rigid_body, and a rigid_body added to or taken from
        // colliders, before it applies the commands. Refusing by the row of the last push would turn away a
        // force sent in the frame that makes a body dynamic.
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.gravity = math::vec3(0.0f);
        physics_fixture fixture(jobs, settings);
        const math::vec3 impulse = math::vec3(2.0f, 0.0f, 0.0f);
        math::transform pose = {};
        pose.position = kind_position();

        SUBCASE("a static body switched to DYNAMIC takes an impulse in the same frame") {
            const ecs::entity box = fixture.spawn_box(kind_position(), physics::motion_type::STATIC);
            fixture.run_step();
            CONJURE_CHECK_ERROR(fixture.physics.add_impulse(box, impulse), physics::physics_error::WRONG_BODY_KIND);
            physics::rigid_body *authored = fixture.world.get<physics::rigid_body>(box);
            REQUIRE(authored != nullptr);
            authored->motionType = physics::motion_type::DYNAMIC;
            CONJURE_CHECK_OK(fixture.physics.add_impulse(box, impulse));
            fixture.run_step();
            const physics::rigid_body *pulled = fixture.world.get<physics::rigid_body>(box);
            REQUIRE(pulled != nullptr);
            // 2 N s on 1 kg.
            CHECK(pulled->linearVelocity.x == doctest::Approx(2.0f).epsilon(0.05));
            CHECK(fixture.physics.stats().droppedCommands == 0);
        }
        SUBCASE("colliders given a dynamic rigid_body take an impulse in the same frame") {
            const ecs::entity crate = spawn_collider_only(fixture);
            fixture.run_step();
            CONJURE_CHECK_ERROR(fixture.physics.add_impulse(crate, impulse), physics::physics_error::WRONG_BODY_KIND);
            CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(crate));
            CONJURE_CHECK_OK(fixture.physics.add_impulse(crate, impulse));
            fixture.run_step();
            const physics::rigid_body *pulled = fixture.world.get<physics::rigid_body>(crate);
            REQUIRE(pulled != nullptr);
            CHECK(pulled->linearVelocity.x == doctest::Approx(2.0f).epsilon(0.05));
            CHECK(fixture.physics.stats().droppedCommands == 0);
        }
        SUBCASE("a dynamic body switched to STATIC refuses forces and velocities at once") {
            const ecs::entity box = fixture.spawn_box(kind_position());
            fixture.run_step();
            CONJURE_CHECK_OK(fixture.physics.add_impulse(box, impulse));
            physics::rigid_body *authored = fixture.world.get<physics::rigid_body>(box);
            REQUIRE(authored != nullptr);
            authored->motionType = physics::motion_type::STATIC;
            CONJURE_CHECK_ERROR(fixture.physics.add_impulse(box, impulse), physics::physics_error::WRONG_BODY_KIND);
            CONJURE_CHECK_ERROR(
              fixture.physics.set_linear_velocity(box, impulse),
              physics::physics_error::WRONG_BODY_KIND
            );
            fixture.run_step();
            // The impulse accepted while the component still said DYNAMIC met a static body: the late drop.
            CHECK(fixture.physics.stats().droppedCommands == 1);
            CHECK(fixture.position_of(box) == kind_position());
        }
        SUBCASE("a dynamic body whose rigid_body is removed refuses forces at once") {
            const ecs::entity box = fixture.spawn_box(kind_position());
            fixture.run_step();
            CONJURE_REQUIRE_OK(fixture.world.remove<physics::rigid_body>(box));
            REQUIRE(fixture.physics.has_body(box));
            CONJURE_CHECK_ERROR(fixture.physics.add_impulse(box, impulse), physics::physics_error::WRONG_BODY_KIND);
            CONJURE_CHECK_OK(fixture.physics.teleport(box, pose));
            fixture.run_step();
            CHECK(fixture.physics.stats().droppedCommands == 0);
        }
        SUBCASE("a kinematic body told to follow its transform refuses move_kinematic at once") {
            const ecs::entity plate = fixture.spawn_box(kind_position(), physics::motion_type::KINEMATIC);
            fixture.run_step();
            CONJURE_CHECK_OK(fixture.physics.move_kinematic(plate, pose));
            physics::rigid_body *authored = fixture.world.get<physics::rigid_body>(plate);
            REQUIRE(authored != nullptr);
            authored->flags |= physics::rigid_body_flags::KINEMATIC_MOVES_WITH_TRANSFORM;
            CONJURE_CHECK_ERROR(fixture.physics.move_kinematic(plate, pose), physics::physics_error::WRONG_BODY_KIND);
            fixture.run_step();
            // The move accepted while the body was a plain kinematic one met a body that follows its transform.
            CHECK(fixture.physics.stats().droppedCommands == 1);
        }
        CONJURE_CHECK_OK(fixture.physics.validate());
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
        const ecs::entity wall = fixture.spawn_box(math::vec3(5.0f, 20.0f, 0.0f), physics::motion_type::STATIC);
        fixture.run_step();
        const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 20.0f, 0.0f));
        CONJURE_CHECK_OK(fixture.physics.add_force(box, math::vec3(1.0f)));
        CONJURE_CHECK_OK(fixture.physics.add_torque(box, math::vec3(1.0f)));
        CONJURE_CHECK_ERROR(
          fixture.physics.add_impulse(box, math::vec3(1.0f)),
          physics::physics_error::COMMAND_QUEUE_FULL
        );
        CHECK(fixture.physics.stats().droppedCommands == 1);
        CHECK(logs.count(core::log_level::WARN) == 1);
        // The kind of body is checked before the queue: with the queue full, a command the body cannot take
        // is still refused as such, and a refusal is not a drop.
        CONJURE_CHECK_ERROR(fixture.physics.add_force(wall, math::vec3(1.0f)), physics::physics_error::WRONG_BODY_KIND);
        CONJURE_CHECK_ERROR(
          fixture.physics.character_move(wall, math::vec3(1.0f)),
          physics::physics_error::NOT_A_CHARACTER
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
