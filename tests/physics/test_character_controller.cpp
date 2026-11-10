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

    TEST_CASE("character_controller - teleport moves a character and move_kinematic is dropped") {
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

        // A character is not a kinematic body: move_kinematic is dropped and counted.
        CONJURE_REQUIRE_OK(fixture.physics.move_kinematic(character, pose));
        fixture.run_step();
        CHECK(fixture.physics.stats().droppedCommands == 1);
        CONJURE_CHECK_OK(fixture.physics.validate());
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
}
