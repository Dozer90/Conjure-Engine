#include "conjure/testing/approx.h"
#include "conjure/testing/doctest.h"
#include "physics_test_fixture.h"

#include "conjure/core/math/transform.h"
#include "conjure/physics/components.h"
#include "conjure/scene/components.h"

#include <cmath>

using namespace conjure;
using namespace physics_test;

namespace {

    [[nodiscard]] ecs::entity spawn_undamped(physics_fixture &fixture, const math::vec3 &position) {
        const ecs::entity box = fixture.create_at(position);
        physics::rigid_body body = {};
        body.linearDamping = 0.0f;
        body.flags = physics::rigid_body_flags::NONE;
        REQUIRE(fixture.world.add<physics::rigid_body>(box, body).has_value());
        REQUIRE(fixture.world.add<physics::box_collider>(box).has_value());
        return box;
    }
}

TEST_SUITE("physics/fixed_step") {

    TEST_CASE("fixed_step - 600 steps at 1/60 are ten seconds of fall") {
        for_each_job_mode([](core::jobs::job_system &jobs) {
            physics_fixture fixture(jobs);
            const ecs::entity box = spawn_undamped(fixture, math::vec3(0.0f, 1000.0f, 0.0f));
            fixture.run_steps(600);
            const f32 expected = 1000.0f - free_fall_drop(600);
            // Ten seconds of fall is 490.9 m; float accumulation over 600 steps stays within a few cm.
            CHECK(std::abs(fixture.position_of(box).y - expected) < 0.05f);
            const physics::rigid_body *pulled = fixture.world.get<physics::rigid_body>(box);
            REQUIRE(pulled != nullptr);
            CHECK(pulled->linearVelocity.y == doctest::Approx(-GRAVITY * 10.0f).epsilon(0.001));
        });
    }

    TEST_CASE("fixed_step - interpolated_transform lerp at 0.5 is the midpoint of the two pulled poses") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const ecs::entity box = spawn_undamped(fixture, math::vec3(0.0f, 100.0f, 0.0f));
        fixture.run_step();
        const scene::interpolated_transform *first = fixture.world.get<scene::interpolated_transform>(box);
        REQUIRE(first != nullptr);
        // The first pull writes previous = current so nothing interpolates from the origin.
        CHECK(first->previous == first->current);
        CHECK(first->current.position.y < 100.0f);

        const math::transform earlier = first->current;
        fixture.run_step();
        const scene::interpolated_transform *second = fixture.world.get<scene::interpolated_transform>(box);
        REQUIRE(second != nullptr);
        CHECK(second->previous == earlier);
        CHECK(second->current.position.y < earlier.position.y);
        const math::transform middle = math::transform::lerp(second->previous, second->current, 0.5f);
        CHECK(middle.position.y == doctest::Approx((second->previous.position.y + second->current.position.y) * 0.5f));
        CHECK(testing::approx_equal(middle.rotation, math::quat(1.0f, 0.0f, 0.0f, 0.0f)));
        CHECK(middle.scale == math::vec3(1.0f));
    }

    TEST_CASE("fixed_step - collisionSteps 2 integrates two half steps and matches its closed form within 1 mm") {
        // Two collision steps integrate twice with dt / 2, so the drop after n steps is
        // g (dt/2)^2 (2n)(2n+1)/2; it differs from a single step by g dt^2 n / 4 (4 cm over 60 steps),
        // which is why the two are compared against their own closed forms rather than each other.
        physics::physics_settings single = test_settings();
        physics::physics_settings halved = test_settings();
        halved.collisionSteps = 2;
        core::jobs::job_system jobs = {};
        physics_fixture one(jobs, single);
        physics_fixture two(jobs, halved);
        const ecs::entity boxOne = spawn_undamped(one, math::vec3(0.0f, 100.0f, 0.0f));
        const ecs::entity boxTwo = spawn_undamped(two, math::vec3(0.0f, 100.0f, 0.0f));
        one.run_steps(60);
        two.run_steps(60);
        const f32 expectedOne = 100.0f - free_fall_drop(60, STEP);
        const f32 expectedTwo = 100.0f - free_fall_drop(120, STEP * 0.5f);
        CHECK(std::abs(one.position_of(boxOne).y - expectedOne) < 0.001f);
        CHECK(std::abs(two.position_of(boxTwo).y - expectedTwo) < 0.001f);
        CHECK(std::abs(expectedOne - expectedTwo - (-GRAVITY * STEP * STEP * 60.0f / 4.0f)) < 1.0e-3f);
    }

    TEST_CASE("fixed_step - a zero or negative delta is ignored by step") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const ecs::entity box = spawn_undamped(fixture, math::vec3(0.0f, 10.0f, 0.0f));
        fixture.physics.push_transforms(STEP);
        fixture.physics.step(0.0f);
        fixture.physics.step(-1.0f);
        fixture.physics.pull_transforms();
        CHECK(fixture.position_of(box).y == doctest::Approx(10.0f));
    }
}
