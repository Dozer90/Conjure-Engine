#include "conjure/testing/approx.h"
#include "conjure/testing/doctest.h"
#include "conjure/testing/log_capture.h"
#include "physics_test_fixture.h"

#include "conjure/physics/components.h"
#include "conjure/physics/physics_error.h"
#include "conjure/scene/components.h"

#include <array>
#include <cmath>
#include <optional>
#include <string>

using namespace conjure;
using namespace physics_test;

TEST_SUITE("physics/rigid_body") {

    TEST_CASE("rigid_body - free fall matches the semi-implicit Euler closed form within 1 cm") {
        // Jolt integrates v += g dt, then x += v dt (semi-implicit Euler), which after n steps gives a
        // drop of g dt^2 n (n + 1) / 2: 4.99 m after 60 steps where the continuous 0.5 g t^2 says
        // 4.905 m. The test checks the integrator's own closed form; damping is turned off so no other
        // term enters.
        for_each_job_mode([](core::jobs::job_system &jobs) {
            physics_fixture fixture(jobs);
            const ecs::entity box = fixture.create_at(math::vec3(0.0f, 10.0f, 0.0f));
            physics::rigid_body body = {};
            body.linearDamping = 0.0f;
            body.flags = physics::rigid_body_flags::NONE;
            CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(box, body));
            CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(box));
            fixture.run_steps(60);
            const f32 expected = 10.0f - free_fall_drop(60);
            CHECK(std::abs(fixture.position_of(box).y - expected) < 0.01f);
            CHECK(std::abs(fixture.transform_position(box).y - expected) < 0.01f);
            // The continuous solution is within the documented bound g dt^2 n / 2 (8.2 cm here).
            const f32 continuous = 10.0f - 0.5f * GRAVITY * 1.0f;
            CHECK(std::abs(fixture.position_of(box).y - continuous) < GRAVITY * STEP * STEP * 30.0f + 0.01f);
            const physics::rigid_body *pulled = fixture.world.get<physics::rigid_body>(box);
            REQUIRE(pulled != nullptr);
            CHECK(pulled->linearVelocity.y == doctest::Approx(-GRAVITY).epsilon(0.001));
            CHECK_FALSE(pulled->isSleeping);
        });
    }

    TEST_CASE("rigid_body - a kinematic plate driven by local_transform carries a dynamic box") {
        for_each_job_mode([](core::jobs::job_system &jobs) {
            physics_fixture fixture(jobs);
            const ecs::entity plate = fixture.create_at(math::vec3(0.0f, 0.0f, 0.0f));
            physics::rigid_body plateBody = {};
            plateBody.motionType = physics::motion_type::KINEMATIC;
            plateBody.flags = physics::rigid_body_flags::KINEMATIC_MOVES_WITH_TRANSFORM;
            CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(plate, plateBody));
            physics::box_collider plateShape = {};
            plateShape.halfExtents = math::vec3(5.0f, 0.5f, 5.0f);
            CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(plate, plateShape));
            const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 1.0f, 0.0f));
            fixture.run_steps(60);
            REQUIRE(fixture.physics.has_body(plate));
            CHECK(fixture.position_of(box).y == doctest::Approx(1.0f).epsilon(0.02));

            // Move the plate 3 m along x over 120 steps through its transform.
            for (u32 step = 0; step < 120; ++step) {
                scene::local_transform *local = fixture.world.get<scene::local_transform>(plate);
                REQUIRE(local != nullptr);
                local->position.x += 3.0f / 120.0f;
                fixture.run_step();
            }
            CHECK(fixture.position_of(plate).x == doctest::Approx(3.0f).epsilon(0.01));
            // The box rode along (friction) and did not fall through.
            CHECK(fixture.position_of(box).x > 2.0f);
            CHECK(fixture.position_of(box).y == doctest::Approx(1.0f).epsilon(0.05));
            // The pull never writes a kinematic-with-transform body's transform.
            CHECK(fixture.transform_position(plate).x == doctest::Approx(3.0f).epsilon(0.01));
        });
    }

    TEST_CASE("rigid_body - a kinematic plate driven by local_transform interpolates between its last two transforms") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const ecs::entity plate = fixture.create_at(math::vec3(0.0f, 5.0f, 0.0f));
        physics::rigid_body plateBody = {};
        plateBody.motionType = physics::motion_type::KINEMATIC;
        plateBody.flags = physics::rigid_body_flags::KINEMATIC_MOVES_WITH_TRANSFORM;
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(plate, plateBody));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(plate));
        fixture.run_step();
        const scene::interpolated_transform *interpolated = fixture.world.get<scene::interpolated_transform>(plate);
        REQUIRE(interpolated != nullptr);
        CHECK(interpolated->current.position == math::vec3(0.0f, 5.0f, 0.0f));
        CHECK(interpolated->previous.position == interpolated->current.position);

        // The pull never visits this row, so the push is what keeps the render pose following the transform.
        scene::local_transform *local = fixture.world.get<scene::local_transform>(plate);
        REQUIRE(local != nullptr);
        local->position.x += 1.0f;
        fixture.run_step();
        interpolated = fixture.world.get<scene::interpolated_transform>(plate);
        REQUIRE(interpolated != nullptr);
        CHECK(interpolated->current.position.x == doctest::Approx(1.0f));
        CHECK(interpolated->current.position.y == doctest::Approx(5.0f));
        CHECK(interpolated->previous.position.x == doctest::Approx(0.0f));
        const math::transform halfway = math::transform::lerp(interpolated->previous, interpolated->current, 0.5f);
        CHECK(halfway.position.x == doctest::Approx(0.5f));
    }

    TEST_CASE("rigid_body - a body that becomes static before its first push carries no interpolated_transform") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        SUBCASE("the rigid_body switches to STATIC") {
            const ecs::entity entity = fixture.create_at(math::vec3(0.0f, 1.0f, 0.0f));
            CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(entity));
            physics::rigid_body *body = fixture.world.get<physics::rigid_body>(entity);
            REQUIRE(body != nullptr);
            body->motionType = physics::motion_type::STATIC;
            CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(entity));
            fixture.run_step();
            CHECK(fixture.physics.has_body(entity));
            CHECK_FALSE(fixture.world.has<scene::interpolated_transform>(entity));
        }
        SUBCASE("the rigid_body goes and the colliders make an implicit static body") {
            const ecs::entity entity = fixture.create_at(math::vec3(0.0f, 1.0f, 0.0f));
            CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(entity));
            CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(entity));
            CONJURE_REQUIRE_OK(fixture.world.remove<physics::rigid_body>(entity));
            fixture.run_step();
            CHECK(fixture.physics.has_body(entity));
            CHECK_FALSE(fixture.world.has<scene::interpolated_transform>(entity));
        }
    }

    TEST_CASE("rigid_body - a velocity written into the component before a step becomes the body velocity") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.gravity = math::vec3(0.0f);
        physics_fixture fixture(jobs, settings);
        const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 5.0f, 0.0f));
        fixture.run_step();
        physics::rigid_body *body = fixture.world.get<physics::rigid_body>(box);
        REQUIRE(body != nullptr);
        body->linearVelocity = math::vec3(2.0f, 0.0f, 0.0f);
        body->angularVelocity = math::vec3(0.0f, 1.0f, 0.0f);
        fixture.run_steps(60);
        CHECK(fixture.position_of(box).x == doctest::Approx(2.0f).epsilon(0.06));
        const physics::rigid_body *pulled = fixture.world.get<physics::rigid_body>(box);
        REQUIRE(pulled != nullptr);
        CHECK(pulled->linearVelocity.x == doctest::Approx(2.0f).epsilon(0.06));
        CHECK(pulled->angularVelocity.y == doctest::Approx(1.0f).epsilon(0.06));
    }

    TEST_CASE("rigid_body - teleport resets velocities and interpolated_transform.previous") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 50.0f, 0.0f));
        fixture.run_steps(30);
        const scene::interpolated_transform *before = fixture.world.get<scene::interpolated_transform>(box);
        REQUIRE(before != nullptr);
        CHECK(before->previous.position.y > before->current.position.y);

        math::transform pose = {};
        pose.position = math::vec3(7.0f, 3.0f, 1.0f);
        pose.rotation = glm::angleAxis(0.5f, math::vec3(0.0f, 1.0f, 0.0f));
        pose.scale = math::vec3(4.0f);   // ignored
        CONJURE_REQUIRE_OK(fixture.physics.teleport(box, pose));
        fixture.physics.push_transforms(STEP);
        const scene::interpolated_transform *after = fixture.world.get<scene::interpolated_transform>(box);
        REQUIRE(after != nullptr);
        CHECK(after->previous.position == pose.position);
        CHECK(after->current.position == pose.position);
        const std::optional<math::transform> joltPose = fixture.physics.body_pose(box);
        REQUIRE(joltPose.has_value());
        CHECK(joltPose->position == pose.position);
        CHECK(testing::approx_equal(joltPose->rotation, pose.rotation));
        CHECK(joltPose->scale == math::vec3(1.0f));

        fixture.physics.step(STEP);
        fixture.physics.pull_transforms();
        fixture.physics.collect_contact_events();
        const physics::rigid_body *pulled = fixture.world.get<physics::rigid_body>(box);
        REQUIRE(pulled != nullptr);
        // One step of gravity only: the earlier fall velocity is gone.
        CHECK(pulled->linearVelocity.y == doctest::Approx(-GRAVITY * STEP).epsilon(0.01));
        CHECK(fixture.transform_position(box).x == doctest::Approx(7.0f));
    }

    TEST_CASE("rigid_body - mass 0 derives from the shape volume and validates") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const ecs::entity derived = fixture.create_at(math::vec3(0.0f, 5.0f, 0.0f));
        physics::rigid_body body = {};
        body.mass = 0.0f;
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(derived, body));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(derived));
        const ecs::entity fixed = fixture.spawn_box(math::vec3(3.0f, 5.0f, 0.0f));
        fixture.run_steps(30);
        CONJURE_CHECK_OK(fixture.physics.validate());
        // Gravity does not care about the mass: both fall the same.
        CHECK(fixture.position_of(derived).y == doctest::Approx(fixture.position_of(fixed).y));
        // But a 1 N impulse moves the heavier derived box (1 m^3 at Jolt's default density) less.
        CONJURE_REQUIRE_OK(fixture.physics.add_impulse(derived, math::vec3(100.0f, 0.0f, 0.0f)));
        CONJURE_REQUIRE_OK(fixture.physics.add_impulse(fixed, math::vec3(100.0f, 0.0f, 0.0f)));
        fixture.run_steps(10);
        const physics::rigid_body *derivedPulled = fixture.world.get<physics::rigid_body>(derived);
        const physics::rigid_body *fixedPulled = fixture.world.get<physics::rigid_body>(fixed);
        REQUIRE(derivedPulled != nullptr);
        REQUIRE(fixedPulled != nullptr);
        CHECK(derivedPulled->linearVelocity.x < fixedPulled->linearVelocity.x);
        CHECK(fixedPulled->linearVelocity.x == doctest::Approx(100.0f).epsilon(0.1));
    }

    TEST_CASE("rigid_body - set_linear_velocity on a pending body applies after creation") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.gravity = math::vec3(0.0f);
        physics_fixture fixture(jobs, settings);
        const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 5.0f, 0.0f));
        CHECK_FALSE(fixture.physics.has_body(box));
        CONJURE_CHECK_OK(fixture.physics.set_linear_velocity(box, math::vec3(0.0f, 0.0f, -3.0f)));
        fixture.run_steps(60);
        CHECK(fixture.position_of(box).z == doctest::Approx(-3.0f).epsilon(0.06));
        CHECK(fixture.physics.stats().droppedCommands == 0);
    }

    TEST_CASE("rigid_body - START_ASLEEP keeps the body still until wake") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const ecs::entity box = fixture.create_at(math::vec3(0.0f, 10.0f, 0.0f));
        physics::rigid_body body = {};
        body.flags = physics::rigid_body_flags::ALLOW_SLEEPING | physics::rigid_body_flags::START_ASLEEP;
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(box, body));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(box));
        fixture.run_steps(30);
        CHECK(fixture.physics.is_sleeping(box));
        CHECK(fixture.position_of(box).y == doctest::Approx(10.0f));
        const physics::rigid_body *pulled = fixture.world.get<physics::rigid_body>(box);
        REQUIRE(pulled != nullptr);
        CHECK(pulled->isSleeping);
        CONJURE_REQUIRE_OK(fixture.physics.wake(box));
        fixture.run_steps(30);
        CHECK_FALSE(fixture.physics.is_sleeping(box));
        CHECK(fixture.position_of(box).y < 10.0f);
        CONJURE_REQUIRE_OK(fixture.physics.sleep(box));
        const f32 frozen = fixture.position_of(box).y;
        fixture.run_steps(10);
        CHECK(fixture.physics.is_sleeping(box));
        CHECK(fixture.position_of(box).y == doctest::Approx(frozen));
    }

    TEST_CASE("rigid_body - LOCK_ROTATION keeps the rotation identity after an off-centre impulse") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.gravity = math::vec3(0.0f);
        physics_fixture fixture(jobs, settings);
        const ecs::entity locked = fixture.create_at(math::vec3(0.0f, 5.0f, 0.0f));
        physics::rigid_body body = {};
        body.flags = physics::rigid_body_flags::LOCK_ROTATION;
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(locked, body));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(locked));
        const ecs::entity free = fixture.spawn_box(math::vec3(5.0f, 5.0f, 0.0f));
        fixture.run_step();
        CONJURE_REQUIRE_OK(fixture.physics.add_impulse_at(
          locked,
          math::vec3(0.0f, 0.0f, 2.0f),
          math::vec3(0.5f, 5.0f, 0.0f)
        ));
        CONJURE_REQUIRE_OK(fixture.physics.add_impulse_at(
          free,
          math::vec3(0.0f, 0.0f, 2.0f),
          math::vec3(5.5f, 5.0f, 0.0f)
        ));
        fixture.run_steps(30);
        const std::optional<math::transform> lockedPose = fixture.physics.body_pose(locked);
        const std::optional<math::transform> freePose = fixture.physics.body_pose(free);
        REQUIRE(lockedPose.has_value());
        REQUIRE(freePose.has_value());
        CHECK(testing::approx_equal(lockedPose->rotation, math::quat(1.0f, 0.0f, 0.0f, 0.0f), 1.0e-4f));
        CHECK_FALSE(testing::approx_equal(freePose->rotation, math::quat(1.0f, 0.0f, 0.0f, 0.0f), 1.0e-3f));
        CHECK(lockedPose->position.z > 0.0f);
    }

    TEST_CASE("rigid_body - runtime changes of motion type and properties reach Jolt") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        (void)fixture.spawn_ground();
        const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 5.0f, 0.0f), physics::motion_type::STATIC);
        fixture.run_steps(10);
        CHECK(fixture.position_of(box).y == doctest::Approx(5.0f));
        CHECK_FALSE(fixture.world.has<scene::interpolated_transform>(box));

        physics::rigid_body *body = fixture.world.get<physics::rigid_body>(box);
        REQUIRE(body != nullptr);
        body->motionType = physics::motion_type::DYNAMIC;
        body->gravityFactor = 0.5f;
        fixture.run_steps(10);
        CHECK(fixture.world.has<scene::interpolated_transform>(box));
        CHECK(fixture.position_of(box).y < 5.0f);
        CHECK(fixture.position_of(box).y > 5.0f - free_fall_drop(10));

        body = fixture.world.get<physics::rigid_body>(box);
        REQUIRE(body != nullptr);
        body->motionType = physics::motion_type::STATIC;
        fixture.run_steps(2);
        CHECK_FALSE(fixture.world.has<scene::interpolated_transform>(box));
        const f32 frozen = fixture.position_of(box).y;
        fixture.run_steps(10);
        CHECK(fixture.position_of(box).y == doctest::Approx(frozen));
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("rigid_body - add_force add_torque and add_angular_impulse act on the body") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.gravity = math::vec3(0.0f);
        physics_fixture fixture(jobs, settings);
        const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 5.0f, 0.0f));
        fixture.run_step();
        for (u32 step = 0; step < 60; ++step) {
            CONJURE_REQUIRE_OK(fixture.physics.add_force(box, math::vec3(1.0f, 0.0f, 0.0f)));
            fixture.run_step();
        }
        const physics::rigid_body *pulled = fixture.world.get<physics::rigid_body>(box);
        REQUIRE(pulled != nullptr);
        // 1 N on 1 kg for 1 s is 1 m/s before damping.
        CHECK(pulled->linearVelocity.x == doctest::Approx(1.0f).epsilon(0.1));
        CONJURE_REQUIRE_OK(fixture.physics.add_angular_impulse(box, math::vec3(0.0f, 0.5f, 0.0f)));
        fixture.run_step();
        pulled = fixture.world.get<physics::rigid_body>(box);
        REQUIRE(pulled != nullptr);
        CHECK(pulled->angularVelocity.y > 0.0f);
        CONJURE_REQUIRE_OK(fixture.physics.add_torque(box, math::vec3(0.0f, -100.0f, 0.0f)));
        fixture.run_step();
        pulled = fixture.world.get<physics::rigid_body>(box);
        REQUIRE(pulled != nullptr);
        CHECK(pulled->angularVelocity.y < 0.0f);
        CONJURE_REQUIRE_OK(fixture.physics.set_angular_velocity(box, math::vec3(0.0f)));
        CONJURE_REQUIRE_OK(fixture.physics.add_force_at(
          box,
          math::vec3(0.0f, 0.0f, 50.0f),
          fixture.position_of(box) + math::vec3(0.5f, 0.0f, 0.0f)
        ));
        fixture.run_step();
        pulled = fixture.world.get<physics::rigid_body>(box);
        REQUIRE(pulled != nullptr);
        CHECK(pulled->angularVelocity.y != doctest::Approx(0.0f));
    }

    TEST_CASE("regression - review 1 a runtime mass change reaches Jolt") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.gravity = math::vec3(0.0f);
        physics_fixture fixture(jobs, settings);
        const ecs::entity box = fixture.create_at(math::vec3(0.0f, 5.0f, 0.0f));
        physics::rigid_body body = {};
        body.linearDamping = 0.0f;
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(box, body));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(box));
        fixture.run_step();

        // 1 kg -> 100 kg: a 100 N s impulse now gives 1 m/s, not 100 m/s.
        physics::rigid_body *authored = fixture.world.get<physics::rigid_body>(box);
        REQUIRE(authored != nullptr);
        authored->mass = 100.0f;
        fixture.run_step();
        CONJURE_REQUIRE_OK(fixture.physics.add_impulse(box, math::vec3(100.0f, 0.0f, 0.0f)));
        fixture.run_step();
        const physics::rigid_body *pulled = fixture.world.get<physics::rigid_body>(box);
        REQUIRE(pulled != nullptr);
        CHECK(pulled->linearVelocity.x == doctest::Approx(1.0f).epsilon(0.01));

        // Back to a derived mass (1 m^3 at Jolt's default density, 1000 kg): the same impulse adds 0.1 m/s.
        CONJURE_REQUIRE_OK(fixture.physics.set_linear_velocity(box, math::vec3(0.0f)));
        authored = fixture.world.get<physics::rigid_body>(box);
        REQUIRE(authored != nullptr);
        authored->mass = 0.0f;
        fixture.run_step();
        CONJURE_REQUIRE_OK(fixture.physics.add_impulse(box, math::vec3(100.0f, 0.0f, 0.0f)));
        fixture.run_step();
        pulled = fixture.world.get<physics::rigid_body>(box);
        REQUIRE(pulled != nullptr);
        CHECK(pulled->linearVelocity.x == doctest::Approx(0.1f).epsilon(0.01));
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("regression - review 9 lock flags and gyroscopic force toggled at runtime take effect") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.gravity = math::vec3(0.0f);
        physics_fixture fixture(jobs, settings);
        const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 5.0f, 0.0f));
        fixture.run_step();

        physics::rigid_body *authored = fixture.world.get<physics::rigid_body>(box);
        REQUIRE(authored != nullptr);
        authored->flags |= physics::rigid_body_flags::LOCK_ROTATION | physics::rigid_body_flags::USE_GYROSCOPIC_FORCE;
        fixture.run_step();
        CONJURE_REQUIRE_OK(fixture.physics.add_impulse_at(
          box,
          math::vec3(0.0f, 0.0f, 2.0f),
          fixture.position_of(box) + math::vec3(0.5f, 0.0f, 0.0f)
        ));
        fixture.run_steps(30);
        std::optional<math::transform> pose = fixture.physics.body_pose(box);
        REQUIRE(pose.has_value());
        CHECK(testing::approx_equal(pose->rotation, math::quat(1.0f, 0.0f, 0.0f, 0.0f), 1.0e-4f));
        CHECK(pose->position.z > 0.0f);

        // Unlocking at runtime works too: the next off-centre impulse spins the box.
        CONJURE_REQUIRE_OK(fixture.physics.set_linear_velocity(box, math::vec3(0.0f)));
        authored = fixture.world.get<physics::rigid_body>(box);
        REQUIRE(authored != nullptr);
        authored->flags = physics::rigid_body_flags::ALLOW_SLEEPING;
        fixture.run_step();
        CONJURE_REQUIRE_OK(fixture.physics.add_impulse_at(
          box,
          math::vec3(0.0f, 0.0f, 2.0f),
          fixture.position_of(box) + math::vec3(0.5f, 0.0f, 0.0f)
        ));
        fixture.run_steps(30);
        pose = fixture.physics.body_pose(box);
        REQUIRE(pose.has_value());
        CHECK_FALSE(testing::approx_equal(pose->rotation, math::quat(1.0f, 0.0f, 0.0f, 0.0f), 1.0e-3f));
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("regression - review 4 a teleported static body keeps following its transform") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const ecs::entity wall = fixture.spawn_box(math::vec3(0.0f, 0.0f, 0.0f), physics::motion_type::STATIC);
        const ecs::entity plate = fixture.create_at(math::vec3(0.0f, 10.0f, 0.0f));
        physics::rigid_body plateBody = {};
        plateBody.motionType = physics::motion_type::KINEMATIC;
        plateBody.flags = physics::rigid_body_flags::KINEMATIC_MOVES_WITH_TRANSFORM;
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(plate, plateBody));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(plate));
        fixture.run_step();

        math::transform pose = {};
        pose.position = math::vec3(5.0f, 0.0f, 0.0f);
        CONJURE_REQUIRE_OK(fixture.physics.teleport(wall, pose));
        pose.position = math::vec3(5.0f, 10.0f, 0.0f);
        CONJURE_REQUIRE_OK(fixture.physics.teleport(plate, pose));
        fixture.run_step();
        // The command moved both bodies and their transforms (the pull never visits these rows).
        CHECK(fixture.position_of(wall).x == doctest::Approx(5.0f));
        CHECK(fixture.transform_position(wall).x == doctest::Approx(5.0f));
        CHECK(fixture.position_of(plate).x == doctest::Approx(5.0f).epsilon(0.01));
        CHECK(fixture.transform_position(plate).x == doctest::Approx(5.0f).epsilon(0.01));

        // A later edit of the transform is a teleport again, not ignored.
        scene::local_transform *wallLocal = fixture.world.get<scene::local_transform>(wall);
        REQUIRE(wallLocal != nullptr);
        wallLocal->position.x = 9.0f;
        scene::local_transform *plateLocal = fixture.world.get<scene::local_transform>(plate);
        REQUIRE(plateLocal != nullptr);
        plateLocal->position.x = 9.0f;
        fixture.run_step();
        CHECK(fixture.position_of(wall).x == doctest::Approx(9.0f));
        CHECK(fixture.position_of(plate).x == doctest::Approx(9.0f).epsilon(0.01));
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("rigid_body - the body limit keeps extra bodies pending with an ERROR until one goes") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.maxBodies = 4;
        physics_fixture fixture(jobs, settings);
        std::array<ecs::entity, 5> boxes = {};
        for (u32 index = 0; index < boxes.size(); ++index) {
            boxes[index] = fixture.spawn_box(math::vec3(static_cast<f32>(index) * 2.0f, 5.0f, 0.0f));
        }
        testing::log_capture logs = {};
        logs.allow_errors();
        fixture.run_steps(3);
        CHECK(fixture.physics.body_count() == 4);
        CHECK(fixture.physics.stats().bodiesWithoutShape == 1);
        CHECK_FALSE(fixture.physics.has_body(boxes[4]));
        CHECK(logs.contains(core::log_level::ERR, "BODY_LIMIT_REACHED"));
        // Once per push, not per pending entity and not for ever.
        CHECK(logs.count(core::log_level::ERR) == 3);
        CONJURE_CHECK_OK(fixture.physics.validate());

        CONJURE_REQUIRE_OK(fixture.world.destroy_entity(boxes[0]));
        fixture.run_step();
        CHECK(fixture.physics.has_body(boxes[4]));
        CHECK(fixture.physics.body_count() == 4);
        CHECK(fixture.physics.stats().bodiesWithoutShape == 0);
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("rigid_body - move_kinematic reaches the pose over one step and is refused for dynamic bodies") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.gravity = math::vec3(0.0f);
        physics_fixture fixture(jobs, settings);
        const ecs::entity plate = fixture.spawn_box(math::vec3(0.0f, 0.0f, 0.0f), physics::motion_type::KINEMATIC);
        const ecs::entity box = fixture.spawn_box(math::vec3(10.0f, 0.0f, 0.0f));
        fixture.run_step();
        math::transform target = {};
        target.position = math::vec3(0.0f, 2.0f, 0.0f);
        CONJURE_REQUIRE_OK(fixture.physics.move_kinematic(plate, target));
        // The dynamic box is refused at the call, so nothing is queued for it and nothing is dropped later.
        CONJURE_CHECK_ERROR(fixture.physics.move_kinematic(box, target), physics::physics_error::WRONG_BODY_KIND);
        fixture.run_step();
        CHECK(fixture.position_of(plate).y == doctest::Approx(2.0f).epsilon(0.01));
        CHECK(fixture.transform_position(plate).y == doctest::Approx(2.0f).epsilon(0.01));
        CHECK(fixture.position_of(box) == math::vec3(10.0f, 0.0f, 0.0f));
        CHECK(fixture.physics.stats().droppedCommands == 0);
    }

    TEST_CASE("regression - code sweep F413 move_kinematic on a body that follows its transform is refused") {
        // It used to be applied: the body jumped to the pose for one step and the next push, which re-poses a
        // KINEMATIC_MOVES_WITH_TRANSFORM body from its local_transform, pulled it back.
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const ecs::entity plate = fixture.spawn_following_plate(math::vec3(0.0f, 5.0f, 0.0f));
        math::transform target = {};
        target.position = math::vec3(0.0f, 9.0f, 0.0f);

        SUBCASE("the body exists: WRONG_BODY_KIND at the call") {
            fixture.run_step();
            const core::result<void> refused = fixture.physics.move_kinematic(plate, target);
            CONJURE_CHECK_ERROR(refused, physics::physics_error::WRONG_BODY_KIND);
            REQUIRE_FALSE(refused.has_value());
            CHECK(refused.get_error().message().find("move_kinematic") != std::string::npos);
            CHECK(refused.get_error().message().find("follows its transform") != std::string::npos);
            fixture.run_step();
            CHECK(fixture.physics.stats().droppedCommands == 0);
        }
        SUBCASE("the entity is still pending: accepted, then dropped and counted by the push") {
            CONJURE_CHECK_OK(fixture.physics.move_kinematic(plate, target));
            fixture.run_step();
            CHECK(fixture.physics.stats().droppedCommands == 1);
        }
        // Either way the body never left the pose its transform gives it.
        CHECK(fixture.position_of(plate) == math::vec3(0.0f, 5.0f, 0.0f));
        CHECK(fixture.transform_position(plate) == math::vec3(0.0f, 5.0f, 0.0f));
    }
}
