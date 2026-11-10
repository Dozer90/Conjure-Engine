#include "conjure/testing/doctest.h"
#include "conjure/testing/log_capture.h"
#include "physics_test_fixture.h"

#include "conjure/core/string_id.h"
#include "conjure/physics/components.h"
#include "conjure/physics/physics_error.h"
#include "conjure/physics/physics_layers.h"

using namespace conjure;
using namespace physics_test;

namespace {

    /// <summary>
    /// Layers default, a (ignoring itself) and b.
    /// </summary>
    [[nodiscard]] physics::physics_settings layered_settings() {
        physics::physics_settings settings = test_settings();
        settings.allowSleeping = false;
        REQUIRE(settings.layers.add_layer(core::string_id("a")).has_value());
        REQUIRE(settings.layers.add_layer(core::string_id("b")).has_value());
        settings.layers.set_collides(1, 1, false);
        return settings;
    }
}

TEST_SUITE("physics/layers") {

    TEST_CASE("layers - ignore a a lets two layer-a boxes pass through each other while a hits default") {
        for_each_job_mode([](core::jobs::job_system &jobs) {
            physics_fixture fixture(jobs, layered_settings());
            CHECK(fixture.physics.layers().count() == 3);
            (void)fixture.spawn_ground(0);
            const ecs::entity resting = fixture.spawn_box(
              math::vec3(0.0f, 0.5f, 0.0f),
              physics::motion_type::STATIC,
              math::vec3(0.5f),
              1
            );
            const ecs::entity falling = fixture.spawn_box(
              math::vec3(0.0f, 3.0f, 0.0f),
              physics::motion_type::DYNAMIC,
              math::vec3(0.5f),
              1
            );
            fixture.run_steps(120);
            // The falling box ignored the resting one and landed on the (default layer) ground.
            CHECK(fixture.position_of(falling).y == doctest::Approx(0.5f).epsilon(0.05));
            CHECK(fixture.position_of(resting).y == doctest::Approx(0.5f));
            bool pairSeen = false;
            for (const physics::contact_event &event : fixture.physics.contact_events()) {
                pairSeen = pairSeen || (event.a == resting && event.b == falling)
                  || (event.a == falling && event.b == resting);
            }
            CHECK_FALSE(pairSeen);
        });
    }

    TEST_CASE("layers - a rigid_body layer change at runtime re-filters the next step") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs, layered_settings());
        (void)fixture.spawn_ground(0);
        const ecs::entity plate = fixture.spawn_box(
          math::vec3(0.0f, 2.0f, 0.0f),
          physics::motion_type::STATIC,
          math::vec3(2.0f, 0.1f, 2.0f),
          1
        );
        const ecs::entity ball = fixture.spawn_sphere(math::vec3(0.0f, 4.0f, 0.0f));
        fixture.run_steps(90);
        // Default collides with a: the ball rests on the plate.
        CHECK(fixture.position_of(ball).y == doctest::Approx(2.6f).epsilon(0.02));

        physics::rigid_body *body = fixture.world.get<physics::rigid_body>(ball);
        REQUIRE(body != nullptr);
        body->layer = 1;
        fixture.run_steps(120);
        // Now both are on layer a, which ignores itself: the ball falls through to the ground.
        CHECK(fixture.position_of(ball).y == doctest::Approx(0.5f).epsilon(0.05));
        bool anyWithPlate = false;
        for (const physics::contact_event &event : fixture.physics.contact_events()) {
            anyWithPlate = anyWithPlate || event.a == plate || event.b == plate;
            if (event.a == ball || event.b == ball) {
                CHECK((event.a == ball ? event.layerA : event.layerB) == 1);
            }
        }
        CHECK_FALSE(anyWithPlate);
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("layers - an unknown layer index warns and uses layer 0") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs, layered_settings());
        (void)fixture.spawn_ground(0);
        const ecs::entity box = fixture.spawn_box(
          math::vec3(0.0f, 2.0f, 0.0f),
          physics::motion_type::DYNAMIC,
          math::vec3(0.5f),
          17
        );
        testing::log_capture logs = {};
        fixture.run_steps(90);
        CHECK(logs.contains(core::log_level::WARN, "UNKNOWN_LAYER"));
        CHECK(fixture.position_of(box).y == doctest::Approx(0.5f).epsilon(0.05));
        bool found = false;
        for (const physics::contact_event &event : fixture.physics.contact_events()) {
            if (event.a == box || event.b == box) {
                found = true;
                CHECK(event.layerA == 0);
                CHECK(event.layerB == 0);
            }
        }
        CHECK(found);
        // validate() reports the component's stale layer.
        CONJURE_CHECK_ERROR(fixture.physics.validate(), physics::physics_error::UNKNOWN_LAYER);
        physics::rigid_body *body = fixture.world.get<physics::rigid_body>(box);
        REQUIRE(body != nullptr);
        body->layer = 2;
        fixture.run_step();
        CONJURE_CHECK_OK(fixture.physics.validate());
    }
}
