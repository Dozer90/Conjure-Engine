#include "conjure/testing/allocation_scope.h"
#include "conjure/testing/approx.h"
#include "conjure/testing/doctest.h"
#include "physics_test_fixture.h"

#include "conjure/core/math/transform.h"
#include "conjure/ecs/archetype.h"
#include "conjure/physics/components.h"
#include "conjure/scene/components.h"

#include <array>
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
