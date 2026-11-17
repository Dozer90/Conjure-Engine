#include "conjure/testing/approx.h"
#include "conjure/testing/doctest.h"
#include "conjure/testing/log_capture.h"
#include "physics_test_fixture.h"

#include "conjure/core/math/ray.h"
#include "conjure/core/math/transform.h"
#include "conjure/physics/character_controller.h"
#include "conjure/physics/components.h"
#include "conjure/physics/physics_error.h"
#include "conjure/scene/components.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <type_traits>

using namespace conjure;
using namespace physics_test;

namespace {

    /// <summary>
    /// A static box rotated about z by 'degrees' so its top face is a slope rising along +x; the slope
    /// surface passes through the origin.
    /// </summary>
    [[nodiscard]] ecs::entity spawn_slope(physics_fixture &fixture, f32 degrees) {
        const f32 half = 20.0f;
        const math::quat rotation = glm::angleAxis(math::radians(degrees), math::vec3(0.0f, 0.0f, 1.0f));
        // Centre the box one half-thickness below the surface along the slope normal.
        const math::vec3 normal = rotation * math::vec3(0.0f, 1.0f, 0.0f);
        const ecs::entity slope = fixture.create_at(-normal * 0.5f);
        scene::local_transform *local = fixture.world.get<scene::local_transform>(slope);
        REQUIRE(local != nullptr);
        local->rotation = rotation;
        physics::rigid_body body = {};
        body.motionType = physics::motion_type::STATIC;
        REQUIRE(fixture.world.add<physics::rigid_body>(slope, body).has_value());
        physics::box_collider collider = {};
        collider.halfExtents = math::vec3(half, 0.5f, half);
        REQUIRE(fixture.world.add<physics::box_collider>(slope, collider).has_value());
        return slope;
    }

    void drive(physics_fixture &fixture, ecs::entity character, const math::vec3 &velocity, u32 steps) {
        for (u32 step = 0; step < steps; ++step) {
            physics::character_controller *controller = fixture.world.get<physics::character_controller>(character);
            REQUIRE(controller != nullptr);
            controller->velocity.x = velocity.x;
            controller->velocity.z = velocity.z;
            if (velocity.y != 0.0f) {
                controller->velocity.y = velocity.y;
            }
            fixture.run_step();
        }
    }

    /// <summary>
    /// How a test moves its platform: by writing its local_transform (a kinematic body with
    /// KINEMATIC_MOVES_WITH_TRANSFORM, whose velocity the push derives from the move) or by setting the
    /// velocity of a plain kinematic body.
    /// </summary>
    enum class platform_drive : u8 {
        TRANSFORM,
        VELOCITY
    };

    struct moving_platform {
        ecs::entity entity = {};
        platform_drive drive = platform_drive::TRANSFORM;
    };

    /// <summary>
    /// The speed the platform tests move their platform at, along x.
    /// </summary>
    const math::vec3 PLATFORM_VELOCITY = math::vec3(2.0f, 0.0f, 0.0f);

    /// <summary>
    /// A kinematic slab of 20 x 0.5 x 8 m centred on the y axis, its top face at y = 0.
    /// </summary>
    [[nodiscard]] moving_platform spawn_platform(physics_fixture &fixture, platform_drive drive) {
        const math::vec3 centre = math::vec3(0.0f, -0.25f, 0.0f);
        const math::vec3 halfExtents = math::vec3(10.0f, 0.25f, 4.0f);
        moving_platform created = {};
        created.drive = drive;
        created.entity = drive == platform_drive::TRANSFORM
          ? fixture.spawn_following_plate(centre, halfExtents)
          : fixture.spawn_box(centre, physics::motion_type::KINEMATIC, halfExtents);
        return created;
    }

    /// <summary>
    /// One fixed step during which the platform moves at 'velocity'.
    /// </summary>
    void step_platform(physics_fixture &fixture, const moving_platform &moving, const math::vec3 &velocity) {
        if (moving.drive == platform_drive::TRANSFORM) {
            scene::local_transform *local = fixture.world.get<scene::local_transform>(moving.entity);
            REQUIRE(local != nullptr);
            local->position += velocity * STEP;
        } else {
            CONJURE_REQUIRE_OK(fixture.physics.set_linear_velocity(moving.entity, velocity));
        }
        fixture.run_step();
    }

    /// <summary>
    /// 'steps' fixed steps during which the platform moves at 'velocity'.
    /// </summary>
    void ride(
      physics_fixture &fixture,
      const moving_platform &moving,
      u32 steps,
      const math::vec3 &velocity = PLATFORM_VELOCITY
    ) {
        for (u32 step = 0; step < steps; ++step) {
            CAPTURE(step);
            step_platform(fixture, moving, velocity);
        }
    }

    /// <summary>
    /// Runs the body once per way of moving a platform, as SUBCASEs.
    /// </summary>
    template <typename Callable>
    void for_each_platform_drive(Callable &&body) {
        SUBCASE("a platform that follows its local_transform") {
            body(platform_drive::TRANSFORM);
        }
        SUBCASE("a kinematic platform with a velocity") {
            body(platform_drive::VELOCITY);
        }
    }

    /// <summary>
    /// Where the character is relative to the platform, both straight from Jolt.
    /// </summary>
    [[nodiscard]] math::vec3 offset_on(
      const physics_fixture &fixture,
      ecs::entity character,
      const moving_platform &moving
    ) {
        return fixture.position_of(character) - fixture.position_of(moving.entity);
    }

    [[nodiscard]] const physics::character_controller &controller_of(
      const physics_fixture &fixture,
      ecs::entity character
    ) {
        const physics::character_controller *controller = fixture.world.get<physics::character_controller>(character);
        REQUIRE(controller != nullptr);
        return *controller;
    }
}

TEST_SUITE("physics/character_controller") {

    TEST_CASE("character_controller - layout and defaults") {
        static_assert(sizeof(physics::character_controller) == 68);
        static_assert(std::is_trivially_copyable_v<physics::character_controller>);
        static_assert(offsetof(physics::character_controller, velocity) == 32);
        static_assert(offsetof(physics::character_controller, groundEntity) == 60);
        const physics::character_controller controller = {};
        CHECK(controller.radius == 0.3f);
        CHECK(controller.height == 1.8f);
        CHECK(controller.ground == physics::ground_state::IN_AIR);
        CHECK(has_flag(controller.flags, physics::character_flags::APPLY_GRAVITY));
        CHECK(has_flag(controller.flags, physics::character_flags::WALK_STAIRS));
        CHECK(has_flag(controller.flags, physics::character_flags::STICK_TO_FLOOR));
        CHECK(controller.groundEntity.is_null());
        CHECK(to_underlying(physics::character_flags::APPLY_GRAVITY) == 4);
    }

    TEST_CASE("character_controller - walks up a 30 degree slope and reports ON_GROUND on it") {
        for_each_job_mode([](core::jobs::job_system &jobs) {
            physics_fixture fixture(jobs);
            const ecs::entity slope = spawn_slope(fixture, 30.0f);
            const ecs::entity character = fixture.spawn_character(math::vec3(0.0f, 0.05f, 0.0f));
            drive(fixture, character, math::vec3(2.0f, 0.0f, 0.0f), 120);
            REQUIRE(fixture.physics.has_body(character));
            const physics::character_controller *controller =
              fixture.world.get<physics::character_controller>(character);
            REQUIRE(controller != nullptr);
            CHECK(controller->ground == physics::ground_state::ON_GROUND);
            CHECK(controller->groundEntity == slope);
            CHECK(controller->groundNormal.y > 0.8f);
            CHECK(fixture.physics.character_ground(character) == physics::ground_state::ON_GROUND);
            const math::vec3 position = fixture.position_of(character);
            CHECK(position.x > 1.5f);
            // Up the slope: y follows x * tan(30).
            CHECK(position.y == doctest::Approx(position.x * std::tan(math::radians(30.0f))).epsilon(0.15));
            CHECK(fixture.transform_position(character) == position);
        });
    }

    TEST_CASE("character_controller - a 60 degree slope with maxSlopeAngle 45 is ON_STEEP_GROUND and not climbed") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        (void)spawn_slope(fixture, 60.0f);
        const ecs::entity character = fixture.spawn_character(math::vec3(0.5f, 1.0f, 0.0f));
        drive(fixture, character, math::vec3(2.0f, 0.0f, 0.0f), 120);
        const physics::character_controller *controller = fixture.world.get<physics::character_controller>(character);
        REQUIRE(controller != nullptr);
        CHECK(controller->ground == physics::ground_state::ON_STEEP_GROUND);
        // The character slid down or stayed; it never climbed above its start.
        CHECK(fixture.position_of(character).y < 1.0f + 0.05f);
        CHECK(fixture.position_of(character).x < 1.0f);
    }

    TEST_CASE("character_controller - walking into a wall stops within padding plus a centimetre") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        (void)fixture.spawn_ground();
        (void)fixture.spawn_box(
          math::vec3(3.0f, 1.0f, 0.0f),
          physics::motion_type::STATIC,
          math::vec3(0.5f, 1.0f, 5.0f)
        );
        physics::character_controller settings = {};
        const ecs::entity character = fixture.spawn_character(math::vec3(0.0f, 0.0f, 0.0f), settings);
        drive(fixture, character, math::vec3(3.0f, 0.0f, 0.0f), 180);
        const f32 wallFace = 2.5f;
        const f32 frontX = fixture.position_of(character).x + settings.radius;
        CHECK(frontX <= wallFace + 0.001f);
        CHECK(frontX >= wallFace - settings.padding - 0.01f);
        const physics::character_controller *controller = fixture.world.get<physics::character_controller>(character);
        REQUIRE(controller != nullptr);
        CHECK(controller->ground == physics::ground_state::ON_GROUND);
        // Blocked for good: the last ten steps did not move it.
        const f32 blockedX = fixture.position_of(character).x;
        drive(fixture, character, math::vec3(3.0f, 0.0f, 0.0f), 10);
        CHECK(std::abs(fixture.position_of(character).x - blockedX) < 0.001f);
    }

    TEST_CASE("character_controller - steps onto a 0.3 m stair and is blocked by a 0.5 m one") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        (void)fixture.spawn_ground();
        // Stairs from x = 3 to x = 9 (0.3 m) and from x = 13 to x = 19 (0.5 m): long enough that three
        // seconds at 2 m/s end on top of the low one.
        (void)fixture.spawn_box(
          math::vec3(6.0f, 0.15f, 0.0f),
          physics::motion_type::STATIC,
          math::vec3(3.0f, 0.15f, 5.0f)
        );
        (void)fixture.spawn_box(
          math::vec3(16.0f, 0.25f, 0.0f),
          physics::motion_type::STATIC,
          math::vec3(3.0f, 0.25f, 5.0f)
        );
        physics::character_controller settings = {};
        settings.stepHeight = 0.4f;
        const ecs::entity low = fixture.spawn_character(math::vec3(0.0f, 0.0f, 0.0f), settings);
        const ecs::entity high = fixture.spawn_character(math::vec3(10.0f, 0.0f, 0.0f), settings);
        for (u32 step = 0; step < 180; ++step) {
            physics::character_controller *lowController = fixture.world.get<physics::character_controller>(low);
            physics::character_controller *highController = fixture.world.get<physics::character_controller>(high);
            REQUIRE(lowController != nullptr);
            REQUIRE(highController != nullptr);
            lowController->velocity.x = 2.0f;
            highController->velocity.x = 2.0f;
            fixture.run_step();
        }
        CHECK(fixture.position_of(low).y == doctest::Approx(0.3f).epsilon(0.05));
        CHECK(fixture.position_of(low).x > 3.5f);
        CHECK(fixture.position_of(high).y < 0.05f);
        CHECK(fixture.position_of(high).x < 13.0f);
    }

    TEST_CASE("character_controller - APPLY_GRAVITY falls off a ledge and lands") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        (void)fixture.spawn_ground();
        (void)fixture.spawn_box(
          math::vec3(0.0f, 1.0f, 0.0f),
          physics::motion_type::STATIC,
          math::vec3(1.0f, 1.0f, 5.0f)
        );
        const ecs::entity character = fixture.spawn_character(math::vec3(0.0f, 2.0f, 0.0f));
        bool wasInAir = false;
        for (u32 step = 0; step < 240; ++step) {
            physics::character_controller *controller = fixture.world.get<physics::character_controller>(character);
            REQUIRE(controller != nullptr);
            controller->velocity.x = 3.0f;
            fixture.run_step();
            controller = fixture.world.get<physics::character_controller>(character);
            REQUIRE(controller != nullptr);
            if (controller->ground == physics::ground_state::IN_AIR) {
                wasInAir = true;
            }
        }
        CHECK(wasInAir);
        const physics::character_controller *controller = fixture.world.get<physics::character_controller>(character);
        REQUIRE(controller != nullptr);
        CHECK(controller->ground == physics::ground_state::ON_GROUND);
        CHECK(fixture.position_of(character).y == doctest::Approx(0.0f).epsilon(0.05));
        CHECK(fixture.position_of(character).x > 2.0f);
    }

    TEST_CASE("character_controller - a raycast hits the character's inner body") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        (void)fixture.spawn_ground();
        const ecs::entity character = fixture.spawn_character(math::vec3(0.0f, 0.0f, 0.0f));
        fixture.run_steps(5);
        math::ray ray = {};
        ray.origin = math::vec3(0.0f, 1.0f, 5.0f);
        ray.direction = math::vec3(0.0f, 0.0f, -1.0f);
        const std::optional<physics::raycast_hit> hit = fixture.physics.raycast(ray, 20.0f);
        REQUIRE(hit.has_value());
        CHECK(hit->entity == character);
        CHECK(std::abs(hit->distance - (5.0f - 0.3f)) < 0.03f);
        const std::optional<math::transform> pose = fixture.physics.body_pose(character);
        REQUIRE(pose.has_value());
        CHECK(pose->position.y == doctest::Approx(0.0f).epsilon(0.05));
        CHECK(fixture.world.has<scene::interpolated_transform>(character));
    }

    TEST_CASE("character_controller - character_move on a non-character is NOT_A_CHARACTER and drives a character") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        (void)fixture.spawn_ground();
        const ecs::entity box = fixture.spawn_box(math::vec3(5.0f, 0.5f, 0.0f));
        const ecs::entity character = fixture.spawn_character(math::vec3(0.0f, 0.0f, 0.0f));
        fixture.run_step();
        CONJURE_CHECK_ERROR(
          fixture.physics.character_move(box, math::vec3(1.0f, 0.0f, 0.0f)),
          physics::physics_error::NOT_A_CHARACTER
        );
        CHECK_FALSE(fixture.physics.character_ground(box).has_value());
        for (u32 step = 0; step < 60; ++step) {
            CAPTURE(step);
            CONJURE_REQUIRE_OK(fixture.physics.character_move(character, math::vec3(0.0f, 0.0f, -1.0f)));
            fixture.run_step();
        }
        CHECK(fixture.position_of(character).z == doctest::Approx(-1.0f).epsilon(0.1));
    }

    TEST_CASE("character_controller - rigid_body and character_controller together warn and the character wins") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        (void)fixture.spawn_ground();
        const ecs::entity both = fixture.create_at(math::vec3(0.0f, 0.0f, 0.0f));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(both));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(both));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::character_controller>(both));
        testing::log_capture logs = {};
        fixture.run_steps(5);
        CHECK(logs.contains(core::log_level::WARN, "CONFLICTING_BODY_COMPONENTS"));
        CHECK(fixture.physics.character_ground(both).has_value());
        CHECK(fixture.physics.stats().characters == 1);
        CHECK(fixture.physics.body_count() == 2);
        // Removing the character component destroys the character; the colliders then rebuild a body.
        CONJURE_REQUIRE_OK(fixture.world.remove<physics::character_controller>(both));
        CHECK_FALSE(fixture.physics.has_body(both));
        fixture.run_steps(2);
        CHECK(fixture.physics.has_body(both));
        CHECK_FALSE(fixture.physics.character_ground(both).has_value());
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("character_controller - changing the radius rebuilds the character in place") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        (void)fixture.spawn_ground();
        const ecs::entity character = fixture.spawn_character(math::vec3(0.0f, 0.0f, 0.0f));
        fixture.run_steps(5);
        math::ray ray = {};
        ray.origin = math::vec3(5.0f, 0.9f, 0.0f);
        ray.direction = math::vec3(-1.0f, 0.0f, 0.0f);
        std::optional<physics::raycast_hit> hit = fixture.physics.raycast(ray, 20.0f);
        REQUIRE(hit.has_value());
        CHECK(std::abs(hit->distance - 4.7f) < 0.03f);
        physics::character_controller *controller = fixture.world.get<physics::character_controller>(character);
        REQUIRE(controller != nullptr);
        controller->radius = 0.6f;
        fixture.run_steps(5);
        hit = fixture.physics.raycast(ray, 20.0f);
        REQUIRE(hit.has_value());
        CHECK(hit->entity == character);
        CHECK(std::abs(hit->distance - 4.4f) < 0.03f);
        CHECK(fixture.physics.stats().characters == 1);
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("regression - review 5 a character_controller added to an existing body wins over it") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        (void)fixture.spawn_ground();
        const ecs::entity late = fixture.create_at(math::vec3(0.0f, 0.0f, 0.0f));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(late));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(late));
        fixture.run_steps(2);
        REQUIRE(fixture.physics.has_body(late));
        CHECK(fixture.physics.stats().characters == 0);

        testing::log_capture logs = {};
        CONJURE_REQUIRE_OK(fixture.world.add<physics::character_controller>(late));
        // The body went at once; the character arrives with the next push.
        CHECK_FALSE(fixture.physics.has_body(late));
        fixture.run_steps(3);
        CHECK(logs.contains(core::log_level::WARN, "CONFLICTING_BODY_COMPONENTS"));
        CHECK(fixture.physics.has_body(late));
        CHECK(fixture.physics.stats().characters == 1);
        CHECK(fixture.physics.character_ground(late).has_value());
        CHECK(fixture.world.has<scene::interpolated_transform>(late));
        CHECK(fixture.physics.body_count() == 2);
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("character_controller - teleport moves a character and move_kinematic is refused") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        (void)fixture.spawn_ground();
        const ecs::entity character = fixture.spawn_character(math::vec3(0.0f, 0.0f, 0.0f));
        fixture.run_steps(5);
        math::transform pose = {};
        pose.position = math::vec3(5.0f, 0.0f, -3.0f);
        pose.rotation = glm::angleAxis(0.5f, math::vec3(0.0f, 1.0f, 0.0f));
        CONJURE_REQUIRE_OK(fixture.physics.teleport(character, pose));
        fixture.physics.push_transforms(STEP);
        std::optional<math::transform> joltPose = fixture.physics.body_pose(character);
        REQUIRE(joltPose.has_value());
        CHECK(testing::approx_equal(joltPose->position, pose.position, 1.0e-4f));
        CHECK(testing::approx_equal(joltPose->rotation, pose.rotation, 1.0e-4f));
        fixture.physics.step(STEP);
        fixture.physics.pull_transforms();
        fixture.physics.collect_contact_events();
        const scene::interpolated_transform *interpolated = fixture.world.get<scene::interpolated_transform>(character);
        REQUIRE(interpolated != nullptr);
        CHECK(testing::approx_equal(interpolated->previous.position, pose.position, 0.05f));
        CHECK(fixture.transform_position(character).x == doctest::Approx(5.0f).epsilon(0.01));
        CHECK(fixture.transform_position(character).z == doctest::Approx(-3.0f).epsilon(0.01));
        fixture.run_steps(5);
        CHECK(fixture.physics.character_ground(character) == physics::ground_state::ON_GROUND);

        // A character is not a kinematic body: move_kinematic is refused at the call, nothing is queued.
        CONJURE_CHECK_ERROR(fixture.physics.move_kinematic(character, pose), physics::physics_error::WRONG_BODY_KIND);
        fixture.run_step();
        CHECK(fixture.physics.stats().droppedCommands == 0);
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("regression - code sweep F413 wake and sleep on a character succeed and change nothing") {
        // Both used to be queued, then dropped and counted by the push. A character never sleeps, so they are
        // no-ops like on a static body.
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        (void)fixture.spawn_ground();
        const ecs::entity character = fixture.spawn_character(math::vec3(0.0f, 0.0f, 0.0f));
        // Still pending: accepted, and a no-op once the push has made the character.
        CONJURE_CHECK_OK(fixture.physics.wake(character));
        CONJURE_CHECK_OK(fixture.physics.sleep(character));
        fixture.run_steps(5);
        REQUIRE(fixture.physics.has_body(character));
        CHECK(fixture.physics.stats().droppedCommands == 0);

        CONJURE_CHECK_OK(fixture.physics.sleep(character));
        CONJURE_CHECK_OK(fixture.physics.wake(character));
        for (u32 step = 0; step < 30; ++step) {
            CAPTURE(step);
            CONJURE_REQUIRE_OK(fixture.physics.sleep(character));
            CONJURE_REQUIRE_OK(fixture.physics.character_move(character, math::vec3(1.0f, 0.0f, 0.0f)));
            fixture.run_step();
        }
        // sleep did not stop the character: it walked half a metre in half a second.
        CHECK(fixture.position_of(character).x == doctest::Approx(0.5f).epsilon(0.1));
        CHECK(fixture.physics.character_ground(character) == physics::ground_state::ON_GROUND);
        CHECK(fixture.physics.stats().droppedCommands == 0);
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("character_controller - a body command on a character is WRONG_BODY_KIND and leaves it alone") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        (void)fixture.spawn_ground();
        const ecs::entity character = fixture.spawn_character(math::vec3(0.0f, 0.0f, 0.0f));
        fixture.run_steps(5);
        const math::vec3 before = fixture.position_of(character);
        const math::vec3 push = math::vec3(50.0f, 0.0f, 0.0f);
        CONJURE_CHECK_ERROR(fixture.physics.add_force(character, push), physics::physics_error::WRONG_BODY_KIND);
        CONJURE_CHECK_ERROR(fixture.physics.add_impulse(character, push), physics::physics_error::WRONG_BODY_KIND);
        CONJURE_CHECK_ERROR(
          fixture.physics.set_linear_velocity(character, push),
          physics::physics_error::WRONG_BODY_KIND
        );
        fixture.run_steps(5);
        CHECK(testing::approx_equal(fixture.position_of(character), before, 1.0e-2f));
        CHECK(fixture.physics.stats().droppedCommands == 0);

        // The same commands sent while the character is still pending are accepted, then dropped and counted.
        const ecs::entity late = fixture.spawn_character(math::vec3(5.0f, 0.0f, 0.0f));
        CONJURE_CHECK_OK(fixture.physics.add_force(late, push));
        CONJURE_CHECK_OK(fixture.physics.set_linear_velocity(late, push));
        fixture.run_steps(5);
        CHECK(fixture.physics.stats().droppedCommands == 2);
        CHECK(fixture.position_of(late).x == doctest::Approx(5.0f).epsilon(0.01));

        // With a rigid_body next to the controller the character still wins, at the call as in the world.
        testing::log_capture logs = {};
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(character));
        CHECK(logs.contains(core::log_level::WARN, "CONFLICTING_BODY_COMPONENTS"));
        CONJURE_CHECK_ERROR(fixture.physics.add_force(character, push), physics::physics_error::WRONG_BODY_KIND);
        CONJURE_CHECK_OK(fixture.physics.character_move(character, math::vec3(0.0f)));
    }

    TEST_CASE("character_controller - under a rotated parent the local_transform composes to the body pose") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        (void)fixture.spawn_ground();
        const ecs::entity parent = fixture.create_at(math::vec3(10.0f, 0.0f, 0.0f));
        scene::local_transform *parentLocal = fixture.world.get<scene::local_transform>(parent);
        REQUIRE(parentLocal != nullptr);
        parentLocal->rotation = glm::angleAxis(1.0f, math::vec3(0.0f, 1.0f, 0.0f));
        const ecs::entity character = fixture.spawn_character(math::vec3(0.0f, 0.0f, 0.0f));
        scene::parent link = {};
        link.value = parent;
        CONJURE_REQUIRE_OK(fixture.world.add<scene::parent>(character, link));
        drive(fixture, character, math::vec3(1.0f, 0.0f, 0.0f), 60);

        const std::optional<math::transform> pose = fixture.physics.body_pose(character);
        REQUIRE(pose.has_value());
        const scene::local_transform *childLocal = fixture.world.get<scene::local_transform>(character);
        REQUIRE(childLocal != nullptr);
        const math::transform composed = parentLocal->to_transform() * childLocal->to_transform();
        CHECK(testing::approx_equal(composed.position, pose->position, 1.0e-3f));
        CHECK(testing::approx_equal(composed.rotation, pose->rotation, 1.0e-3f));
        // Walked 1 m along world x from the parent's origin, on the ground.
        CHECK(pose->position.x == doctest::Approx(11.0f).epsilon(0.05));
        CHECK(pose->position.y == doctest::Approx(0.0f).epsilon(0.05));
        CHECK(fixture.physics.character_ground(character) == physics::ground_state::ON_GROUND);
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("character_controller - the character limit keeps extra characters pending with an ERROR") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.maxCharacters = 1;
        physics_fixture fixture(jobs, settings);
        (void)fixture.spawn_character(math::vec3(0.0f, 0.0f, 0.0f));
        const ecs::entity second = fixture.spawn_character(math::vec3(3.0f, 0.0f, 0.0f));
        testing::log_capture logs = {};
        logs.allow_errors();
        fixture.run_steps(3);
        CHECK(fixture.physics.stats().characters == 1);
        CHECK_FALSE(fixture.physics.has_body(second));
        CHECK(logs.contains(core::log_level::ERR, "BODY_LIMIT_REACHED"));
    }

    TEST_CASE("regression - code sweep F421 a runtime scale change reaches the character's interpolated pose") {
        // The render pose kept the scale the character had when it was created.
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        (void)fixture.spawn_ground();
        const ecs::entity parent = fixture.create_at(math::vec3(0.0f));
        const ecs::entity character = fixture.spawn_character(math::vec3(0.0f));
        ecs::entity scaled = character;
        SUBCASE("its own scale") {}
        SUBCASE("the scale of its parent") {
            scene::parent link = {};
            link.value = parent;
            CONJURE_REQUIRE_OK(fixture.world.add<scene::parent>(character, link));
            scaled = parent;
        }
        fixture.run_steps(5);
        const scene::interpolated_transform *interpolated = fixture.world.get<scene::interpolated_transform>(character);
        REQUIRE(interpolated != nullptr);
        REQUIRE(interpolated->current.scale == math::vec3(1.0f));
        const math::vec3 before = fixture.position_of(character);

        const math::vec3 scale = math::vec3(2.0f, 3.0f, 2.0f);
        scene::local_transform *local = fixture.world.get<scene::local_transform>(scaled);
        REQUIRE(local != nullptr);
        local->scale = scale;
        fixture.run_steps(2);
        interpolated = fixture.world.get<scene::interpolated_transform>(character);
        REQUIRE(interpolated != nullptr);
        CHECK(interpolated->current.scale == scale);
        CHECK(interpolated->previous.scale == scale);

        // And back again, which a scale remembered from creation would also have hidden.
        local = fixture.world.get<scene::local_transform>(scaled);
        REQUIRE(local != nullptr);
        local->scale = math::vec3(1.0f);
        fixture.run_steps(2);
        interpolated = fixture.world.get<scene::interpolated_transform>(character);
        REQUIRE(interpolated != nullptr);
        CHECK(interpolated->current.scale == math::vec3(1.0f));

        // The capsule is not scaled: the same character stands where it stood (a parent's scale moves it by
        // the few millimetres its feet are away from the parent's origin, and it settles again).
        fixture.run_steps(10);
        CHECK(testing::approx_equal(fixture.position_of(character), before, 0.05f));
        CHECK(fixture.physics.character_ground(character) == physics::ground_state::ON_GROUND);
        CHECK(fixture.physics.stats().characters == 1);
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("regression - code sweep F421 a character standing on a moving platform is carried with it") {
        // The step followed the ground's vertical velocity only, so a platform slid away under a character.
        for_each_platform_drive([](platform_drive drive) {
            core::jobs::job_system jobs = {};
            physics_fixture fixture(jobs);
            const moving_platform moving = spawn_platform(fixture, drive);
            const ecs::entity character = fixture.spawn_character(math::vec3(0.0f));
            fixture.run_steps(10);
            REQUIRE(fixture.physics.character_ground(character) == physics::ground_state::ON_GROUND);
            const math::vec3 before = offset_on(fixture, character, moving);

            ride(fixture, moving, 60);
            CHECK(fixture.position_of(moving.entity).x == doctest::Approx(2.0f).epsilon(0.01));
            CHECK(testing::approx_equal(offset_on(fixture, character, moving), before, 0.01f));
            const physics::character_controller &controller = controller_of(fixture, character);
            CHECK(controller.ground == physics::ground_state::ON_GROUND);
            CHECK(controller.groundEntity == moving.entity);
            // It moves at 2 m/s and its velocity still reads zero: the component holds the character's own.
            CHECK(testing::approx_equal(controller.velocity, math::vec3(0.0f), 1.0e-4f));
            CHECK(fixture.transform_position(character) == fixture.position_of(character));

            // The platform stops, and so does the character: nothing of the carrying is left in it.
            const math::vec3 stopped = fixture.position_of(character);
            for (u32 step = 0; step < 10; ++step) {
                CAPTURE(step);
                step_platform(fixture, moving, math::vec3(0.0f));
            }
            CHECK(testing::approx_equal(fixture.position_of(character), stopped, 1.0e-3f));
            CONJURE_CHECK_OK(fixture.physics.validate());
        });
    }

    TEST_CASE("character_controller - a velocity on a moving platform is relative to it and does not compound") {
        for_each_platform_drive([](platform_drive drive) {
            core::jobs::job_system jobs = {};
            physics_fixture fixture(jobs);
            const moving_platform moving = spawn_platform(fixture, drive);
            const ecs::entity character = fixture.spawn_character(math::vec3(0.0f));
            fixture.run_steps(10);
            ride(fixture, moving, 10);
            const math::vec3 before = offset_on(fixture, character, moving);
            const math::vec3 walk = math::vec3(0.0f, 0.0f, 1.0f);

            bool everyStep = false;
            SUBCASE("written into the component once") {
                physics::character_controller *controller = fixture.world.get<physics::character_controller>(character);
                REQUIRE(controller != nullptr);
                controller->velocity = walk;
            }
            SUBCASE("sent with character_move every step") {
                everyStep = true;
            }
            for (u32 step = 0; step < 60; ++step) {
                CAPTURE(step);
                if (everyStep) {
                    CONJURE_REQUIRE_OK(fixture.physics.character_move(character, walk));
                }
                step_platform(fixture, moving, PLATFORM_VELOCITY);
                // Read back every step: a pull that left the ground's share in would feed it to the next step.
                CHECK(testing::approx_equal(controller_of(fixture, character).velocity, walk, 1.0e-4f));
            }
            // One metre across the platform in a second, and no drift along the way the platform moves.
            const math::vec3 walked = offset_on(fixture, character, moving) - before;
            CHECK(walked.z == doctest::Approx(1.0f).epsilon(0.02));
            CHECK(std::abs(walked.x) < 0.01f);
            CHECK(controller_of(fixture, character).ground == physics::ground_state::ON_GROUND);
        });
    }

    TEST_CASE("character_controller - a jump off a moving platform keeps the platform's speed until it lands") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const moving_platform moving = spawn_platform(fixture, platform_drive::TRANSFORM);
        const ecs::entity character = fixture.spawn_character(math::vec3(0.0f));
        fixture.run_steps(10);
        ride(fixture, moving, 10);
        const math::vec3 before = offset_on(fixture, character, moving);

        bool platformStops = false;
        SUBCASE("the platform keeps moving and the character lands where it jumped") {}
        SUBCASE("the platform stops under the jump and the character lands ahead of where it jumped") {
            platformStops = true;
        }
        physics::character_controller *jumping = fixture.world.get<physics::character_controller>(character);
        REQUIRE(jumping != nullptr);
        jumping->velocity.y = 5.0f;
        u32 airborne = 0;
        for (u32 step = 0; step < 90; ++step) {
            // The platform still moves during the step of the jump itself: that is the speed the character takes.
            const bool moves = !platformStops || step == 0;
            const f32 xBefore = fixture.position_of(character).x;
            step_platform(fixture, moving, moves ? PLATFORM_VELOCITY : math::vec3(0.0f));
            const physics::character_controller &controller = controller_of(fixture, character);
            if (controller.ground != physics::ground_state::IN_AIR) {
                continue;
            }
            ++airborne;
            CAPTURE(step);
            // In the air it goes on at the platform's 2 m/s, and its velocity still reads its own: none along x.
            const f32 moved = fixture.position_of(character).x - xBefore;
            CHECK(moved == doctest::Approx(PLATFORM_VELOCITY.x * STEP).epsilon(0.01));
            CHECK(std::abs(controller.velocity.x) < 1.0e-4f);
        }
        // 5 m/s upwards under 9.81 m/s2 is about a second in the air.
        CHECK(airborne > 50);
        CHECK(airborne < 70);
        const physics::character_controller &landed = controller_of(fixture, character);
        CHECK(landed.ground == physics::ground_state::ON_GROUND);
        CHECK(landed.groundEntity == moving.entity);
        CHECK(testing::approx_equal(landed.velocity, math::vec3(0.0f), 1.0e-4f));
        const f32 ahead = offset_on(fixture, character, moving).x - before.x;
        if (platformStops) {
            // It flew on for as long as it was in the air (give or take the step it left and the one it landed in).
            CHECK(ahead == doctest::Approx(PLATFORM_VELOCITY.x * static_cast<f32>(airborne) * STEP).epsilon(0.05));
        } else {
            CHECK(std::abs(ahead) < 0.02f);
        }
        // Landed, it takes the velocity of the ground it stands on again: it stays put on the platform.
        const math::vec3 settledOffset = offset_on(fixture, character, moving);
        for (u32 step = 0; step < 10; ++step) {
            CAPTURE(step);
            step_platform(fixture, moving, platformStops ? math::vec3(0.0f) : PLATFORM_VELOCITY);
        }
        CHECK(testing::approx_equal(offset_on(fixture, character, moving), settledOffset, 1.0e-3f));
    }

    TEST_CASE("character_controller - a teleport ends what the moving ground carried the character with") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const moving_platform moving = spawn_platform(fixture, platform_drive::TRANSFORM);
        const ecs::entity landing = fixture.spawn_box(
          math::vec3(100.0f, -0.5f, 0.0f),
          physics::motion_type::STATIC,
          math::vec3(5.0f, 0.5f, 5.0f)
        );
        const ecs::entity character = fixture.spawn_character(math::vec3(0.0f));
        fixture.run_steps(10);
        ride(fixture, moving, 20);
        REQUIRE(controller_of(fixture, character).groundEntity == moving.entity);

        // Three metres above the landing place: the character is in the air from the first step on.
        const math::vec3 target = math::vec3(100.0f, 3.0f, 0.0f);
        SUBCASE("through the teleport command") {
            math::transform pose = {};
            pose.position = target;
            CONJURE_REQUIRE_OK(fixture.physics.teleport(character, pose));
        }
        SUBCASE("through its local_transform") {
            scene::local_transform *local = fixture.world.get<scene::local_transform>(character);
            REQUIRE(local != nullptr);
            local->position = target;
        }
        // A pull between the push that teleports and the next step already reads the character's own velocity,
        // not the 2 m/s the last step gave Jolt.
        fixture.physics.push_transforms(STEP);
        fixture.physics.pull_transforms();
        CHECK(std::abs(controller_of(fixture, character).velocity.x) < 1.0e-4f);
        CHECK(testing::approx_equal(fixture.position_of(character), target, 1.0e-4f));
        for (u32 step = 0; step < 90; ++step) {
            step_platform(fixture, moving, PLATFORM_VELOCITY);
            CAPTURE(step);
            // Straight down: neither the step after the teleport nor the fall takes the platform's 2 m/s along.
            CHECK(std::abs(fixture.position_of(character).x - target.x) < 1.0e-3f);
            CHECK(std::abs(controller_of(fixture, character).velocity.x) < 1.0e-4f);
        }
        const physics::character_controller &controller = controller_of(fixture, character);
        CHECK(controller.ground == physics::ground_state::ON_GROUND);
        CHECK(controller.groundEntity == landing);
        CHECK(fixture.position_of(character).y == doctest::Approx(0.0f).epsilon(0.05));
    }

    TEST_CASE("character_controller - a character parented to the platform under its feet moves with it once") {
        // The parent's motion reaches the character as a teleport in every push; the ground's velocity on top of
        // that would move it twice as far as the platform. That holds for the horizontal part, which the step
        // after a teleport leaves out, and not for a parent that rises (the last SUBCASE).
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const moving_platform moving = spawn_platform(fixture, platform_drive::TRANSFORM);
        // The platform's origin is its centre, a quarter of a metre below its top face.
        const ecs::entity character = fixture.spawn_character(math::vec3(0.0f, 0.25f, 0.0f));
        scene::parent link = {};
        link.value = moving.entity;
        CONJURE_REQUIRE_OK(fixture.world.add<scene::parent>(character, link));
        fixture.run_steps(10);
        REQUIRE(fixture.physics.character_ground(character) == physics::ground_state::ON_GROUND);
        const math::vec3 before = offset_on(fixture, character, moving);

        SUBCASE("the platform moves sideways") {
            ride(fixture, moving, 60);
            CHECK(fixture.position_of(moving.entity).x == doctest::Approx(2.0f).epsilon(0.01));
            CHECK(testing::approx_equal(offset_on(fixture, character, moving), before, 0.01f));
            CHECK(controller_of(fixture, character).ground == physics::ground_state::ON_GROUND);
            CHECK(testing::approx_equal(controller_of(fixture, character).velocity, math::vec3(0.0f), 1.0e-4f));
        }
        SUBCASE("the platform sinks") {
            ride(fixture, moving, 60, math::vec3(0.0f, -1.0f, 0.0f));
            CHECK(fixture.position_of(moving.entity).y == doctest::Approx(-1.25f).epsilon(0.01));
            CHECK(testing::approx_equal(offset_on(fixture, character, moving), before, 0.01f));
            CHECK(controller_of(fixture, character).ground == physics::ground_state::ON_GROUND);
            CHECK(testing::approx_equal(controller_of(fixture, character).velocity, math::vec3(0.0f), 1.0e-4f));
        }
        SUBCASE("the platform rises, and its vertical velocity comes on top of the parent's motion") {
            // The documented limit (character_controller.h): the vertical velocity of the ground is not left out
            // in the step after a teleport, so the character is thrown a few centimetres up, falls back and is
            // thrown again. It neither sinks into the platform nor flies away.
            f32 lowest = 0.0f;
            f32 highest = 0.0f;
            u32 airborne = 0;
            for (u32 step = 0; step < 90; ++step) {
                CAPTURE(step);
                step_platform(fixture, moving, math::vec3(0.0f, 1.0f, 0.0f));
                const f32 above = offset_on(fixture, character, moving).y - before.y;
                lowest = std::min(lowest, above);
                highest = std::max(highest, above);
                airborne += controller_of(fixture, character).ground == physics::ground_state::IN_AIR ? 1u : 0u;
            }
            CHECK(fixture.position_of(moving.entity).y == doctest::Approx(1.25f).epsilon(0.01));
            // Measured: one step of the platform (1.7 cm) below at the start, hops of 6 cm, in the air in 78
            // of the 90 steps.
            CHECK(lowest > -0.03f);
            CHECK(highest > 0.02f);
            CHECK(highest < 0.2f);
            CHECK(airborne > 30);
        }
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("character_controller - a character on a turning platform goes round with it") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const moving_platform turning = spawn_platform(fixture, platform_drive::VELOCITY);
        const math::vec3 start = math::vec3(3.0f, 0.0f, 0.0f);
        const ecs::entity character = fixture.spawn_character(start);
        fixture.run_steps(10);
        REQUIRE(fixture.physics.character_ground(character) == physics::ground_state::ON_GROUND);

        // One radian in a second about the vertical axis through the platform's centre.
        CONJURE_REQUIRE_OK(fixture.physics.set_angular_velocity(turning.entity, math::vec3(0.0f, 1.0f, 0.0f)));
        fixture.run_steps(60);
        const std::optional<math::transform> platformPose = fixture.physics.body_pose(turning.entity);
        REQUIRE(platformPose.has_value());
        const math::vec3 expected = platformPose->rotation * start;
        CHECK(std::abs(expected.z) > 2.0f);
        const math::vec3 position = fixture.position_of(character);
        CAPTURE(position);
        CAPTURE(expected);
        CHECK(testing::approx_equal(math::vec3(position.x, 0.0f, position.z), expected, 0.05f));
        // The ground's velocity under the character changes direction every step; its own stays zero.
        const physics::character_controller &controller = controller_of(fixture, character);
        CHECK(testing::approx_equal(controller.velocity, math::vec3(0.0f), 1.0e-4f));
        CHECK(controller.ground == physics::ground_state::ON_GROUND);
    }

    TEST_CASE("character_controller - a character rebuilt while it rides a platform stays carried") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const moving_platform moving = spawn_platform(fixture, platform_drive::TRANSFORM);
        const ecs::entity character = fixture.spawn_character(math::vec3(0.0f));
        fixture.run_steps(10);
        ride(fixture, moving, 20);
        const math::vec3 before = offset_on(fixture, character, moving);

        // An authored field rebuilds the character in place; the new one knows its ground only after its first
        // update, and that step still carries it with what the platform gave it.
        physics::character_controller *controller = fixture.world.get<physics::character_controller>(character);
        REQUIRE(controller != nullptr);
        controller->mass = 90.0f;
        ride(fixture, moving, 40);
        CHECK(testing::approx_equal(offset_on(fixture, character, moving), before, 0.01f));
        CHECK(controller_of(fixture, character).ground == physics::ground_state::ON_GROUND);
        CHECK(testing::approx_equal(controller_of(fixture, character).velocity, math::vec3(0.0f), 1.0e-4f));
        CHECK(fixture.physics.stats().characters == 1);
    }

    TEST_CASE("character_controller - a character turned on a moving platform stays carried") {
        // A rotation written into local_transform is how gameplay turns a character. Treated like a teleport it
        // would end the carrying, and a character that turns in every step would be left behind by the platform.
        for_each_platform_drive([](platform_drive drive) {
            core::jobs::job_system jobs = {};
            physics_fixture fixture(jobs);
            const moving_platform moving = spawn_platform(fixture, drive);
            const ecs::entity character = fixture.spawn_character(math::vec3(0.0f));
            fixture.run_steps(10);
            REQUIRE(fixture.physics.character_ground(character) == physics::ground_state::ON_GROUND);
            const math::vec3 before = offset_on(fixture, character, moving);

            const math::vec3 up = math::vec3(0.0f, 1.0f, 0.0f);
            for (u32 step = 0; step < 60; ++step) {
                // One degree further in every step.
                scene::local_transform *local = fixture.world.get<scene::local_transform>(character);
                REQUIRE(local != nullptr);
                local->rotation = glm::angleAxis(math::radians(static_cast<f32>(step + 1)), up);
                step_platform(fixture, moving, PLATFORM_VELOCITY);
                CAPTURE(step);
                CHECK(testing::approx_equal(controller_of(fixture, character).velocity, math::vec3(0.0f), 1.0e-4f));
            }
            CHECK(fixture.position_of(moving.entity).x == doctest::Approx(2.0f).epsilon(0.01));
            CHECK(testing::approx_equal(offset_on(fixture, character, moving), before, 0.01f));
            CHECK(controller_of(fixture, character).ground == physics::ground_state::ON_GROUND);
            CHECK(controller_of(fixture, character).groundEntity == moving.entity);
            // The turns reached the body, and the pull left them in the transform.
            const std::optional<math::transform> pose = fixture.physics.body_pose(character);
            REQUIRE(pose.has_value());
            const math::quat turned = glm::angleAxis(math::radians(60.0f), up);
            CHECK(math::same_rotation(pose->rotation, turned, 1.0e-4f));
            const scene::local_transform *local = fixture.world.get<scene::local_transform>(character);
            REQUIRE(local != nullptr);
            CHECK(math::same_rotation(local->rotation, turned, 1.0e-4f));
            CONJURE_CHECK_OK(fixture.physics.validate());
        });
    }

    TEST_CASE("character_controller - a character on a rising and a sinking platform stays on it") {
        // The vertical part is not carried like the horizontal one: with APPLY_GRAVITY the step sets it to the
        // ground's, read from the ground's body as the step starts, and the component reads it back.
        for_each_platform_drive([](platform_drive drive) {
            core::jobs::job_system jobs = {};
            physics_fixture fixture(jobs);
            const moving_platform lift = spawn_platform(fixture, drive);
            const ecs::entity character = fixture.spawn_character(math::vec3(0.0f));
            fixture.run_steps(10);
            REQUIRE(fixture.physics.character_ground(character) == physics::ground_state::ON_GROUND);
            const math::vec3 before = offset_on(fixture, character, lift);

            // What the lift does, what velocity.y reads on it and how far above its place the character rides.
            math::vec3 velocity = math::vec3(0.0f);
            f32 reads = 0.0f;
            f32 behind = 0.0f;
            SUBCASE("rising") {
                velocity = math::vec3(0.0f, 1.0f, 0.0f);
                reads = 1.0f;
            }
            SUBCASE("rising and moving sideways at once") {
                velocity = math::vec3(2.0f, 1.0f, 0.0f);
                reads = 1.0f;
            }
            SUBCASE("sinking slower than the jump threshold of 0.1 m/s") {
                velocity = math::vec3(0.0f, -0.05f, 0.0f);
                reads = -0.05f;
            }
            SUBCASE("sinking faster than that: STICK_TO_FLOOR steps the character down after the lift") {
                // Standing still on ground that drops away at 1 m/s is, to the vertical rule, moving upwards
                // against the ground: a jump, which the rule leaves alone. The flag brings the character down
                // to the place the lift had one step earlier, and its velocity goes on reading zero.
                velocity = math::vec3(0.0f, -1.0f, 0.0f);
                reads = 0.0f;
                behind = 1.0f * STEP;
            }
            for (u32 step = 0; step < 60; ++step) {
                step_platform(fixture, lift, velocity);
                CAPTURE(step);
                const physics::character_controller &controller = controller_of(fixture, character);
                CHECK(controller.ground == physics::ground_state::ON_GROUND);
                CHECK(controller.groundEntity == lift.entity);
            }
            const math::vec3 expected = before + math::vec3(0.0f, behind, 0.0f);
            CHECK(testing::approx_equal(offset_on(fixture, character, lift), expected, 2.0e-3f));
            // Only the horizontal part is taken out again; the vertical part reads as the step set it.
            const physics::character_controller &controller = controller_of(fixture, character);
            CHECK(std::abs(controller.velocity.x) < 1.0e-4f);
            CHECK(std::abs(controller.velocity.z) < 1.0e-4f);
            CHECK(std::abs(controller.velocity.y - reads) < 1.0e-3f);
            CONJURE_CHECK_OK(fixture.physics.validate());
        });
    }

    TEST_CASE("character_controller - a jump off a moving platform onto a steep slope ends what the platform gave it") {
        // In the air the character keeps the platform's speed. Kept on ground that does not carry it, that speed
        // would push it along the slope, and where the step cancels the push the pull would still subtract it.
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const moving_platform moving = spawn_platform(fixture, platform_drive::TRANSFORM);
        const ecs::entity character = fixture.spawn_character(math::vec3(0.0f));
        fixture.run_steps(10);
        // Towards the slope and along it at once: what the slope does not block would show as a slide.
        const math::vec3 velocity = math::vec3(2.0f, 0.0f, 1.0f);
        ride(fixture, moving, 10, velocity);
        // A static slope of 60 degrees whose foot crosses the platform's top a metre ahead of the character: at
        // 2 m/s the jump meets it about a metre up, on the way down.
        const ecs::entity slope = spawn_slope(fixture, 60.0f);
        scene::local_transform *slopeLocal = fixture.world.get<scene::local_transform>(slope);
        REQUIRE(slopeLocal != nullptr);
        slopeLocal->position.x += fixture.position_of(character).x + 1.0f;

        physics::character_controller *jumping = fixture.world.get<physics::character_controller>(character);
        REQUIRE(jumping != nullptr);
        jumping->velocity.y = 5.0f;
        u32 airborne = 0;
        u32 onSlope = 0;
        for (u32 step = 0; step < 90; ++step) {
            const physics::ground_state atStart = controller_of(fixture, character).ground;
            const math::vec3 positionBefore = fixture.position_of(character);
            step_platform(fixture, moving, velocity);
            const physics::character_controller &controller = controller_of(fixture, character);
            if (atStart == physics::ground_state::IN_AIR) {
                ++airborne;
                continue;
            }
            if (atStart == physics::ground_state::ON_GROUND) {
                continue;
            }
            // A step that starts on the slope: ON_STEEP_GROUND, or NOT_SUPPORTED at its edge.
            ++onSlope;
            CAPTURE(step);
            CHECK(std::abs(fixture.position_of(character).z - positionBefore.z) < 1.0e-3f);
            CHECK(std::abs(controller.velocity.x) < 1.0e-3f);
            CHECK(std::abs(controller.velocity.z) < 1.0e-3f);
        }
        // Measured: 43 steps in the air, 23 on the slope.
        CHECK(airborne > 20);
        CHECK(onSlope > 5);
        // Slid down the slope, it stands on the platform again.
        const physics::character_controller &landed = controller_of(fixture, character);
        CHECK(landed.ground == physics::ground_state::ON_GROUND);
        CHECK(landed.groundEntity == moving.entity);
    }

    TEST_CASE("character_controller - a character standing on a sliding dynamic body is carried with it") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        // A heavy slab on a floor without friction keeps the speed it is given.
        physics::physics_material ice = {};
        ice.friction = 0.0f;
        const ecs::entity ground = fixture.spawn_ground();
        CONJURE_REQUIRE_OK(fixture.world.add<physics::physics_material>(ground, ice));
        const ecs::entity slab = fixture.create_at(math::vec3(0.0f, 0.25f, 0.0f));
        physics::rigid_body body = {};
        body.mass = 5000.0f;
        body.linearDamping = 0.0f;
        body.flags = physics::rigid_body_flags::LOCK_ROTATION;
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(slab, body));
        physics::box_collider collider = {};
        collider.halfExtents = math::vec3(10.0f, 0.25f, 4.0f);
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(slab, collider));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::physics_material>(slab, ice));
        const ecs::entity character = fixture.spawn_character(math::vec3(0.0f, 0.5f, 0.0f));
        fixture.run_steps(20);
        REQUIRE(controller_of(fixture, character).ground == physics::ground_state::ON_GROUND);
        REQUIRE(controller_of(fixture, character).groundEntity == slab);
        const math::vec3 before = fixture.position_of(character) - fixture.position_of(slab);

        CONJURE_REQUIRE_OK(fixture.physics.set_linear_velocity(slab, PLATFORM_VELOCITY));
        for (u32 step = 0; step < 60; ++step) {
            fixture.run_step();
            CAPTURE(step);
            const physics::character_controller &controller = controller_of(fixture, character);
            CHECK(controller.ground == physics::ground_state::ON_GROUND);
        }
        CHECK(fixture.position_of(slab).x > 1.5f);
        const math::vec3 offset = fixture.position_of(character) - fixture.position_of(slab);
        CHECK(testing::approx_equal(offset, before, 0.01f));
        const physics::character_controller &controller = controller_of(fixture, character);
        CHECK(controller.groundEntity == slab);
        CHECK(std::abs(controller.velocity.x) < 1.0e-3f);
        CHECK(std::abs(controller.velocity.z) < 1.0e-3f);
    }

    TEST_CASE("character_controller - a character without APPLY_GRAVITY is carried while it presses on the platform") {
        // Carrying asks for ON_GROUND at the start of the step, not for the flag. Without gravity nothing but
        // the character's own velocity keeps it on the ground.
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const moving_platform moving = spawn_platform(fixture, platform_drive::TRANSFORM);
        physics::character_controller settings = {};
        settings.flags = physics::character_flags::STICK_TO_FLOOR | physics::character_flags::WALK_STAIRS;
        settings.velocity = math::vec3(0.0f, -1.0f, 0.0f);
        const ecs::entity character = fixture.spawn_character(math::vec3(0.0f), settings);
        fixture.run_steps(10);
        REQUIRE(fixture.physics.character_ground(character) == physics::ground_state::ON_GROUND);
        const math::vec3 before = offset_on(fixture, character, moving);

        ride(fixture, moving, 60);
        CHECK(testing::approx_equal(offset_on(fixture, character, moving), before, 0.01f));
        const physics::character_controller &controller = controller_of(fixture, character);
        CHECK(controller.ground == physics::ground_state::ON_GROUND);
        // The velocity it was given is still its own: the pull took the platform's share out again.
        CHECK(testing::approx_equal(controller.velocity, math::vec3(0.0f, -1.0f, 0.0f), 1.0e-4f));
    }

    TEST_CASE("character_controller - a teleport off a rising platform keeps the platform's vertical velocity") {
        // The limit of 'a teleport ends the carrying': only the horizontal part is carried and ended. The
        // vertical velocity of a grounded character is the ground's and sits in its component, and the step
        // after the teleport still takes the character for standing where it stood.
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const moving_platform lift = spawn_platform(fixture, platform_drive::TRANSFORM);
        const ecs::entity character = fixture.spawn_character(math::vec3(0.0f));
        fixture.run_steps(10);
        const math::vec3 rising = math::vec3(2.0f, 1.0f, 0.0f);
        ride(fixture, lift, 20, rising);
        REQUIRE(controller_of(fixture, character).groundEntity == lift.entity);
        REQUIRE(controller_of(fixture, character).velocity.y == doctest::Approx(rising.y).epsilon(0.01));

        const math::vec3 target = math::vec3(100.0f, 50.0f, 0.0f);
        SUBCASE("through the teleport command") {
            math::transform pose = {};
            pose.position = target;
            CONJURE_REQUIRE_OK(fixture.physics.teleport(character, pose));
        }
        SUBCASE("through its local_transform") {
            scene::local_transform *local = fixture.world.get<scene::local_transform>(character);
            REQUIRE(local != nullptr);
            local->position = target;
        }
        // The step after the teleport: nothing of the lift's 2 m/s sideways, all of its 1 m/s upwards, no gravity.
        step_platform(fixture, lift, rising);
        CHECK(std::abs(fixture.position_of(character).x - target.x) < 1.0e-3f);
        CHECK(fixture.position_of(character).y == doctest::Approx(target.y + rising.y * STEP).epsilon(1.0e-5));
        CHECK(std::abs(controller_of(fixture, character).velocity.x) < 1.0e-4f);
        CHECK(controller_of(fixture, character).velocity.y == doctest::Approx(rising.y).epsilon(0.01));
        CHECK(controller_of(fixture, character).ground == physics::ground_state::IN_AIR);
        // From then on it falls, starting from that velocity.
        step_platform(fixture, lift, rising);
        CHECK(controller_of(fixture, character).velocity.y == doctest::Approx(rising.y - GRAVITY * STEP).epsilon(0.01));
        CHECK(std::abs(fixture.position_of(character).x - target.x) < 1.0e-3f);
    }
}
