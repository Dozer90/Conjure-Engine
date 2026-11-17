#include "conjure/testing/approx.h"
#include "conjure/testing/assert_capture.h"
#include "conjure/testing/doctest.h"
#include "conjure/testing/log_capture.h"
#include "physics_test_fixture.h"

#include "conjure/core/math/transform.h"
#include "conjure/physics/components.h"
#include "conjure/physics/contact_events.h"
#include "conjure/scene/components.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <optional>
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

    /// <summary>
    /// Settings under which a resting body falls asleep within a quarter of a second.
    /// </summary>
    [[nodiscard]] physics::physics_settings drowsy_settings() {
        physics::physics_settings settings = test_settings();
        settings.timeBeforeSleepSeconds = 0.2f;
        return settings;
    }

    /// <summary>
    /// A unit box created asleep, by default resting on the ground at the origin.
    /// </summary>
    [[nodiscard]] ecs::entity spawn_sleeping_box(
      physics_fixture &fixture,
      const math::vec3 &position = math::vec3(0.0f, 0.5f, 0.0f)
    ) {
        const ecs::entity box = fixture.create_at(position);
        physics::rigid_body body = {};
        body.flags = physics::rigid_body_flags::ALLOW_SLEEPING | physics::rigid_body_flags::START_ASLEEP;
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(box, body));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(box));
        return box;
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
        // A static trigger never reports static geometry; a kinematic one does (mCollideKinematicVsNonDynamic),
        // so the sweeping plate reports the wall it passes through.
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
        physics_fixture fixture(jobs, drowsy_settings());
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

    TEST_CASE("contact_events - a STAY after one sub-shape contact of a compound ends carries a live point") {
        // A floor and a wall in one static compound and a box in the corner: the two sub-shape contacts have
        // different normals, so Jolt keeps them as two manifolds instead of reducing them into one.
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs, awake_settings());
        const ecs::entity corner = fixture.create_at(math::vec3(0.0f));
        physics::box_collider floor = {};
        floor.halfExtents = math::vec3(5.0f, 0.5f, 5.0f);
        floor.offset = math::vec3(0.0f, -0.5f, 0.0f);
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(corner, floor));
        physics::capsule_collider wall = {};
        wall.radius = 0.5f;
        wall.halfHeight = 2.0f;
        wall.offset = math::vec3(1.0f, 2.5f, 0.0f);
        CONJURE_REQUIRE_OK(fixture.world.add<physics::capsule_collider>(corner, wall));
        const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 0.5f, 0.0f));
        fixture.run_steps(30);

        // The box leaves one sub-shape while it keeps touching the other, so one step reports that sub-shape pair
        // REMOVED and the other PERSISTED; the two subcases put the REMOVED record first and last in the run.
        math::vec3 push = math::vec3(0.0f);
        SUBCASE("the box slides away from the wall") {
            push = math::vec3(-2.0f, 0.0f, 0.0f);
        }
        SUBCASE("the box rises along the wall") {
            push = math::vec3(0.0f, 3.0f, 0.0f);
        }
        CONJURE_REQUIRE_OK(fixture.physics.add_impulse(box, push));
        u32 stays = 0;
        for (u32 step = 0; step < 10; ++step) {
            fixture.run_step();
            for (const physics::contact_event &event : fixture.physics.contact_events()) {
                if (event.kind != physics::contact_kind::STAY || !involves(event, corner, box)) {
                    continue;
                }
                CAPTURE(step);
                ++stays;
                CHECK(event.contactCount > 0);
                CHECK(event.normal != math::vec3(0.0f));
            }
        }
        CHECK(stays > 0);
    }

    TEST_CASE("regression - code sweep F408 a body that falls asleep inside a static trigger stays inside") {
        // The trigger was a static sensor, which loses a body the moment it falls asleep: TRIGGER_EXIT for a
        // crate still standing on the pressure plate.
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs, drowsy_settings());
        (void)fixture.spawn_ground();
        bool withRigidBody = true;
        bool startAsleep = false;
        SUBCASE("a trigger with a static rigid_body") {}
        SUBCASE("a trigger with a static rigid_body that carries START_ASLEEP") {
            // The flag means nothing on a static body: the trigger is added active all the same, and only an
            // active sensor keeps a body that falls asleep.
            startAsleep = true;
        }
        SUBCASE("a trigger made of a collider alone") {
            withRigidBody = false;
        }
        // The volume stands on the ground, which it must not report, and holds the resting box.
        trigger_tally tally = {};
        tally.trigger = fixture.spawn_static_trigger(
          math::vec3(0.0f, 1.0f, 0.0f),
          math::vec3(2.0f, 1.0f, 2.0f),
          withRigidBody
        );
        if (startAsleep) {
            physics::rigid_body *body = fixture.world.get<physics::rigid_body>(tally.trigger);
            REQUIRE(body != nullptr);
            body->flags = physics::rigid_body_flags::ALLOW_SLEEPING | physics::rigid_body_flags::START_ASLEEP;
        }
        tally.other = fixture.spawn_box(math::vec3(0.0f, 0.5f, 0.0f));
        tally.run(fixture, 120);
        CHECK(fixture.settled(tally.other));
        CHECK(tally.enters == 1);
        CHECK(tally.exits == 0);

        // Awake again it is still the same visit, and the sensor did not wake it by itself before.
        CONJURE_REQUIRE_OK(fixture.physics.wake(tally.other));
        tally.run(fixture, 5);
        CHECK_FALSE(fixture.settled(tally.other));
        CHECK(tally.enters == 1);
        CHECK(tally.exits == 0);

        // It leaves once, when it really leaves.
        math::transform outside = {};
        outside.position = math::vec3(30.0f, 0.5f, 0.0f);
        CONJURE_REQUIRE_OK(fixture.physics.teleport(tally.other, outside));
        tally.run(fixture, 5);
        CHECK(tally.enters == 1);
        CHECK(tally.exits == 1);
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("regression - code sweep F408 a static trigger reports a sleeping body it meets") {
        // A static sensor saw awake bodies only: a volume moved over a sleeping body, or a body created asleep
        // inside one, was never reported.
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs, drowsy_settings());
        (void)fixture.spawn_ground();
        const math::vec3 overTheBox = math::vec3(0.0f, 1.0f, 0.0f);
        const math::vec3 away = math::vec3(30.0f, 1.0f, 0.0f);
        const math::vec3 halfExtents = math::vec3(2.0f, 1.0f, 2.0f);
        trigger_tally tally = {};
        SUBCASE("the body is created asleep inside the trigger") {
            tally.trigger = fixture.spawn_static_trigger(overTheBox, halfExtents);
            tally.other = spawn_sleeping_box(fixture);
        }
        SUBCASE("the trigger is moved over the body through its local_transform") {
            tally.trigger = fixture.spawn_static_trigger(away, halfExtents);
            tally.other = spawn_sleeping_box(fixture);
            tally.run(fixture, 5);
            REQUIRE(tally.enters == 0);
            scene::local_transform *local = fixture.world.get<scene::local_transform>(tally.trigger);
            REQUIRE(local != nullptr);
            local->position = overTheBox;
        }
        SUBCASE("the trigger is teleported over the body") {
            tally.trigger = fixture.spawn_static_trigger(away, halfExtents);
            tally.other = spawn_sleeping_box(fixture);
            tally.run(fixture, 5);
            REQUIRE(tally.enters == 0);
            math::transform pose = {};
            pose.position = overTheBox;
            CONJURE_REQUIRE_OK(fixture.physics.teleport(tally.trigger, pose));
        }
        tally.run(fixture, 30);
        CHECK(tally.enters == 1);
        CHECK(tally.exits == 0);
        // Seeing it did not wake it.
        CHECK(fixture.settled(tally.other));
        CHECK(fixture.physics.active_body_count() == 0);

        // The volume goes away again: the body left it, asleep as it is.
        math::transform pose = {};
        pose.position = away;
        CONJURE_REQUIRE_OK(fixture.physics.teleport(tally.trigger, pose));
        tally.run(fixture, 5);
        CHECK(tally.enters == 1);
        CHECK(tally.exits == 1);
        CHECK(fixture.settled(tally.other));
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("contact_events - a static trigger reports neither static geometry nor another static trigger") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        (void)fixture.spawn_ground();
        // Three static triggers through one another, through the ground and through a static wall.
        const math::vec3 halfExtents = math::vec3(2.0f, 1.0f, 2.0f);
        const ecs::entity first = fixture.spawn_static_trigger(math::vec3(0.0f, 0.5f, 0.0f), halfExtents);
        const ecs::entity second = fixture.spawn_static_trigger(math::vec3(1.0f, 0.5f, 0.0f), halfExtents);
        const ecs::entity third = fixture.spawn_static_trigger(math::vec3(0.0f, 0.5f, 1.0f), halfExtents, false);
        (void)fixture.spawn_box(math::vec3(0.0f, 1.0f, 0.0f), physics::motion_type::STATIC);
        for (u32 step = 0; step < 10; ++step) {
            fixture.run_step();
            CAPTURE(step);
            CHECK(fixture.physics.contact_events().empty());
        }
        CHECK(fixture.physics.body_count() == 5);
        // Jolt keeps the three active; to the engine they are static bodies: never awake, never counted.
        CHECK(fixture.physics.active_body_count() == 0);
        CHECK(fixture.physics.stats().activeBodies == 0);
        for (const ecs::entity trigger : {first, second, third}) {
            CAPTURE(trigger.index);
            CHECK(fixture.physics.is_sleeping(trigger));
            // wake and sleep stay the no-ops they are on every static body.
            CONJURE_CHECK_OK(fixture.physics.sleep(trigger));
            CONJURE_CHECK_OK(fixture.physics.wake(trigger));
        }
        fixture.run_step();
        CHECK(fixture.physics.stats().droppedCommands == 0);
        CHECK(fixture.physics.active_body_count() == 0);
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("regression - code sweep F408 bodies inside a static trigger leave the contact budget to other pairs") {
        // Jolt reports a pair with a static trigger as persisted in every step, asleep or not. Recorded, those
        // reports filled maxContactEventsPerStep for as long as the bodies stayed inside, and the records that
        // make another pair's ENTER or EXIT were dropped.
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = drowsy_settings();
        settings.maxContactEventsPerStep = 12;
        physics_fixture fixture(jobs, settings);
        const ecs::entity ground = fixture.spawn_ground();
        // Two trigger colliders over the same six boxes: two sub-shape contacts per box, twelve records in the
        // step they enter, which is the whole budget.
        const ecs::entity zone =
          fixture.spawn_static_trigger(math::vec3(0.0f, 1.0f, 0.0f), math::vec3(20.0f, 1.0f, 2.0f));
        physics::sphere_collider second = {};
        second.radius = 20.0f;
        second.isTrigger = true;
        CONJURE_REQUIRE_OK(fixture.world.add<physics::sphere_collider>(zone, second));
        std::array<ecs::entity, 6> boxes = {};
        for (u32 index = 0; index < boxes.size(); ++index) {
            const f32 x = static_cast<f32>(index) * 2.0f - 5.0f;
            boxes[index] = spawn_sleeping_box(fixture, math::vec3(x, 0.5f, 0.0f));
        }
        testing::log_capture logs = {};
        trigger_tally inside = {};
        inside.run(fixture, 1);
        CHECK(inside.enters == boxes.size());
        inside.run(fixture, 10);
        CHECK(inside.enters == boxes.size());
        CHECK(inside.exits == 0);
        CHECK(fixture.physics.stats().droppedContactEvents == 0);

        // A solid pair far from the trigger still gets its ENTER, and nothing is dropped to make room for it.
        const ecs::entity crate = fixture.spawn_box(math::vec3(40.0f, 0.55f, 0.0f));
        u32 enters = 0;
        for (u32 step = 0; step < 20; ++step) {
            fixture.run_step();
            for (const physics::contact_event &event : fixture.physics.contact_events()) {
                CAPTURE(step);
                CHECK(involves(event, ground, crate));
                enters += event.kind == physics::contact_kind::ENTER ? 1u : 0u;
            }
        }
        CHECK(enters == 1);
        CHECK(fixture.physics.stats().droppedContactEvents == 0);
        CHECK(logs.count(core::log_level::WARN) == 0);
        for (u32 index = 0; index < boxes.size(); ++index) {
            CAPTURE(index);
            CHECK(fixture.settled(boxes[index]));
        }
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("regression - code sweep F408 a trigger that becomes kinematic reports static geometry it overlaps") {
        // Whether a kinematic sensor meets bodies that do not move is a flag of the Jolt body. It was set where
        // the body is created and where its shape is rebuilt, so a trigger that became kinematic by a motion
        // type written into its rigid_body went on without it.
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = awake_settings();
        settings.gravity = math::vec3(0.0f);
        physics_fixture fixture(jobs, settings);
        trigger_tally tally = {};
        tally.other = fixture.spawn_box(math::vec3(0.0f), physics::motion_type::STATIC);
        tally.trigger = fixture.create_at(math::vec3(0.0f));
        physics::rigid_body body = {};
        physics::box_collider volume = {};
        volume.halfExtents = math::vec3(2.0f);
        volume.isTrigger = true;
        const auto become = [&](physics::motion_type motion) {
            physics::rigid_body *authored = fixture.world.get<physics::rigid_body>(tally.trigger);
            REQUIRE(authored != nullptr);
            authored->motionType = motion;
            tally.run(fixture, 5);
        };

        SUBCASE("a dynamic trigger that becomes kinematic keeps the wall it is in") {
            body.motionType = physics::motion_type::DYNAMIC;
            CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(tally.trigger, body));
            CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(tally.trigger, volume));
            tally.run(fixture, 5);
            REQUIRE(tally.enters == 1);
            become(physics::motion_type::KINEMATIC);
            CHECK(tally.enters == 1);
            CHECK(tally.exits == 0);
            become(physics::motion_type::DYNAMIC);
            CHECK(tally.enters == 1);
            CHECK(tally.exits == 0);
        }
        SUBCASE("a static trigger that becomes kinematic enters the wall it is in and leaves it as a static one") {
            body.motionType = physics::motion_type::STATIC;
            CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(tally.trigger, body));
            CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(tally.trigger, volume));
            tally.run(fixture, 5);
            REQUIRE(tally.enters == 0);
            become(physics::motion_type::KINEMATIC);
            CHECK(tally.enters == 1);
            CHECK(tally.exits == 0);
            become(physics::motion_type::STATIC);
            CHECK(tally.enters == 1);
            CHECK(tally.exits == 1);
            become(physics::motion_type::KINEMATIC);
            CHECK(tally.enters == 2);
            CHECK(tally.exits == 1);
        }
        CHECK(fixture.position_of(tally.trigger) == math::vec3(0.0f));
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("contact_events - a static body that turns into a trigger sees the sleeping body inside it") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs, drowsy_settings());
        (void)fixture.spawn_ground();
        // A static body with one solid sphere, 20 m to the side of a box that sleeps at the origin.
        const ecs::entity zone = fixture.create_at(math::vec3(0.0f, 1.0f, 0.0f));
        SUBCASE("a body with a static rigid_body is converted in place") {
            physics::rigid_body body = {};
            body.motionType = physics::motion_type::STATIC;
            CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(zone, body));
        }
        SUBCASE("a body made of colliders alone is replaced") {}
        physics::sphere_collider solid = {};
        solid.offset = math::vec3(20.0f, 0.0f, 0.0f);
        CONJURE_REQUIRE_OK(fixture.world.add<physics::sphere_collider>(zone, solid));
        trigger_tally tally = {};
        tally.trigger = zone;
        tally.other = spawn_sleeping_box(fixture);
        tally.run(fixture, 5);
        REQUIRE(fixture.physics.has_body(zone));
        REQUIRE(tally.enters == 0);

        // Both changes reach the same push, so the body goes from solid to trigger in one rebuild.
        physics::box_collider volume = {};
        volume.halfExtents = math::vec3(2.0f, 1.0f, 2.0f);
        volume.isTrigger = true;
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(zone, volume));
        CONJURE_REQUIRE_OK(fixture.world.remove<physics::sphere_collider>(zone));
        tally.run(fixture, 30);
        CHECK(fixture.physics.has_body(zone));
        CHECK(fixture.physics.body_count() == 3);
        CHECK(tally.enters == 1);
        CHECK(tally.exits == 0);
        CHECK(fixture.settled(tally.other));
        CHECK(fixture.physics.active_body_count() == 0);
        CONJURE_CHECK_OK(fixture.physics.validate());

        // And back to solid, away from the box: the trigger is gone, so the box left it.
        CONJURE_REQUIRE_OK(fixture.world.add<physics::sphere_collider>(zone, solid));
        CONJURE_REQUIRE_OK(fixture.world.remove<physics::box_collider>(zone));
        tally.run(fixture, 5);
        CHECK(tally.enters == 1);
        CHECK(tally.exits == 1);
        CHECK(fixture.settled(tally.other));
        CHECK(fixture.physics.active_body_count() == 0);
        std::array<ecs::entity, 2> out = {};
        CHECK(fixture.physics.overlap_sphere(math::vec3(20.0f, 1.0f, 0.0f), 0.1f, out) == 1);
        CHECK(out[0] == zone);
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("contact_events - a static trigger changes its motion type without losing the body inside it") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = drowsy_settings();
        settings.gravity = math::vec3(0.0f);
        physics_fixture fixture(jobs, settings);
        trigger_tally tally = {};
        tally.trigger = fixture.spawn_static_trigger(math::vec3(0.0f), math::vec3(2.0f));
        // No gravity and no ground: the box hangs inside the volume and falls asleep there.
        tally.other = fixture.spawn_box(math::vec3(0.0f));
        tally.run(fixture, 60);
        REQUIRE(fixture.settled(tally.other));
        REQUIRE(tally.enters == 1);

        // Static to kinematic and back: an awake sensor either way, so the sleeping box stays inside.
        for (const physics::motion_type motion : {physics::motion_type::KINEMATIC, physics::motion_type::STATIC}) {
            CAPTURE(static_cast<u32>(motion));
            physics::rigid_body *body = fixture.world.get<physics::rigid_body>(tally.trigger);
            REQUIRE(body != nullptr);
            body->motionType = motion;
            tally.run(fixture, 30);
            CHECK(tally.enters == 1);
            CHECK(tally.exits == 0);
            CHECK(fixture.settled(tally.other));
            CHECK(fixture.position_of(tally.trigger) == math::vec3(0.0f));
            CONJURE_CHECK_OK(fixture.physics.validate());
        }
        // Only the static one is left out of the count of active bodies.
        CHECK(fixture.physics.active_body_count() == 0);
    }

    TEST_CASE("contact_events - a static trigger does not move by a velocity") {
        // To Jolt the static trigger is a kinematic body, which a velocity would move.
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = awake_settings();
        settings.gravity = math::vec3(0.0f);
        physics_fixture fixture(jobs, settings);
        const math::vec3 linear = math::vec3(5.0f, 0.0f, 0.0f);
        const math::vec3 angular = math::vec3(0.0f, 3.0f, 0.0f);
        const ecs::entity trigger = fixture.create_at(math::vec3(0.0f));
        physics::rigid_body body = {};
        body.linearVelocity = linear;
        body.angularVelocity = angular;
        physics::box_collider volume = {};
        volume.isTrigger = true;
        SUBCASE("authored in the rigid_body it is created with") {
            body.motionType = physics::motion_type::STATIC;
            CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(trigger, body));
            CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(trigger, volume));
            fixture.run_step();
        }
        SUBCASE("left over from the dynamic trigger it was") {
            CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(trigger, body));
            CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(trigger, volume));
            fixture.run_steps(10);
            REQUIRE(fixture.position_of(trigger).x > 0.5f);
            physics::rigid_body *moving = fixture.world.get<physics::rigid_body>(trigger);
            REQUIRE(moving != nullptr);
            moving->motionType = physics::motion_type::STATIC;
            fixture.run_step();
        }
        SUBCASE("left over from the static solid it was") {
            // A static body keeps the velocities it was created with; they must not come to life when a
            // collider change makes it a trigger.
            body.motionType = physics::motion_type::STATIC;
            CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(trigger, body));
            CONJURE_REQUIRE_OK(fixture.world.add<physics::sphere_collider>(trigger));
            fixture.run_step();
            CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(trigger, volume));
            CONJURE_REQUIRE_OK(fixture.world.remove<physics::sphere_collider>(trigger));
            fixture.run_step();
        }
        const std::optional<math::transform> before = fixture.physics.body_pose(trigger);
        REQUIRE(before.has_value());
        fixture.run_steps(30);
        const std::optional<math::transform> after = fixture.physics.body_pose(trigger);
        REQUIRE(after.has_value());
        CHECK(after->position == before->position);
        CHECK(testing::approx_equal(after->rotation, before->rotation, 1.0e-6f));
        CHECK(fixture.physics.is_sleeping(trigger));
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("contact_events - a character walking through a static trigger enters it once and leaves it once") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        (void)fixture.spawn_ground();
        trigger_tally tally = {};
        tally.trigger = fixture.spawn_static_trigger(math::vec3(3.0f, 1.0f, 0.0f), math::vec3(1.0f, 1.0f, 2.0f));
        tally.other = fixture.spawn_character(math::vec3(0.0f));
        // A second and a half before it sets off, three metres in a second and a half, then it stands inside.
        tally.run(fixture, 90);
        CHECK(tally.enters == 0);
        const auto walk = [&](f32 speed, u32 steps) {
            for (u32 step = 0; step < steps; ++step) {
                CAPTURE(step);
                CONJURE_REQUIRE_OK(fixture.physics.character_move(tally.other, math::vec3(speed, 0.0f, 0.0f)));
                tally.run(fixture, 1);
            }
        };
        walk(2.0f, 90);
        walk(0.0f, 120);
        CHECK(fixture.position_of(tally.other).x == doctest::Approx(3.0f).epsilon(0.05));
        CHECK(tally.enters == 1);
        CHECK(tally.exits == 0);
        walk(2.0f, 90);
        CHECK(tally.enters == 1);
        CHECK(tally.exits == 1);
    }
}
