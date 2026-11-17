#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/doctest.h"
#include "physics_test_fixture.h"

#include "conjure/physics/components.h"

#include <cmath>
#include <vector>

using namespace conjure;
using namespace physics_test;

TEST_SUITE("physics/physics_stack") {

    TEST_CASE("physics_stack - ten boxes stacked over a plane settle with the top box at its analytic height") {
        for_each_job_mode([](core::jobs::job_system &jobs) {
            physics::physics_settings settings = test_settings();
            settings.timeBeforeSleepSeconds = 0.3f;
            physics_fixture fixture(jobs, settings);
            (void)fixture.spawn_ground();
            std::vector<ecs::entity> stack = {};
            for (u32 level = 0; level < 10; ++level) {
                stack.push_back(fixture.spawn_box(math::vec3(0.0f, 0.5f + static_cast<f32>(level), 0.0f)));
            }
            fixture.run_steps(180);
            // Box centres sit at 0.5, 1.5, ..., 9.5 m: the top box's top face is at 10 m. Jolt's contact
            // solver leaves about 1.3 mm of penetration per resting contact whatever the penetration slop
            // or the position iteration count (measured: 0.5 to 1.3 cm at the top for slops 0 to 0.02 and
            // 2 to 16 position steps), so the stack is 2 cm tolerant where a single box is 1 cm.
            const f32 topCentre = fixture.position_of(stack.back()).y;
            CHECK(std::abs(topCentre + 0.5f - 10.0f) < 0.02f);
            for (u32 level = 0; level < 10; ++level) {
                CAPTURE(level);
                CHECK(std::abs(fixture.position_of(stack[level]).y - (0.5f + static_cast<f32>(level))) < 0.02f);
                CHECK(fixture.settled(stack[level]));
            }
            CHECK(fixture.physics.active_body_count() == 0);
            CONJURE_CHECK_OK(fixture.physics.validate());
        });
    }

    TEST_CASE("physics_stack - one step over 2000 settled boxes allocates nothing [slow]") {
        for_each_job_mode([](core::jobs::job_system &jobs) {
            physics::physics_settings settings = test_settings();
            settings.timeBeforeSleepSeconds = 0.1f;
            physics_fixture fixture(jobs, settings);
            (void)fixture.spawn_ground();
            for (u32 index = 0; index < 2000; ++index) {
                const f32 x = static_cast<f32>(index % 45) * 2.0f - 44.0f;
                const f32 z = static_cast<f32>(index / 45) * 2.0f - 44.0f;
                (void)fixture.spawn_box(math::vec3(x, 0.5f, z));
            }
            fixture.run_steps(120);
            REQUIRE(fixture.physics.body_count() == 2001);
            CHECK(fixture.physics.active_body_count() == 0);
            CONJURE_CHECK_NO_ALLOCATIONS(fixture.physics.step(STEP));
            CONJURE_CHECK_NO_ALLOCATIONS(fixture.run_step());
            CHECK(fixture.physics.active_body_count() == 0);
        });
    }

    TEST_CASE("physics_stack - a step over static triggers that hold sleeping boxes allocates nothing") {
        // Jolt steps every static trigger as an active body (code sweep F408): the steady state of a level
        // full of trigger volumes must stay as allocation-free as one without them (P15).
        for_each_job_mode([](core::jobs::job_system &jobs) {
            physics::physics_settings settings = test_settings();
            settings.timeBeforeSleepSeconds = 0.1f;
            physics_fixture fixture(jobs, settings);
            (void)fixture.spawn_ground();
            // 400 boxes two metres apart and 100 trigger volumes of 4 x 2 x 4 m, each over four of them.
            for (u32 index = 0; index < 400; ++index) {
                const f32 x = static_cast<f32>(index % 20) * 2.0f - 19.0f;
                const f32 z = static_cast<f32>(index / 20) * 2.0f - 19.0f;
                (void)fixture.spawn_box(math::vec3(x, 0.5f, z));
            }
            for (u32 index = 0; index < 100; ++index) {
                const f32 x = static_cast<f32>(index % 10) * 4.0f - 18.0f;
                const f32 z = static_cast<f32>(index / 10) * 4.0f - 18.0f;
                (void)fixture.spawn_static_trigger(math::vec3(x, 1.0f, z), math::vec3(1.9f, 1.0f, 1.9f));
            }
            // Both entities left null: the trigger events of every pair.
            trigger_tally tally = {};
            tally.run(fixture, 120);
            REQUIRE(fixture.physics.body_count() == 501);
            // Every box entered its volume once and is still inside it, asleep.
            CHECK(tally.enters == 400);
            CHECK(tally.exits == 0);
            CHECK(fixture.physics.active_body_count() == 0);
            CONJURE_CHECK_NO_ALLOCATIONS(fixture.physics.step(STEP));
            CONJURE_CHECK_NO_ALLOCATIONS(fixture.run_step());
            CHECK(fixture.physics.contact_events().empty());
            CHECK(fixture.physics.active_body_count() == 0);
        });
    }
}
