#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/approx.h"
#include "conjure/testing/doctest.h"
#include "physics_test_fixture.h"

#include "conjure/core/math/transform.h"
#include "conjure/ecs/archetype.h"
#include "conjure/physics/components.h"
#include "conjure/scene/components.h"

#include <array>
#include <cstring>
#include <vector>

using namespace conjure;
using namespace physics_test;

namespace {

    /// <summary>
    /// The local_transform column version of the chunk holding 'entity'.
    /// </summary>
    [[nodiscard]] u64 transform_column_version(physics_fixture &fixture, ecs::entity entity) {
        const ecs::component_id id = fixture.registry.id_of<scene::local_transform>();
        for (const ecs::archetype *archetype : fixture.world.archetypes()) {
            if (!archetype->has(id)) {
                continue;
            }
            for (u32 chunk = 0; chunk < archetype->chunk_count(); ++chunk) {
                for (const ecs::entity candidate : archetype->chunk_entities(chunk)) {
                    if (candidate == entity) {
                        return archetype->chunk_column_version(chunk, id);
                    }
                }
            }
        }
        return 0;
    }

    /// <summary>
    /// Chunks of the archetypes the pull visits in parallel: those holding rigid_body, interpolated_transform
    /// and local_transform without a scene::parent (parented rows are written by the serial tail).
    /// </summary>
    [[nodiscard]] u32 parallel_pull_chunk_count(physics_fixture &fixture) {
        const ecs::component_id body = fixture.registry.id_of<physics::rigid_body>();
        const ecs::component_id interpolated = fixture.registry.id_of<scene::interpolated_transform>();
        const ecs::component_id local = fixture.registry.id_of<scene::local_transform>();
        const ecs::component_id parent = fixture.registry.id_of<scene::parent>();
        u32 count = 0;
        for (const ecs::archetype *archetype : fixture.world.archetypes()) {
            if (archetype->has(body) && archetype->has(interpolated) && archetype->has(local)
              && !archetype->has(parent)) {
                count += archetype->chunk_count();
            }
        }
        return count;
    }
}

TEST_SUITE("physics/component_sync") {

    TEST_CASE("component_sync - a dynamic body under a rotated parent writes a consistent local_transform") {
        for_each_job_mode([](core::jobs::job_system &jobs) {
            physics::physics_settings settings = test_settings();
            settings.gravity = math::vec3(0.0f);
            physics_fixture fixture(jobs, settings);
            const ecs::entity parent = fixture.create_at(math::vec3(10.0f, 0.0f, 0.0f));
            scene::local_transform *parentLocal = fixture.world.get<scene::local_transform>(parent);
            REQUIRE(parentLocal != nullptr);
            parentLocal->rotation = glm::angleAxis(1.0f, math::vec3(0.0f, 1.0f, 0.0f));
            const ecs::entity child = fixture.spawn_box(math::vec3(0.0f, 2.0f, 0.0f));
            scene::parent link = {};
            link.value = parent;
            CONJURE_REQUIRE_OK(fixture.world.add<scene::parent>(child, link));
            fixture.run_step();
            CONJURE_REQUIRE_OK(fixture.physics.set_linear_velocity(child, math::vec3(0.0f, 0.0f, 1.0f)));
            fixture.run_steps(30);

            const std::optional<math::transform> pose = fixture.physics.body_pose(child);
            REQUIRE(pose.has_value());
            const scene::local_transform *childLocal = fixture.world.get<scene::local_transform>(child);
            REQUIRE(childLocal != nullptr);
            const math::transform composed = parentLocal->to_transform() * childLocal->to_transform();
            CHECK(testing::approx_equal(composed.position, pose->position, 1.0e-4f));
            CHECK(testing::approx_equal(composed.rotation, pose->rotation, 1.0e-4f));
            CHECK(pose->position.z > 0.4f);
            // The parent's own transform is untouched.
            CHECK(parentLocal->position == math::vec3(10.0f, 0.0f, 0.0f));
        });
    }

    TEST_CASE("component_sync - sleeping bodies are not written") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.timeBeforeSleepSeconds = 0.1f;
        physics_fixture fixture(jobs, settings);
        (void)fixture.spawn_ground();
        std::vector<ecs::entity> boxes = {};
        for (u32 index = 0; index < 8; ++index) {
            boxes.push_back(fixture.spawn_box(math::vec3(static_cast<f32>(index) * 2.0f, 0.5f, 0.0f)));
        }
        fixture.run_steps(240);
        for (const ecs::entity box : boxes) {
            CAPTURE(box);
            CHECK(fixture.settled(box));
        }
        CHECK(fixture.physics.active_body_count() == 0);
        const u64 version = transform_column_version(fixture, boxes[0]);
        fixture.run_steps(10);
        CHECK(transform_column_version(fixture, boxes[0]) == version);
        const physics::rigid_body *pulled = fixture.world.get<physics::rigid_body>(boxes[0]);
        REQUIRE(pulled != nullptr);
        CHECK(pulled->isSleeping);
        // Waking one box writes again.
        CONJURE_REQUIRE_OK(fixture.physics.wake(boxes[0]));
        fixture.run_steps(2);
        CHECK(transform_column_version(fixture, boxes[0]) > version);
    }

    TEST_CASE("component_sync - a teleport through local_transform is detected while the pull's own write is not") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.gravity = math::vec3(0.0f);
        physics_fixture fixture(jobs, settings);
        const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 5.0f, 0.0f));
        fixture.run_step();
        CONJURE_REQUIRE_OK(fixture.physics.set_linear_velocity(box, math::vec3(1.0f, 0.0f, 0.0f)));
        fixture.run_steps(30);
        // The pull wrote local_transform every step; the body kept its velocity (no teleport reset).
        const physics::rigid_body *moving = fixture.world.get<physics::rigid_body>(box);
        REQUIRE(moving != nullptr);
        CHECK(moving->linearVelocity.x == doctest::Approx(1.0f).epsilon(0.05));
        CHECK(fixture.position_of(box).x == doctest::Approx(0.5f).epsilon(0.1));

        scene::local_transform *local = fixture.world.get<scene::local_transform>(box);
        REQUIRE(local != nullptr);
        local->position = math::vec3(-20.0f, 5.0f, 0.0f);
        fixture.run_step();
        // Teleported; the velocity survives a transform teleport (only teleport() resets it).
        CHECK(fixture.position_of(box).x == doctest::Approx(-20.0f + 1.0f * STEP).epsilon(0.05));
        const scene::interpolated_transform *interpolated = fixture.world.get<scene::interpolated_transform>(box);
        REQUIRE(interpolated != nullptr);
        CHECK(interpolated->previous.position.x == doctest::Approx(-20.0f).epsilon(0.01));
    }

    TEST_CASE("component_sync - a scale change rebuilds the shape") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.gravity = math::vec3(0.0f);
        physics_fixture fixture(jobs, settings);
        const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 5.0f, 0.0f));
        fixture.run_step();
        std::array<ecs::entity, 2> out = {};
        CHECK(fixture.physics.overlap_sphere(math::vec3(0.0f, 5.0f, 1.8f), 0.05f, out) == 0);
        scene::local_transform *local = fixture.world.get<scene::local_transform>(box);
        REQUIRE(local != nullptr);
        local->scale = math::vec3(4.0f);
        fixture.run_steps(2);
        CHECK(fixture.physics.overlap_sphere(math::vec3(0.0f, 5.0f, 1.8f), 0.05f, out) == 1);
        const scene::interpolated_transform *interpolated = fixture.world.get<scene::interpolated_transform>(box);
        REQUIRE(interpolated != nullptr);
        CHECK(interpolated->current.scale == math::vec3(4.0f));
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("regression - review 10 parented bodies and characters follow a moved parent") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.gravity = math::vec3(0.0f);
        physics_fixture fixture(jobs, settings);
        const ecs::entity parent = fixture.create_at(math::vec3(0.0f));
        const ecs::entity wall = fixture.spawn_box(math::vec3(0.0f, 0.0f, 0.0f), physics::motion_type::STATIC);
        const ecs::entity crate = fixture.spawn_box(math::vec3(0.0f, 3.0f, 0.0f));
        const ecs::entity character = fixture.spawn_character(math::vec3(0.0f, 6.0f, 0.0f));
        scene::parent link = {};
        link.value = parent;
        CONJURE_REQUIRE_OK(fixture.world.add<scene::parent>(wall, link));
        CONJURE_REQUIRE_OK(fixture.world.add<scene::parent>(crate, link));
        CONJURE_REQUIRE_OK(fixture.world.add<scene::parent>(character, link));
        fixture.run_steps(2);
        CHECK(fixture.position_of(wall).x == doctest::Approx(0.0f));
        CHECK(fixture.position_of(crate).x == doctest::Approx(0.0f));
        CHECK(fixture.position_of(character).x == doctest::Approx(0.0f));

        // Only the parent's own column changes: the children's chunks are untouched, so the push has
        // to compose parented rows itself.
        scene::local_transform *parentLocal = fixture.world.get<scene::local_transform>(parent);
        REQUIRE(parentLocal != nullptr);
        parentLocal->position.x = 10.0f;
        fixture.run_step();
        CHECK(fixture.position_of(wall).x == doctest::Approx(10.0f));
        CHECK(fixture.position_of(crate).x == doctest::Approx(10.0f).epsilon(1.0e-3));
        CHECK(fixture.position_of(character).x == doctest::Approx(10.0f).epsilon(1.0e-3));
        // The children's local poses stay local: still at the parent's origin.
        CHECK(fixture.transform_position(wall).x == doctest::Approx(0.0f));
        CHECK(fixture.transform_position(crate).x == doctest::Approx(0.0f).epsilon(1.0e-3));
        CHECK(fixture.transform_position(character).x == doctest::Approx(0.0f).epsilon(1.0e-3));
        // A dynamic child is not re-posed every step when nothing moved (its velocity is intact).
        CONJURE_REQUIRE_OK(fixture.physics.set_linear_velocity(crate, math::vec3(0.0f, 0.0f, 1.0f)));
        fixture.run_steps(30);
        CHECK(fixture.position_of(crate).z == doctest::Approx(0.5f).epsilon(0.05));
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("component_sync - the scheduled parallel pull writes what the serial fallback writes") {
        // The same scene in two worlds: one stepped through the physics_world calls (the serial
        // pull_transforms() fallback), one through a scheduler with the registered systems (the PARALLEL
        // pull over chunks, one chunk per job). Jolt is deterministic for any worker count, so every
        // component the pull writes must be bit-identical, the serial tail (parented rows) included.
        // The boxes fill several chunks of one archetype whatever the row size (about 60 rows fit a 16 KiB
        // chunk today, so 400 boxes are 7 chunks) and the scheduled pull really fans out; a scene that shrank
        // to one chunk would take the inline branch of dispatch_chunks and the subcase would stop exercising
        // the worker path.
        constexpr u32 BOX_COUNT = 400;
        for_each_job_mode([](core::jobs::job_system &jobs) {
            physics::physics_settings settings = test_settings();
            settings.allowSleeping = false;
            physics_fixture serial(jobs, settings);
            physics_fixture scheduled(jobs, settings);
            scheduled_physics schedule(scheduled, 1);
            std::vector<ecs::entity> serialEntities = {};
            std::vector<ecs::entity> scheduledEntities = {};
            const auto build = [](physics_fixture &fixture, std::vector<ecs::entity> &entities) {
                (void)fixture.spawn_ground();
                for (u32 index = 0; index < BOX_COUNT; ++index) {
                    const f32 x = static_cast<f32>(index % 20) * 1.1f - 11.0f;
                    const f32 y = 0.5f + static_cast<f32>(index / 20) * 1.1f;
                    entities.push_back(fixture.spawn_box(math::vec3(x, y, 0.0f)));
                }
                const ecs::entity parent = fixture.create_at(math::vec3(20.0f, 0.0f, 0.0f));
                const ecs::entity child = fixture.spawn_box(math::vec3(0.0f, 3.0f, 0.0f));
                scene::parent link = {};
                link.value = parent;
                CONJURE_REQUIRE_OK(fixture.world.add<scene::parent>(child, link));
                entities.push_back(child);
            };
            build(serial, serialEntities);
            build(scheduled, scheduledEntities);
            REQUIRE(serialEntities.size() == scheduledEntities.size());

            // The first pull of the scheduled path can lag one step (interpolated_transform arrives through
            // the stage's command buffer), so the poses are compared once both previous and current are
            // steps of the same simulation.
            serial.run_steps(40);
            schedule.run_steps(40);
            REQUIRE(serial.physics.body_count() == scheduled.physics.body_count());
            const u32 chunkCount = parallel_pull_chunk_count(scheduled);
            CAPTURE(chunkCount);
            CHECK(chunkCount > 1);

            u32 moved = 0;
            for (usize index = 0; index < serialEntities.size(); ++index) {
                CAPTURE(index);
                const ecs::entity a = serialEntities[index];
                const ecs::entity b = scheduledEntities[index];
                const scene::local_transform *localA = serial.world.get<scene::local_transform>(a);
                const scene::local_transform *localB = scheduled.world.get<scene::local_transform>(b);
                const scene::interpolated_transform *lerpA = serial.world.get<scene::interpolated_transform>(a);
                const scene::interpolated_transform *lerpB = scheduled.world.get<scene::interpolated_transform>(b);
                const physics::rigid_body *bodyA = serial.world.get<physics::rigid_body>(a);
                const physics::rigid_body *bodyB = scheduled.world.get<physics::rigid_body>(b);
                REQUIRE(localA != nullptr);
                REQUIRE(localB != nullptr);
                REQUIRE(lerpA != nullptr);
                REQUIRE(lerpB != nullptr);
                REQUIRE(bodyA != nullptr);
                REQUIRE(bodyB != nullptr);
                // The transforms are padding-free floats; rigid_body carries padding, so its own ==.
                CHECK(std::memcmp(localA, localB, sizeof(scene::local_transform)) == 0);
                CHECK(std::memcmp(lerpA, lerpB, sizeof(scene::interpolated_transform)) == 0);
                CHECK(*bodyA == *bodyB);
                if (lerpA->current.position != lerpA->previous.position) {
                    ++moved;
                }
            }
            // The scene is still settling after 40 steps: the comparison covered written rows, not idle ones.
            CHECK(moved > 0);
        });
    }

    TEST_CASE("component_sync - the scheduled pull over 2000 bodies allocates nothing after warm-up, 0 and 4 workers") {
        // The design's allocation gate (08_testing.md 7.1, physics.sync_10k: 0) on the path the engine
        // takes, at a body count that keeps the default run short: 2,000 bodies fill some 35 chunks, enough to
        // prove the chunk pass and its fan-out allocation-free. The 10,000-body figure is the [slow] case
        // of slow/test_component_sync_10k.cpp and the benchmark physics.sync_10k.
        for_each_job_mode([](core::jobs::job_system &jobs) {
            check_pull_allocation_free(jobs, 2000);
        });
    }

    TEST_CASE("component_sync - pull over 2000 sleeping bodies allocates nothing [bench]") {
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
            CHECK(fixture.physics.active_body_count() == 0);
            testing::allocation_scope scope = {};
            fixture.physics.pull_transforms();
            const u64 allocations = scope.allocation_count();
            CHECK(allocations == 0);
            CHECK(fixture.physics.stats().pullMicroseconds < 50000);
        });
    }
}
