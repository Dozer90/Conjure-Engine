#include "conjure/testing/doctest.h"
#include "conjure/testing/log_capture.h"
#include "physics_test_fixture.h"

#include "conjure/core/math/math_utils.h"
#include "conjure/core/math/ray.h"
#include "conjure/physics/components.h"
#include "conjure/physics/queries.h"
#include "conjure/scene/components.h"

#include <array>
#include <cmath>
#include <optional>

using namespace conjure;
using namespace physics_test;

TEST_SUITE("physics/colliders") {

    TEST_CASE("colliders - removing the rigid_body of a static floor keeps a body under the box in the same push") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.allowSleeping = false;
        physics_fixture fixture(jobs, settings);
        const ecs::entity ground = fixture.spawn_ground();
        const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 0.5f, 0.0f));
        fixture.run_steps(30);

        // The floor's body is replaced by an implicit static one; the step after this push must have it.
        CONJURE_REQUIRE_OK(fixture.world.remove<physics::rigid_body>(ground));
        fixture.physics.push_transforms(STEP);
        CHECK(fixture.physics.has_body(ground));
        fixture.physics.step(STEP);
        fixture.physics.pull_transforms();
        fixture.physics.collect_contact_events();
        CHECK(fixture.position_of(box).y == doctest::Approx(0.5f).epsilon(0.02));
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("colliders - an implicit static body given a dynamic rigid_body keeps its interpolated_transform") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const ecs::entity crate = fixture.create_at(math::vec3(0.0f, 5.0f, 0.0f));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(crate));
        fixture.run_step();
        REQUIRE(fixture.physics.has_body(crate));
        // Added by the owner to the static body; the dynamic body that replaces it in the next push keeps it.
        CONJURE_REQUIRE_OK(fixture.world.add<scene::interpolated_transform>(crate));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(crate));
        fixture.physics.push_transforms(STEP);
        CHECK(fixture.physics.has_body(crate));
        CHECK(fixture.world.has<scene::interpolated_transform>(crate));
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("colliders - an implicit static body whose static rigid_body turns dynamic has a body in the same push") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const ecs::entity crate = fixture.create_at(math::vec3(0.0f, 5.0f, 0.0f));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(crate));
        fixture.run_step();
        // A STATIC rigid_body does not change the motion, so the row stays the implicit static one.
        physics::rigid_body authored = {};
        authored.motionType = physics::motion_type::STATIC;
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(crate, authored));
        fixture.run_step();
        REQUIRE(fixture.physics.has_body(crate));

        // The change detection replaces the body after the push's first flush of pending creations.
        physics::rigid_body *body = fixture.world.get<physics::rigid_body>(crate);
        REQUIRE(body != nullptr);
        body->motionType = physics::motion_type::DYNAMIC;
        fixture.physics.push_transforms(STEP);
        CHECK(fixture.physics.has_body(crate));
        CHECK(fixture.world.has<scene::interpolated_transform>(crate));
        fixture.physics.step(STEP);
        fixture.physics.pull_transforms();
        fixture.physics.collect_contact_events();
        CHECK(fixture.position_of(crate).y < 5.0f);
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("colliders - each primitive rests on the ground within 1 cm of its analytic height") {
        for_each_job_mode([](core::jobs::job_system &jobs) {
            physics_fixture fixture(jobs);
            (void)fixture.spawn_ground();
            const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 1.0f, 0.0f));
            const ecs::entity sphere = fixture.spawn_sphere(math::vec3(3.0f, 1.0f, 0.0f), 0.25f);
            const ecs::entity capsule = fixture.create_at(math::vec3(6.0f, 1.5f, 0.0f));
            CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(capsule));
            physics::capsule_collider capsuleShape = {};
            capsuleShape.radius = 0.3f;
            capsuleShape.halfHeight = 0.4f;
            CONJURE_REQUIRE_OK(fixture.world.add<physics::capsule_collider>(capsule, capsuleShape));
            u32 eventsSeen = 0;
            for (u32 step = 0; step < 120; ++step) {
                fixture.run_step();
                eventsSeen += static_cast<u32>(fixture.physics.contact_events().size());
            }
            CHECK(std::abs(fixture.position_of(box).y - 0.5f) < 0.01f);
            CHECK(std::abs(fixture.position_of(sphere).y - 0.25f) < 0.01f);
            CHECK(std::abs(fixture.position_of(capsule).y - 0.7f) < 0.01f);
            CHECK(eventsSeen > 0);
        });
    }

    TEST_CASE("colliders - an offset compound of box and sphere places its centre of mass between them") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.gravity = math::vec3(0.0f);
        physics_fixture fixture(jobs, settings);
        const ecs::entity compound = fixture.create_at(math::vec3(0.0f, 5.0f, 0.0f));
        physics::rigid_body body = {};
        body.mass = 0.0f;   // derived from the shapes so the centre of mass is meaningful
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(compound, body));
        physics::box_collider box = {};
        box.offset = math::vec3(-1.0f, 0.0f, 0.0f);
        box.rotation = glm::angleAxis(0.3f, math::vec3(0.0f, 0.0f, 1.0f));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(compound, box));
        physics::sphere_collider sphere = {};
        sphere.offset = math::vec3(1.0f, 0.0f, 0.0f);
        CONJURE_REQUIRE_OK(fixture.world.add<physics::sphere_collider>(compound, sphere));
        fixture.run_step();
        REQUIRE(fixture.physics.has_body(compound));
        // The body origin stays at the entity origin while the sub-shapes sit left and right of it:
        // a sphere overlap at each offset finds the body, one at the origin between them does not.
        std::array<ecs::entity, 4> out = {};
        CHECK(fixture.physics.overlap_sphere(math::vec3(-1.0f, 5.0f, 0.0f), 0.1f, out) == 1);
        CHECK(out[0] == compound);
        CHECK(fixture.physics.overlap_sphere(math::vec3(1.0f, 5.0f, 0.0f), 0.1f, out) == 1);
        CHECK(fixture.physics.overlap_sphere(math::vec3(0.0f, 5.0f, 0.0f), 0.1f, out) == 0);
        // A torque around y spins the compound about its centre of mass, which stays near the origin.
        CONJURE_REQUIRE_OK(fixture.physics.add_angular_impulse(compound, math::vec3(0.0f, 1.0f, 0.0f)));
        fixture.run_steps(30);
        CHECK(fixture.position_of(compound).x == doctest::Approx(0.0f).epsilon(0.05));
        CHECK(fixture.physics.stats().shapeCacheEntries == 2);
    }

    TEST_CASE("regression - code sweep F213 a single collider turned by a tenth of a degree keeps its rotation") {
        // A lone collider without offset or rotation is used as the body's shape directly; any other
        // placement needs a compound. The identity test was a cosine tolerance of 1e-6, which took every
        // rotation below 0.16 degrees for none and dropped it. Over the 10 m half length of this beam a tenth
        // of a degree is 1.7 cm.
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const f32 tilt = math::radians(0.1f);
        const ecs::entity beam = fixture.create_at(math::vec3(0.0f));
        physics::box_collider collider = {};
        collider.halfExtents = math::vec3(10.0f, 0.5f, 0.5f);
        collider.rotation = glm::angleAxis(tilt, math::vec3(0.0f, 0.0f, 1.0f));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(beam, collider));
        fixture.run_step();
        REQUIRE(fixture.physics.has_body(beam));

        // The top face rises along +x: at x it is 0.5 / cos(tilt) + x * tan(tilt) high.
        const std::array<f32, 2> stations = {9.0f, -9.0f};
        for (const f32 station : stations) {
            CAPTURE(station);
            math::ray ray = {};
            ray.origin = math::vec3(station, 5.0f, 0.0f);
            ray.direction = math::vec3(0.0f, -1.0f, 0.0f);
            const std::optional<physics::raycast_hit> hit = fixture.physics.raycast(ray, 20.0f);
            REQUIRE(hit.has_value());
            CHECK(hit->entity == beam);
            const f32 expected = 0.5f / std::cos(tilt) + station * std::tan(tilt);
            CHECK(std::abs(hit->point.y - expected) < 1.0e-3f);
            CHECK(std::abs(hit->point.y - 0.5f) > 1.0e-2f);
        }
        // A collider whose rotation is the identity (stored as -q here, the same rotation) lies level, and
        // both beams share the one cached box.
        const ecs::entity level = fixture.create_at(math::vec3(0.0f, 0.0f, 10.0f));
        physics::box_collider plain = {};
        plain.halfExtents = math::vec3(10.0f, 0.5f, 0.5f);
        plain.rotation = math::quat(-1.0f, 0.0f, 0.0f, 0.0f);   // the identity rotation, stored as -q
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(level, plain));
        fixture.run_step();
        math::ray ray = {};
        ray.origin = math::vec3(9.0f, 5.0f, 10.0f);
        ray.direction = math::vec3(0.0f, -1.0f, 0.0f);
        const std::optional<physics::raycast_hit> hit = fixture.physics.raycast(ray, 20.0f);
        REQUIRE(hit.has_value());
        CHECK(hit->entity == level);
        CHECK(std::abs(hit->point.y - 0.5f) < 1.0e-3f);
        CHECK(fixture.physics.stats().shapeCacheEntries == 1);
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("colliders - mixed trigger flags warn and build a non-trigger body") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        (void)fixture.spawn_ground();
        const ecs::entity mixed = fixture.create_at(math::vec3(0.0f, 2.0f, 0.0f));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(mixed));
        physics::box_collider box = {};
        box.isTrigger = true;
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(mixed, box));
        physics::sphere_collider sphere = {};
        sphere.offset = math::vec3(0.0f, 0.5f, 0.0f);
        CONJURE_REQUIRE_OK(fixture.world.add<physics::sphere_collider>(mixed, sphere));
        testing::log_capture logs = {};
        fixture.run_steps(90);
        CHECK(logs.contains(core::log_level::WARN, "MIXED_TRIGGER_FLAGS"));
        // Not a trigger: the body rests on the ground instead of falling through, and queries see it
        // without includeTriggers.
        CHECK(fixture.position_of(mixed).y > 0.4f);
        std::array<ecs::entity, 2> out = {};
        CHECK(fixture.physics.overlap_sphere(fixture.position_of(mixed), 0.1f, out) == 1);
        bool sawTrigger = false;
        for (const physics::contact_event &event : fixture.physics.contact_events()) {
            sawTrigger = sawTrigger || event.kind == physics::contact_kind::TRIGGER_ENTER;
        }
        CHECK_FALSE(sawTrigger);
    }

    TEST_CASE("colliders - a mesh_collider on a dynamic body warns and is ignored") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const ecs::entity dynamicMesh = fixture.create_at(math::vec3(0.0f, 2.0f, 0.0f));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(dynamicMesh));
        physics::mesh_collider mesh = {};
        mesh.mesh = assets::asset_ref<physics::collision_mesh_asset>(core::uuid(1, 2));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::mesh_collider>(dynamicMesh, mesh));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::sphere_collider>(dynamicMesh));
        testing::log_capture logs = {};
        fixture.run_step();
        CHECK(logs.contains(core::log_level::WARN, "UNSUPPORTED_SHAPE_FOR_MOTION"));
        CHECK(fixture.physics.has_body(dynamicMesh));
        // Only the sphere was used: the shape cache holds one sphere leaf.
        CHECK(fixture.physics.stats().shapeCacheEntries == 1);

        // Without any usable collider the body stays pending.
        const ecs::entity meshOnly = fixture.create_at(math::vec3(5.0f, 2.0f, 0.0f));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(meshOnly));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::mesh_collider>(meshOnly, mesh));
        fixture.run_step();
        CHECK_FALSE(fixture.physics.has_body(meshOnly));
        CHECK(fixture.physics.stats().bodiesWithoutShape == 1);
    }

    TEST_CASE("colliders - removing a collider rebuilds the shape") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.gravity = math::vec3(0.0f);
        physics_fixture fixture(jobs, settings);
        const ecs::entity entity = fixture.create_at(math::vec3(0.0f, 5.0f, 0.0f));
        physics::rigid_body body = {};
        body.motionType = physics::motion_type::STATIC;
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(entity, body));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(entity));
        physics::sphere_collider sphere = {};
        sphere.offset = math::vec3(3.0f, 0.0f, 0.0f);
        CONJURE_REQUIRE_OK(fixture.world.add<physics::sphere_collider>(entity, sphere));
        fixture.run_step();
        std::array<ecs::entity, 2> out = {};
        CHECK(fixture.physics.overlap_sphere(math::vec3(3.0f, 5.0f, 0.0f), 0.1f, out) == 1);
        CONJURE_REQUIRE_OK(fixture.world.remove<physics::sphere_collider>(entity));
        fixture.run_step();
        CHECK(fixture.physics.has_body(entity));
        CHECK(fixture.physics.overlap_sphere(math::vec3(3.0f, 5.0f, 0.0f), 0.1f, out) == 0);
        CHECK(fixture.physics.overlap_sphere(math::vec3(0.0f, 5.0f, 0.0f), 0.1f, out) == 1);
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("colliders - the last collider removed from an implicit static body destroys it") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const ecs::entity wall = fixture.create_at(math::vec3(0.0f, 0.0f, 0.0f));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(wall));
        fixture.run_step();
        CHECK(fixture.physics.has_body(wall));
        CHECK_FALSE(fixture.world.has<scene::interpolated_transform>(wall));
        const std::optional<math::transform> pose = fixture.physics.body_pose(wall);
        REQUIRE(pose.has_value());
        CONJURE_REQUIRE_OK(fixture.world.remove<physics::box_collider>(wall));
        CHECK_FALSE(fixture.physics.has_body(wall));
        CHECK(fixture.physics.body_count() == 0);
        fixture.run_step();
        CHECK(fixture.physics.body_count() == 0);
        CONJURE_CHECK_OK(fixture.physics.validate());
    }

    TEST_CASE("colliders - non-uniform scale on a sphere warns INVALID_SHAPE_SCALE and builds unscaled") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.gravity = math::vec3(0.0f);
        physics_fixture fixture(jobs, settings);
        const ecs::entity sphere = fixture.create_at(math::vec3(0.0f, 5.0f, 0.0f));
        scene::local_transform *local = fixture.world.get<scene::local_transform>(sphere);
        REQUIRE(local != nullptr);
        local->scale = math::vec3(2.0f, 1.0f, 1.0f);
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(sphere));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::sphere_collider>(sphere));
        testing::log_capture logs = {};
        fixture.run_step();
        CHECK(logs.contains(core::log_level::WARN, "INVALID_SHAPE_SCALE"));
        std::array<ecs::entity, 2> out = {};
        // Unscaled radius 0.5: a probe at x = 0.8 misses, the scaled sphere would have reached x = 1.
        CHECK(fixture.physics.overlap_sphere(math::vec3(0.8f, 5.0f, 0.0f), 0.05f, out) == 0);
        CHECK(fixture.physics.overlap_sphere(math::vec3(0.4f, 5.0f, 0.0f), 0.05f, out) == 1);

        // A uniform scale is applied.
        const ecs::entity big = fixture.create_at(math::vec3(10.0f, 5.0f, 0.0f));
        scene::local_transform *bigLocal = fixture.world.get<scene::local_transform>(big);
        REQUIRE(bigLocal != nullptr);
        bigLocal->scale = math::vec3(2.0f);
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(big));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::sphere_collider>(big));
        fixture.run_step();
        CHECK(fixture.physics.overlap_sphere(math::vec3(10.8f, 5.0f, 0.0f), 0.05f, out) == 1);
    }

    TEST_CASE("colliders - the shape cache shares one Jolt shape across 100 identical boxes") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        for (u32 index = 0; index < 100; ++index) {
            (void)fixture.spawn_box(math::vec3(static_cast<f32>(index) * 1.5f, 1.0f, 0.0f));
        }
        fixture.run_step();
        CHECK(fixture.physics.body_count() == 100);
        CHECK(fixture.physics.stats().shapeCacheEntries == 1);
        // A different size adds one entry; an offset box becomes a per-entity compound over the cached leaf.
        (void)fixture.spawn_box(math::vec3(0.0f, 5.0f, 5.0f), physics::motion_type::DYNAMIC, math::vec3(0.25f));
        const ecs::entity offset = fixture.create_at(math::vec3(0.0f, 5.0f, 10.0f));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(offset));
        physics::box_collider shifted = {};
        shifted.offset = math::vec3(0.0f, 1.0f, 0.0f);
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(offset, shifted));
        fixture.run_step();
        CHECK(fixture.physics.stats().shapeCacheEntries == 2);
    }

    TEST_CASE("colliders - a trigger body is a sensor that passes through geometry") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        (void)fixture.spawn_ground();
        const ecs::entity trigger = fixture.create_at(math::vec3(0.0f, 3.0f, 0.0f));
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(trigger));
        physics::box_collider box = {};
        box.isTrigger = true;
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(trigger, box));
        fixture.run_steps(120);
        // No response: the sensor falls through the ground.
        CHECK(fixture.position_of(trigger).y < -1.0f);
    }
}
