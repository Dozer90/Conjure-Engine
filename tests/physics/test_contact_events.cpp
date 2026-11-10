#include "conjure/testing/assert_capture.h"
#include "conjure/testing/doctest.h"
#include "conjure/testing/log_capture.h"
#include "physics_test_fixture.h"

#include "conjure/physics/components.h"
#include "conjure/physics/contact_events.h"
#include "conjure/scene/components.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <set>
#include <tuple>
#include <vector>

using namespace conjure;
using namespace physics_test;

namespace {

    /// <summary>
    /// Settings without sleeping so counting tests see a STAY every step.
    /// </summary>
    [[nodiscard]] physics::physics_settings awake_settings() {
        physics::physics_settings settings = test_settings();
        settings.allowSleeping = false;
        return settings;
    }

    [[nodiscard]] u32 count_kind(std::span<const physics::contact_event> events, physics::contact_kind kind) {
        u32 count = 0;
        for (const physics::contact_event &event : events) {
            if (event.kind == kind) {
                ++count;
            }
        }
        return count;
    }

    [[nodiscard]] bool involves(const physics::contact_event &event, ecs::entity a, ecs::entity b) {
        return (event.a == a && event.b == b) || (event.a == b && event.b == a);
    }
}

TEST_SUITE("physics/contact_events") {

    TEST_CASE("contact_kind - to_string names every kind") {
        CHECK(physics::to_string(physics::contact_kind::ENTER) == "ENTER");
        CHECK(physics::to_string(physics::contact_kind::STAY) == "STAY");
        CHECK(physics::to_string(physics::contact_kind::EXIT) == "EXIT");
        CHECK(physics::to_string(physics::contact_kind::TRIGGER_ENTER) == "TRIGGER_ENTER");
        CHECK(physics::to_string(physics::contact_kind::TRIGGER_EXIT) == "TRIGGER_EXIT");
        CHECK(physics::to_string(static_cast<physics::contact_kind>(42)) == "UNKNOWN");
        static_assert(sizeof(physics::contact_event) == 48);
        const physics::contact_event event = {};
        CHECK(event.kind == physics::contact_kind::ENTER);
        CHECK(event.impulse == 0.0f);
        CHECK(event.a.is_null());
    }

    TEST_CASE("contact_events - a box dropped on the ground reports one ENTER then one STAY per step then one EXIT") {
        for_each_job_mode([](core::jobs::job_system &jobs) {
            physics_fixture fixture(jobs, awake_settings());
            const ecs::entity ground = fixture.spawn_ground();
            const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 1.0f, 0.0f));
            u32 enters = 0;
            u32 stays = 0;
            u32 exits = 0;
            u32 firstEnterStep = 0;
            for (u32 step = 0; step < 90; ++step) {
                fixture.run_step();
                const std::span<const physics::contact_event> events = fixture.physics.contact_events();
                CHECK(events.size() <= 1);
                for (const physics::contact_event &event : events) {
                    CHECK(involves(event, ground, box));
                    CHECK(event.a == ground);   // the ground was created first and has the lower body id
                    CHECK(event.b == box);
                    if (event.kind == physics::contact_kind::ENTER) {
                        ++enters;
                        firstEnterStep = step;
                        CHECK(event.contactCount > 0);
                        CHECK(event.normal.y > 0.9f);
                        CHECK(event.point.y == doctest::Approx(0.0f).epsilon(0.05));
                    } else if (event.kind == physics::contact_kind::STAY) {
                        ++stays;
                    } else {
                        ++exits;
                    }
                }
            }
            CHECK(enters == 1);
            CHECK(exits == 0);
            CHECK(stays == 89 - firstEnterStep);

            // An impulse lifts the box off the ground: exactly one EXIT.
            CONJURE_REQUIRE_OK(fixture.physics.add_impulse(box, math::vec3(0.0f, 8.0f, 0.0f)));
            u32 exitsAfter = 0;
            for (u32 step = 0; step < 20; ++step) {
                fixture.run_step();
                exitsAfter += count_kind(fixture.physics.contact_events(), physics::contact_kind::EXIT);
            }
            CHECK(exitsAfter == 1);
            CHECK(fixture.position_of(box).y > 0.6f);
        });
    }

    TEST_CASE("contact_events - 50 boxes landing in one step are sorted by body id pair and unique per pair and kind") {
        for_each_job_mode([](core::jobs::job_system &jobs) {
            physics_fixture fixture(jobs, awake_settings());
            const ecs::entity ground = fixture.spawn_ground();
            std::vector<ecs::entity> boxes = {};
            for (u32 index = 0; index < 50; ++index) {
                boxes.push_back(fixture.spawn_box(math::vec3(static_cast<f32>(index) * 1.5f - 37.0f, 0.55f, 0.0f)));
            }
            // 5 cm of fall takes six steps; by twelve every box rests (STAY) and no box touches another.
            fixture.run_steps(12);
            const std::span<const physics::contact_event> events = fixture.physics.contact_events();
            CHECK(events.size() == 50);

            // Sorted by (bodyIdA, bodyIdB): body ids are assigned in creation order, so the entity
            // creation order of the boxes is the event order; the ground (first body) is always a.
            std::vector<physics::contact_event> sorted(events.begin(), events.end());
            std::stable_sort(sorted.begin(), sorted.end(), [&](
              const physics::contact_event &left,
              const physics::contact_event &right
            ) {
                const auto rank = [&](ecs::entity entity) {
                    return static_cast<u32>(std::find(boxes.begin(), boxes.end(), entity) - boxes.begin());
                };
                return std::tuple(
                  rank(left.b),
                  static_cast<u8>(left.kind)
                ) < std::tuple(rank(right.b), static_cast<u8>(right.kind));
            });
            CHECK(std::memcmp(sorted.data(), events.data(), events.size_bytes()) == 0);
            std::set<std::tuple<u32, u32, u8>> seen = {};
            for (const physics::contact_event &event : events) {
                CHECK(event.a == ground);
                CHECK(std::find(boxes.begin(), boxes.end(), event.b) != boxes.end());
                CHECK(seen.insert(std::tuple(event.a.index, event.b.index, static_cast<u8>(event.kind))).second);
            }
        });
    }

    TEST_CASE("contact_events - a trigger reports TRIGGER_ENTER once nothing while inside and TRIGGER_EXIT once") {
        for_each_job_mode([](core::jobs::job_system &jobs) {
            physics_fixture fixture(jobs, awake_settings());
            const ecs::entity trigger = fixture.create_at(math::vec3(0.0f, 5.0f, 0.0f));
            physics::rigid_body triggerBody = {};
            triggerBody.motionType = physics::motion_type::STATIC;
            CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(trigger, triggerBody));
            physics::box_collider volume = {};
            volume.halfExtents = math::vec3(2.0f, 1.0f, 2.0f);
            volume.isTrigger = true;
            CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(trigger, volume));
            const ecs::entity sphere = fixture.spawn_sphere(math::vec3(0.0f, 9.0f, 0.0f), 0.25f);

            u32 enters = 0;
            u32 exits = 0;
            u32 others = 0;
            for (u32 step = 0; step < 150; ++step) {
                fixture.run_step();
                for (const physics::contact_event &event : fixture.physics.contact_events()) {
                    CHECK(involves(event, trigger, sphere));
                    if (event.kind == physics::contact_kind::TRIGGER_ENTER) {
                        ++enters;
                        CHECK(event.impulse == 0.0f);
                    } else if (event.kind == physics::contact_kind::TRIGGER_EXIT) {
                        ++exits;
                    } else {
                        ++others;
                    }
                }
            }
            CHECK(enters == 1);
            CHECK(exits == 1);
            CHECK(others == 0);
            CHECK(fixture.position_of(sphere).y < 3.0f);
        });
    }

    TEST_CASE("contact_events - destroying an entity in contact produces EXIT the next step and nothing after") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs, awake_settings());
        const ecs::entity ground = fixture.spawn_ground();
        const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 0.5f, 0.0f));
        fixture.run_steps(30);
        REQUIRE(count_kind(fixture.physics.contact_events(), physics::contact_kind::STAY) == 1);

        CONJURE_REQUIRE_OK(fixture.world.destroy_entity(box));
        // The exit is synthesised at destruction and reported by the next collect.
        fixture.run_step();
        const std::span<const physics::contact_event> events = fixture.physics.contact_events();
        REQUIRE(events.size() == 1);
        CHECK(events[0].kind == physics::contact_kind::EXIT);
        CHECK(events[0].a == ground);
        CHECK(events[0].b == box);
        CHECK_FALSE(fixture.world.is_alive(events[0].b));
        CHECK(events[0].point == math::vec3(0.0f));
        fixture.run_steps(5);
        CHECK(fixture.physics.contact_events().empty());
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("contact_events - impulse is estimated on ENTER when enabled and zero otherwise") {
        physics::physics_settings estimating = awake_settings();
        physics::physics_settings silent = awake_settings();
        silent.estimateContactImpulses = false;
        core::jobs::job_system jobs = {};
        for (const bool estimate : {true, false}) {
            CAPTURE(estimate);
            physics_fixture fixture(jobs, estimate ? estimating : silent);
            (void)fixture.spawn_ground();
            (void)fixture.spawn_box(math::vec3(0.0f, 3.0f, 0.0f));
            f32 enterImpulse = -1.0f;
            for (u32 step = 0; step < 90 && enterImpulse < 0.0f; ++step) {
                fixture.run_step();
                for (const physics::contact_event &event : fixture.physics.contact_events()) {
                    if (event.kind == physics::contact_kind::ENTER) {
                        enterImpulse = event.impulse;
                    }
                }
            }
            REQUIRE(enterImpulse >= 0.0f);
            if (estimate) {
                CHECK(enterImpulse > 0.5f);
            } else {
                CHECK(enterImpulse == 0.0f);
            }
        }
    }

    TEST_CASE("contact_events - the span is empty after the next push_transforms") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs, awake_settings());
        (void)fixture.spawn_ground();
        (void)fixture.spawn_box(math::vec3(0.0f, 0.5f, 0.0f));
        fixture.run_steps(5);
        REQUIRE_FALSE(fixture.physics.contact_events().empty());
        fixture.physics.push_transforms(STEP);
        CHECK(fixture.physics.contact_events().empty());
        fixture.physics.step(STEP);
        CHECK(fixture.physics.contact_events().empty());
        fixture.physics.pull_transforms();
        fixture.physics.collect_contact_events();
        CHECK_FALSE(fixture.physics.contact_events().empty());
    }

    TEST_CASE("contact_events - a buffer of four drops and counts the overflow") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = awake_settings();
        settings.maxContactEventsPerStep = 4;
        physics_fixture fixture(jobs, settings);
        (void)fixture.spawn_ground();
        for (u32 index = 0; index < 10; ++index) {
            (void)fixture.spawn_box(math::vec3(static_cast<f32>(index) * 1.5f, 0.5f, 0.0f));
        }
        testing::log_capture logs = {};
        fixture.run_steps(5);
        CHECK(fixture.physics.contact_events().size() <= 4);
        CHECK(fixture.physics.stats().droppedContactEvents > 0);
        CHECK(logs.count(core::log_level::WARN) > 0);
    }

    TEST_CASE("contact_events - a kinematic trigger plate swept through a static box reports one enter and one exit") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs, awake_settings());
        // A static sensor only sees active bodies; a kinematic one also sees static geometry
        // (mCollideKinematicVsNonDynamic), so the sweeping plate reports the wall it passes through.
        const ecs::entity wall = fixture.spawn_box(math::vec3(0.0f, 0.0f, 0.0f), physics::motion_type::STATIC);
        const ecs::entity plate = fixture.create_at(math::vec3(-5.0f, 0.0f, 0.0f));
        physics::rigid_body plateBody = {};
        plateBody.motionType = physics::motion_type::KINEMATIC;
        plateBody.flags = physics::rigid_body_flags::KINEMATIC_MOVES_WITH_TRANSFORM;
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(plate, plateBody));
        physics::box_collider volume = {};
        volume.halfExtents = math::vec3(0.5f, 2.0f, 2.0f);
        volume.isTrigger = true;
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(plate, volume));
        fixture.run_step();

        u32 enters = 0;
        u32 exits = 0;
        u32 others = 0;
        for (u32 step = 0; step < 100; ++step) {
            scene::local_transform *local = fixture.world.get<scene::local_transform>(plate);
            REQUIRE(local != nullptr);
            local->position.x += 0.1f;
            fixture.run_step();
            for (const physics::contact_event &event : fixture.physics.contact_events()) {
                CHECK(involves(event, wall, plate));
                if (event.kind == physics::contact_kind::TRIGGER_ENTER) {
                    ++enters;
                } else if (event.kind == physics::contact_kind::TRIGGER_EXIT) {
                    ++exits;
                } else {
                    ++others;
                }
            }
        }
        CHECK(enters == 1);
        CHECK(exits == 1);
        CHECK(others == 0);
        CHECK(fixture.position_of(plate).x == doctest::Approx(5.0f).epsilon(0.01));
        // The wall never moved: a sensor has no response.
        CHECK(fixture.position_of(wall) == math::vec3(0.0f));
    }

    TEST_CASE("contact_events - destroying a body inside a trigger reports TRIGGER_EXIT the next step") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = awake_settings();
        settings.gravity = math::vec3(0.0f);
        physics_fixture fixture(jobs, settings);
        const ecs::entity trigger = fixture.create_at(math::vec3(0.0f, 5.0f, 0.0f));
        physics::rigid_body triggerBody = {};
        triggerBody.motionType = physics::motion_type::STATIC;
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(trigger, triggerBody));
        physics::box_collider volume = {};
        volume.halfExtents = math::vec3(2.0f);
        volume.isTrigger = true;
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(trigger, volume));
        const ecs::entity sphere = fixture.spawn_sphere(math::vec3(0.0f, 5.0f, 0.0f), 0.25f);
        fixture.run_steps(5);
        REQUIRE(count_kind(fixture.physics.contact_events(), physics::contact_kind::TRIGGER_EXIT) == 0);

        CONJURE_REQUIRE_OK(fixture.world.destroy_entity(sphere));
        fixture.run_step();
        const std::span<const physics::contact_event> events = fixture.physics.contact_events();
        REQUIRE(events.size() == 1);
        CHECK(events[0].kind == physics::contact_kind::TRIGGER_EXIT);
        CHECK(events[0].a == trigger);
        CHECK(events[0].b == sphere);
        CHECK_FALSE(fixture.world.is_alive(events[0].b));
        fixture.run_steps(3);
        CHECK(fixture.physics.contact_events().empty());
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("regression - review 6 adding a collider to an implicit static floor keeps its body and contacts") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs, awake_settings());
        // A floor made of a collider alone (no rigid_body) is an implicit static body.
        const ecs::entity floor = fixture.create_at(math::vec3(0.0f, -0.5f, 0.0f));
        physics::box_collider slab = {};
        slab.halfExtents = math::vec3(10.0f, 0.5f, 10.0f);
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(floor, slab));
        const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 0.5f, 0.0f));
        fixture.run_steps(30);
        REQUIRE(count_kind(fixture.physics.contact_events(), physics::contact_kind::STAY) == 1);

        // A second collider far from the box rebuilds the floor's shape in place: no new body, so no
        // EXIT / ENTER pair for the box resting on it.
        physics::sphere_collider bump = {};
        bump.offset = math::vec3(8.0f, 1.0f, 8.0f);
        CONJURE_REQUIRE_OK(fixture.world.add<physics::sphere_collider>(floor, bump));
        u32 enters = 0;
        u32 exits = 0;
        u32 stays = 0;
        for (u32 step = 0; step < 5; ++step) {
            fixture.run_step();
            enters += count_kind(fixture.physics.contact_events(), physics::contact_kind::ENTER);
            exits += count_kind(fixture.physics.contact_events(), physics::contact_kind::EXIT);
            stays += count_kind(fixture.physics.contact_events(), physics::contact_kind::STAY);
        }
        CHECK(enters == 0);
        CHECK(exits == 0);
        CHECK(stays == 5);
        CHECK(fixture.physics.has_body(floor));
        CHECK(fixture.physics.body_count() == 2);
        std::array<ecs::entity, 2> out = {};
        CHECK(fixture.physics.overlap_sphere(math::vec3(8.0f, 0.5f, 8.0f), 0.1f, out) == 1);
        CHECK(out[0] == floor);
        CHECK(fixture.position_of(box).y == doctest::Approx(0.5f).epsilon(0.02));
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("contact_events - a step past maxContactConstraints counts an update error and warns") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = awake_settings();
        settings.maxContactConstraints = 4;   // the smallest Jolt accepts
        physics_fixture fixture(jobs, settings);
        (void)fixture.spawn_ground();
        for (u32 index = 0; index < 8; ++index) {
            (void)fixture.spawn_box(math::vec3(static_cast<f32>(index) * 2.0f, 0.5f, 0.0f));
        }
        testing::log_capture logs = {};
        // Jolt asserts on every update error in assert builds (the hook logs an ERROR and asserts);
        // the world still degrades as designed: the error is counted, the WARN names the cache.
        logs.allow_errors();
        {
            testing::expect_assert joltAsserts = {};
            fixture.run_steps(10);
            CHECK(joltAsserts.count() == 10 * ASSERTS_PER_VIOLATION);
        }
        CHECK(fixture.physics.stats().updateErrors == 10);
        CHECK(logs.count(core::log_level::WARN) > 0);
        CHECK(logs.contains(core::log_level::WARN, "PhysicsSystem::Update reported"));
        CHECK(logs.contains(core::log_level::WARN, "Full"));
        // The simulation goes on and the world stays consistent.
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("contact_events - a sleeping stack reports EXIT once at sleep and ENTER once at wake") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.timeBeforeSleepSeconds = 0.2f;
        physics_fixture fixture(jobs, settings);
        (void)fixture.spawn_ground();
        const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 0.5f, 0.0f));
        u32 enters = 0;
        u32 exits = 0;
        for (u32 step = 0; step < 120; ++step) {
            fixture.run_step();
            enters += count_kind(fixture.physics.contact_events(), physics::contact_kind::ENTER);
            exits += count_kind(fixture.physics.contact_events(), physics::contact_kind::EXIT);
        }
        CHECK(fixture.settled(box));
        CHECK(enters == 1);
        CHECK(exits == 1);
        CHECK(fixture.physics.contact_events().empty());
        CONJURE_REQUIRE_OK(fixture.physics.wake(box));
        for (u32 step = 0; step < 5; ++step) {
            fixture.run_step();
            enters += count_kind(fixture.physics.contact_events(), physics::contact_kind::ENTER);
        }
        CHECK(enters == 2);
    }
}
