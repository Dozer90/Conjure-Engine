#include "conjure/testing/approx.h"
#include "conjure/testing/assert_capture.h"
#include "conjure/testing/doctest.h"
#include "physics_test_fixture.h"

#include "conjure/core/math/ray.h"
#include "conjure/core/math/transform.h"
#include "conjure/ecs/observer.h"
#include "conjure/physics/components.h"
#include "conjure/physics/queries.h"
#include "conjure/scene/components.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>

using namespace conjure;
using namespace physics_test;

namespace {

    [[nodiscard]] math::ray down_from(f32 height) {
        math::ray ray = {};
        ray.origin = math::vec3(0.0f, height, 0.0f);
        ray.direction = math::vec3(0.0f, -1.0f, 0.0f);
        return ray;
    }

    [[nodiscard]] physics::physics_settings settings_without_gravity() {
        physics::physics_settings settings = test_settings();
        settings.gravity = math::vec3(0.0f);
        return settings;
    }

    /// <summary>
    /// A static wall, 1 m thick and 10 m square, centred at x = 10 (a sweep along +x from the origin meets
    /// its face at x = 9.5), stepped once so the queries see it.
    /// </summary>
    [[nodiscard]] ecs::entity spawn_wall(physics_fixture &fixture) {
        const ecs::entity wall = fixture.spawn_box(
          math::vec3(10.0f, 0.0f, 0.0f),
          physics::motion_type::STATIC,
          math::vec3(0.5f, 5.0f, 5.0f)
        );
        fixture.run_step();
        return wall;
    }
}

TEST_SUITE("physics/queries") {

    TEST_CASE("query_filter - defaults include every layer and static bodies but no triggers") {
        const physics::query_filter filter = {};
        CHECK(filter.layers == physics::physics_layer_mask::all());
        CHECK_FALSE(filter.includeTriggers);
        CHECK(filter.includeStatic);
        CHECK(filter.ignore.is_null());
        const physics::raycast_hit hit = {};
        CHECK(hit.distance == 0.0f);
        const physics::cast_shape_desc shape = physics::sphere_shape_desc{};
        CHECK(std::holds_alternative<physics::sphere_shape_desc>(shape));
        CHECK(physics::box_shape_desc().halfExtents == math::vec3(0.5f));
        CHECK(physics::capsule_shape_desc().halfHeight == 0.5f);
    }

    TEST_CASE("raycast - a ray down onto the ground hits it at distance 10 with an upward normal") {
        for_each_job_mode([](core::jobs::job_system &jobs) {
            physics_fixture fixture(jobs);
            const ecs::entity ground = fixture.spawn_ground();
            fixture.run_step();
            const std::optional<physics::raycast_hit> hit = fixture.physics.raycast(down_from(10.0f), 100.0f);
            REQUIRE(hit.has_value());
            CHECK(hit->entity == ground);
            CHECK(std::abs(hit->distance - 10.0f) < 1.0e-3f);
            CHECK(hit->fraction == doctest::Approx(0.1f).epsilon(1.0e-3));
            CHECK(testing::approx_equal(hit->normal, math::vec3(0.0f, 1.0f, 0.0f), 1.0e-3f));
            CHECK(testing::approx_equal(hit->point, math::vec3(0.0f, 0.0f, 0.0f), 1.0e-3f));
            CHECK(hit->layer == 0);
            CHECK_FALSE(hit->isTrigger);
            // Too short to reach the ground: nothing.
            CHECK_FALSE(fixture.physics.raycast(down_from(10.0f), 5.0f).has_value());
            CHECK_FALSE(fixture.physics.raycast(down_from(10.0f), 0.0f).has_value());
        });
    }

    TEST_CASE("raycast - layer masks static filtering and ignore skip bodies") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        REQUIRE(settings.layers.add_layer(core::string_id("props")).has_value());
        physics_fixture fixture(jobs, settings);
        const ecs::entity ground = fixture.spawn_ground();
        const ecs::entity box = fixture.spawn_box(
          math::vec3(0.0f, 5.0f, 0.0f),
          physics::motion_type::DYNAMIC,
          math::vec3(0.5f),
          1
        );
        fixture.run_step();

        physics::query_filter excludeGround = {};
        excludeGround.layers = physics::physics_layer_mask::all().without(0);
        std::optional<physics::raycast_hit> hit = fixture.physics.raycast(down_from(10.0f), 100.0f, excludeGround);
        REQUIRE(hit.has_value());
        CHECK(hit->entity == box);
        CHECK(hit->layer == 1);

        physics::query_filter onlyGround = {};
        onlyGround.layers = physics::physics_layer_mask::only(0);
        hit = fixture.physics.raycast(down_from(10.0f), 100.0f, onlyGround);
        REQUIRE(hit.has_value());
        CHECK(hit->entity == ground);

        physics::query_filter nothing = {};
        nothing.layers = physics::physics_layer_mask::none();
        CHECK_FALSE(fixture.physics.raycast(down_from(10.0f), 100.0f, nothing).has_value());

        physics::query_filter noStatic = {};
        noStatic.includeStatic = false;
        hit = fixture.physics.raycast(down_from(10.0f), 100.0f, noStatic);
        REQUIRE(hit.has_value());
        CHECK(hit->entity == box);
        noStatic.layers = physics::physics_layer_mask::only(0);
        CHECK_FALSE(fixture.physics.raycast(down_from(10.0f), 100.0f, noStatic).has_value());

        physics::query_filter ignoreBox = {};
        ignoreBox.ignore = box;
        hit = fixture.physics.raycast(down_from(10.0f), 100.0f, ignoreBox);
        REQUIRE(hit.has_value());
        CHECK(hit->entity == ground);
        ignoreBox.ignore = ground;
        hit = fixture.physics.raycast(down_from(10.0f), 100.0f, ignoreBox);
        REQUIRE(hit.has_value());
        CHECK(hit->entity == box);
    }

    TEST_CASE("raycast_all - three stacked plates return three hits sorted by distance and truncate to two") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        std::array<ecs::entity, 3> plates = {};
        for (u32 index = 0; index < 3; ++index) {
            plates[index] = fixture.spawn_box(
              math::vec3(0.0f, static_cast<f32>(index) * 3.0f, 0.0f),
              physics::motion_type::STATIC,
              math::vec3(2.0f, 0.1f, 2.0f)
            );
        }
        fixture.run_step();
        std::array<physics::raycast_hit, 8> eight = {};
        const u32 count = fixture.physics.raycast_all(down_from(10.0f), 100.0f, eight);
        REQUIRE(count == 3);
        CHECK(eight[0].entity == plates[2]);
        CHECK(eight[1].entity == plates[1]);
        CHECK(eight[2].entity == plates[0]);
        CHECK(std::abs(eight[0].distance - 3.9f) < 1.0e-3f);
        CHECK(std::abs(eight[1].distance - 6.9f) < 1.0e-3f);
        CHECK(std::abs(eight[2].distance - 9.9f) < 1.0e-3f);
        CHECK(eight[0].fraction < eight[1].fraction);
        CHECK(eight[1].fraction < eight[2].fraction);
        for (u32 index = 0; index < count; ++index) {
            CHECK(testing::approx_equal(eight[index].normal, math::vec3(0.0f, 1.0f, 0.0f), 1.0e-3f));
        }

        std::array<physics::raycast_hit, 2> two = {};
        const u32 truncated = fixture.physics.raycast_all(down_from(10.0f), 100.0f, two);
        CHECK(truncated == 2);
        CHECK(two[0].entity == plates[2]);
        CHECK(two[1].entity == plates[1]);
        CHECK(two[0].distance < two[1].distance);
        CHECK(fixture.physics.raycast_all(down_from(10.0f), 100.0f, std::span<physics::raycast_hit>()) == 0);
        CHECK(fixture.physics.raycast_all(down_from(10.0f), 2.0f, eight) == 0);
    }

    TEST_CASE("raycast_all - a span smaller than the hit count keeps the nearest hits") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        // The three top plates are kinematic: the broad phase walks its static tree before the moving one, so
        // a collector that kept the first hits it was handed would return three of the lower, static plates.
        std::array<ecs::entity, 8> plates = {};
        for (u32 index = 0; index < plates.size(); ++index) {
            plates[index] = fixture.spawn_box(
              math::vec3(0.0f, static_cast<f32>(index) * 2.0f, 0.0f),
              index >= 5 ? physics::motion_type::KINEMATIC : physics::motion_type::STATIC,
              math::vec3(2.0f, 0.1f, 2.0f)
            );
        }
        fixture.run_step();
        std::array<physics::raycast_hit, 3> three = {};
        const u32 count = fixture.physics.raycast_all(down_from(20.0f), 100.0f, three);
        REQUIRE(count == 3);
        CHECK(three[0].entity == plates[7]);
        CHECK(three[1].entity == plates[6]);
        CHECK(three[2].entity == plates[5]);
        CHECK(std::abs(three[0].distance - 5.9f) < 1.0e-3f);
        CHECK(std::abs(three[2].distance - 9.9f) < 1.0e-3f);
        std::array<physics::raycast_hit, 8> every = {};
        CHECK(fixture.physics.raycast_all(down_from(20.0f), 100.0f, every) == 8);
    }

    TEST_CASE("queries - a raycast with an infinite maxDistance hits like a long finite one") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const ecs::entity ground = fixture.spawn_ground();
        fixture.run_step();
        const f32 infinity = std::numeric_limits<f32>::infinity();
        const std::optional<physics::raycast_hit> unlimited = fixture.physics.raycast(down_from(10.0f), infinity);
        const std::optional<physics::raycast_hit> finite = fixture.physics.raycast(down_from(10.0f), 1000.0f);
        REQUIRE(unlimited.has_value());
        REQUIRE(finite.has_value());
        CHECK(unlimited->entity == ground);
        CHECK(std::abs(unlimited->distance - finite->distance) < 1.0e-3f);
        CHECK(testing::approx_equal(unlimited->point, finite->point, 1.0e-3f));
        CHECK(testing::approx_equal(unlimited->normal, math::vec3(0.0f, 1.0f, 0.0f), 1.0e-3f));
        std::array<physics::raycast_hit, 4> all = {};
        REQUIRE(fixture.physics.raycast_all(down_from(10.0f), infinity, all) == 1);
        CHECK(std::abs(all[0].distance - 10.0f) < 1.0e-3f);

        // A non-finite origin or direction hits nothing.
        math::ray broken = down_from(10.0f);
        broken.origin.x = std::numeric_limits<f32>::quiet_NaN();
        CHECK_FALSE(fixture.physics.raycast(broken, 100.0f).has_value());
        CHECK(fixture.physics.raycast_all(broken, 100.0f, all) == 0);
        broken = down_from(10.0f);
        broken.direction.x = infinity;
        CHECK_FALSE(fixture.physics.raycast(broken, 100.0f).has_value());
        const physics::cast_shape_desc sphere = physics::sphere_shape_desc{};
        math::transform start = {};
        start.position = math::vec3(0.0f, 10.0f, 0.0f);
        CHECK_FALSE(fixture.physics.shape_cast(sphere, start, math::vec3(0.0f, -infinity, 0.0f), 20.0f).has_value());
        start.rotation = math::quat(std::numeric_limits<f32>::quiet_NaN(), 0.0f, 0.0f, 0.0f);
        CHECK_FALSE(fixture.physics.shape_cast(sphere, start, math::vec3(0.0f, -1.0f, 0.0f), 20.0f).has_value());
    }

    TEST_CASE("overlap_sphere - finds exactly the bodies inside") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.gravity = math::vec3(0.0f);
        physics_fixture fixture(jobs, settings);
        const ecs::entity near = fixture.spawn_box(math::vec3(1.0f, 0.0f, 0.0f));
        const ecs::entity alsoNear = fixture.spawn_sphere(math::vec3(-1.0f, 0.0f, 0.0f));
        const ecs::entity far = fixture.spawn_box(math::vec3(10.0f, 0.0f, 0.0f));
        fixture.run_step();
        std::array<ecs::entity, 8> out = {};
        const u32 count = fixture.physics.overlap_sphere(math::vec3(0.0f), 1.0f, out);
        REQUIRE(count == 2);
        const std::span<const ecs::entity> found(out.data(), count);
        CHECK(std::find(found.begin(), found.end(), near) != found.end());
        CHECK(std::find(found.begin(), found.end(), alsoNear) != found.end());
        CHECK(std::find(found.begin(), found.end(), far) == found.end());
        std::array<ecs::entity, 1> one = {};
        CHECK(fixture.physics.overlap_sphere(math::vec3(0.0f), 1.0f, one) == 1);
        CHECK(fixture.physics.overlap_sphere(math::vec3(0.0f), 0.2f, out) == 0);
        physics::query_filter ignoreNear = {};
        ignoreNear.ignore = near;
        CHECK(fixture.physics.overlap_sphere(math::vec3(0.0f), 1.0f, out, ignoreNear) == 1);
        CHECK(out[0] == alsoNear);
    }

    TEST_CASE("overlap_box - a box rotated 45 degrees reaches the corners an axis-aligned one misses") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.gravity = math::vec3(0.0f);
        physics_fixture fixture(jobs, settings);
        // A small sphere at x = 1.3: inside a box of half extent 1 only when the box is rotated so its
        // diagonal (length sqrt(2)) points along x.
        const ecs::entity target = fixture.spawn_sphere(math::vec3(1.3f, 0.0f, 0.0f), 0.05f);
        fixture.run_step();
        std::array<ecs::entity, 4> out = {};
        const math::quat identity(1.0f, 0.0f, 0.0f, 0.0f);
        CHECK(fixture.physics.overlap_box(math::vec3(0.0f), math::vec3(1.0f), identity, out) == 0);
        const math::quat rotated = glm::angleAxis(math::radians(45.0f), math::vec3(0.0f, 1.0f, 0.0f));
        REQUIRE(fixture.physics.overlap_box(math::vec3(0.0f), math::vec3(1.0f), rotated, out) == 1);
        CHECK(out[0] == target);
    }

    TEST_CASE("shape_cast - a sphere stops at the wall with the analytic fraction") {
        for_each_job_mode([](core::jobs::job_system &jobs) {
            physics_fixture fixture(jobs, settings_without_gravity());
            const ecs::entity wall = spawn_wall(fixture);
            math::transform start = {};
            start.position = math::vec3(0.0f);
            physics::sphere_shape_desc sphere = {};
            sphere.radius = 0.5f;
            const std::optional<physics::shape_cast_hit> hit = fixture.physics.shape_cast(
              sphere,
              start,
              math::vec3(1.0f, 0.0f, 0.0f),
              20.0f
            );
            REQUIRE(hit.has_value());
            CHECK(hit->entity == wall);
            // The sphere surface meets the wall face at x = 9.5, so its centre travels 9 m of 20.
            CHECK(std::abs(hit->fraction - 0.45f) < 1.0e-3f);
            CHECK(std::abs(hit->distance - 9.0f) < 0.02f);
            CHECK(testing::approx_equal(hit->normal, math::vec3(-1.0f, 0.0f, 0.0f), 1.0e-2f));
            CHECK(hit->point.x == doctest::Approx(9.5f).epsilon(0.01));
            CHECK(hit->layer == 0);
            CHECK(hit->penetrationDepth == 0.0f);
            CHECK_FALSE(fixture.physics.shape_cast(sphere, start, math::vec3(1.0f, 0.0f, 0.0f), 5.0f).has_value());

            physics::box_shape_desc box = {};
            box.halfExtents = math::vec3(0.5f);
            const std::optional<physics::shape_cast_hit> boxHit = fixture.physics.shape_cast(
              box,
              start,
              math::vec3(1.0f, 0.0f, 0.0f),
              20.0f
            );
            REQUIRE(boxHit.has_value());
            CHECK(std::abs(boxHit->fraction - 0.45f) < 1.0e-2f);
            physics::capsule_shape_desc capsule = {};
            const std::optional<physics::shape_cast_hit> capsuleHit = fixture.physics.shape_cast(
              capsule,
              start,
              math::vec3(1.0f, 0.0f, 0.0f),
              20.0f
            );
            REQUIRE(capsuleHit.has_value());
            CHECK(std::abs(capsuleHit->fraction - 0.45f) < 1.0e-2f);
        });
    }

    TEST_CASE("regression - code sweep F621 shape_cast takes a unit direction and a maxDistance like raycast") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs, settings_without_gravity());
        const ecs::entity wall = spawn_wall(fixture);
        const physics::cast_shape_desc sphere = physics::sphere_shape_desc{0.5f};
        const math::transform start = math::transform::from_position(math::vec3(0.0f));

        SUBCASE("the length of the direction does not scale the sweep") {
            // The sphere surface meets the wall face at x = 9.5, so its centre travels 9 m of the 20.
            constexpr std::array<f32, 4> LENGTHS = {0.25f, 1.0f, 4.0f, 20.0f};
            for (const f32 length : LENGTHS) {
                CAPTURE(length);
                const math::vec3 direction = math::vec3(length, 0.0f, 0.0f);
                const std::optional<physics::shape_cast_hit> hit =
                  fixture.physics.shape_cast(sphere, start, direction, 20.0f);
                REQUIRE(hit.has_value());
                CHECK(hit->entity == wall);
                CHECK(std::abs(hit->fraction - 0.45f) < 1.0e-3f);
                CHECK(std::abs(hit->distance - 9.0f) < 0.02f);
                CHECK(std::abs(hit->distance - hit->fraction * 20.0f) < 1.0e-4f);
                // 5 m stops short of the wall whatever the length of the direction.
                CHECK_FALSE(fixture.physics.shape_cast(sphere, start, direction, 5.0f).has_value());
            }
        }

        SUBCASE("an infinite maxDistance is clamped to 1e6 m and hits like a long finite one") {
            const f32 infinity = std::numeric_limits<f32>::infinity();
            const std::optional<physics::shape_cast_hit> unlimited =
              fixture.physics.shape_cast(sphere, start, math::vec3(1.0f, 0.0f, 0.0f), infinity);
            REQUIRE(unlimited.has_value());
            CHECK(unlimited->entity == wall);
            CHECK(std::abs(unlimited->distance - 9.0f) < 0.02f);
            CHECK(std::abs(unlimited->distance - unlimited->fraction * 1.0e6f) < 1.0e-3f);
            CHECK(testing::approx_equal(unlimited->normal, math::vec3(-1.0f, 0.0f, 0.0f), 1.0e-2f));
        }

        SUBCASE("a maxDistance that is not positive hits nothing, even from inside a body") {
            // Both starts point away from the wall (-x), so a negative distance that reversed the sweep would
            // send it towards the wall from the origin and deeper into it from inside, and either would hit.
            const math::vec3 away = math::vec3(-1.0f, 0.0f, 0.0f);
            const math::transform inside = math::transform::from_position(math::vec3(9.8f, 0.0f, 0.0f));
            constexpr std::array<f32, 4> REFUSED = {0.0f, -0.0f, -20.0f, std::numeric_limits<f32>::quiet_NaN()};
            for (usize index = 0; index < REFUSED.size(); ++index) {
                CAPTURE(index);
                CHECK_FALSE(fixture.physics.shape_cast(sphere, start, away, REFUSED[index]).has_value());
                CHECK_FALSE(fixture.physics.shape_cast(sphere, inside, away, REFUSED[index]).has_value());
            }
        }
    }

    TEST_CASE("shape_cast - a zero direction sweeps along minus z and a start inside a body that sweeps deeper hits "
              "at fraction 0") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs, settings_without_gravity());
        const ecs::entity wall = spawn_wall(fixture);
        const physics::cast_shape_desc sphere = physics::sphere_shape_desc{0.5f};

        SUBCASE("a zero direction takes the default direction of math::ray") {
            // The +z face of the wall is at z = 5: from z = 15 the centre of the sphere travels 9.5 m along -z.
            const math::transform start = math::transform::from_position(math::vec3(10.0f, 0.0f, 15.0f));
            const std::optional<physics::shape_cast_hit> hit =
              fixture.physics.shape_cast(sphere, start, math::vec3(0.0f), 20.0f);
            REQUIRE(hit.has_value());
            CHECK(hit->entity == wall);
            CHECK(std::abs(hit->distance - 9.5f) < 0.02f);
            CHECK(testing::approx_equal(hit->normal, math::vec3(0.0f, 0.0f, 1.0f), 1.0e-2f));
            // The same start sweeping along +x misses the wall.
            CHECK_FALSE(fixture.physics.shape_cast(sphere, start, math::vec3(1.0f, 0.0f, 0.0f), 20.0f).has_value());
        }

        SUBCASE("a shape that starts inside a body and sweeps deeper reports it at fraction 0") {
            // At x = 9.8 the sphere reaches 0.8 m past the -x face of the wall (x = 9.5). Only the fraction and
            // the distance are pinned: the normal and penetrationDepth of an overlapping start are approximate.
            const math::transform inside = math::transform::from_position(math::vec3(9.8f, 0.0f, 0.0f));
            const std::optional<physics::shape_cast_hit> hit =
              fixture.physics.shape_cast(sphere, inside, math::vec3(1.0f, 0.0f, 0.0f), 20.0f);
            REQUIRE(hit.has_value());
            CHECK(hit->entity == wall);
            CHECK(hit->fraction == 0.0f);
            CHECK(hit->distance == 0.0f);
        }
    }

    TEST_CASE("queries - triggers are excluded unless includeTriggers") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.gravity = math::vec3(0.0f);
        physics_fixture fixture(jobs, settings);
        const ecs::entity trigger = fixture.create_at(math::vec3(0.0f, 0.0f, 0.0f));
        physics::rigid_body body = {};
        body.motionType = physics::motion_type::STATIC;
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(trigger, body));
        physics::box_collider volume = {};
        volume.isTrigger = true;
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(trigger, volume));
        fixture.run_step();
        CHECK_FALSE(fixture.physics.raycast(down_from(5.0f), 10.0f).has_value());
        std::array<ecs::entity, 2> out = {};
        CHECK(fixture.physics.overlap_sphere(math::vec3(0.0f), 0.1f, out) == 0);
        std::array<physics::raycast_hit, 2> hits = {};
        CHECK(fixture.physics.raycast_all(down_from(5.0f), 10.0f, hits) == 0);
        CHECK_FALSE(fixture.physics.shape_cast(
          physics::sphere_shape_desc{},
          math::transform::from_position(math::vec3(-5.0f, 0.0f, 0.0f)),
          math::vec3(1.0f, 0.0f, 0.0f),
          10.0f
        ).has_value());

        physics::query_filter withTriggers = {};
        withTriggers.includeTriggers = true;
        const std::optional<physics::raycast_hit> hit = fixture.physics.raycast(down_from(5.0f), 10.0f, withTriggers);
        REQUIRE(hit.has_value());
        CHECK(hit->entity == trigger);
        CHECK(hit->isTrigger);
        CHECK(fixture.physics.overlap_sphere(math::vec3(0.0f), 0.1f, out, withTriggers) == 1);
        CHECK(fixture.physics.raycast_all(down_from(5.0f), 10.0f, hits, withTriggers) == 1);
        CHECK(fixture.physics.shape_cast(
          physics::sphere_shape_desc{},
          math::transform::from_position(math::vec3(-5.0f, 0.0f, 0.0f)),
          math::vec3(1.0f, 0.0f, 0.0f),
          10.0f,
          withTriggers
        ).has_value());

        // A static trigger stays a static body to the filters, although Jolt keeps it as an active kinematic
        // sensor (code sweep F408).
        physics::query_filter movingOnly = withTriggers;
        movingOnly.includeStatic = false;
        CHECK_FALSE(fixture.physics.raycast(down_from(5.0f), 10.0f, movingOnly).has_value());
        CHECK(fixture.physics.overlap_sphere(math::vec3(0.0f), 0.1f, out, movingOnly) == 0);
        CHECK(fixture.physics.raycast_all(down_from(5.0f), 10.0f, hits, movingOnly) == 0);
    }

    TEST_CASE("shape_cast - isTrigger is set on a trigger volume and clear on a solid body") {
        core::jobs::job_system jobs = {};
        physics::physics_settings settings = test_settings();
        settings.gravity = math::vec3(0.0f);
        physics_fixture fixture(jobs, settings);
        // Along +x from x = -5: the trigger volume at the origin first, the solid wall at x = 5 behind it.
        const ecs::entity trigger = fixture.create_at(math::vec3(0.0f, 0.0f, 0.0f));
        physics::rigid_body body = {};
        body.motionType = physics::motion_type::STATIC;
        CONJURE_REQUIRE_OK(fixture.world.add<physics::rigid_body>(trigger, body));
        physics::box_collider volume = {};
        volume.isTrigger = true;
        CONJURE_REQUIRE_OK(fixture.world.add<physics::box_collider>(trigger, volume));
        const ecs::entity wall = fixture.spawn_box(math::vec3(5.0f, 0.0f, 0.0f), physics::motion_type::STATIC);
        fixture.run_step();

        physics::query_filter withTriggers = {};
        withTriggers.includeTriggers = true;
        const math::transform start = math::transform::from_position(math::vec3(-5.0f, 0.0f, 0.0f));
        const math::vec3 direction = math::vec3(1.0f, 0.0f, 0.0f);
        const std::array<physics::cast_shape_desc, 3> shapes = {
          physics::sphere_shape_desc{0.25f},
          physics::box_shape_desc{math::vec3(0.25f)},
          physics::capsule_shape_desc{0.25f, 0.25f},
        };
        for (usize index = 0; index < shapes.size(); ++index) {
            CAPTURE(index);
            const std::optional<physics::shape_cast_hit> volumeHit =
              fixture.physics.shape_cast(shapes[index], start, direction, 20.0f, withTriggers);
            REQUIRE(volumeHit.has_value());
            CHECK(volumeHit->entity == trigger);
            CHECK(volumeHit->isTrigger);

            const std::optional<physics::shape_cast_hit> wallHit =
              fixture.physics.shape_cast(shapes[index], start, direction, 20.0f);
            REQUIRE(wallHit.has_value());
            CHECK(wallHit->entity == wall);
            CHECK_FALSE(wallHit->isTrigger);
        }
    }

    TEST_CASE("queries - during a push step or pull they assert and return nothing") {
        core::jobs::job_system jobs = {};
        physics_fixture fixture(jobs);
        const ecs::entity ground = fixture.spawn_ground();
        const ecs::entity box = fixture.spawn_box(math::vec3(0.0f, 5.0f, 0.0f), physics::motion_type::STATIC);
        fixture.run_step();

        // Re-entrancy from inside push_transforms: a STATIC -> DYNAMIC change adds interpolated_transform
        // through the world's own deferred buffer, which the push flushes before it returns, so an
        // ADDED observer of that component runs while the push (and its row-table mutation) is in
        // progress. Every query must refuse (STEP_IN_PROGRESS) instead of reading the tables.
        struct probe_results {
            u32 fired = 0;
            std::optional<physics::raycast_hit> hit = std::nullopt;
            u32 rayHits = 99;
            u32 sphereHits = 99;
            u32 boxHits = 99;
            std::optional<physics::shape_cast_hit> cast = std::nullopt;
            std::optional<math::transform> pose = std::nullopt;
        };
        probe_results probe = {};
        const ecs::component_id interpolatedId = fixture.registry.id_of<scene::interpolated_transform>();
        const ecs::observer_id observer = fixture.world.observe(
          interpolatedId,
          ecs::component_event::ADDED,
          [&](ecs::world &, ecs::entity, ecs::component_id, ecs::command_buffer &) {
              ++probe.fired;
              std::array<physics::raycast_hit, 4> rays = {};
              std::array<ecs::entity, 4> out = {};
              probe.hit = fixture.physics.raycast(down_from(10.0f), 100.0f);
              probe.rayHits = fixture.physics.raycast_all(down_from(10.0f), 100.0f, rays);
              probe.sphereHits = fixture.physics.overlap_sphere(math::vec3(0.0f), 1.0f, out);
              probe.boxHits = fixture.physics.overlap_box(
                math::vec3(0.0f),
                math::vec3(1.0f),
                math::quat(1.0f, 0.0f, 0.0f, 0.0f),
                out
              );
              probe.cast = fixture.physics.shape_cast(
                physics::sphere_shape_desc{},
                math::transform::from_position(math::vec3(0.0f, 10.0f, 0.0f)),
                math::vec3(0.0f, -1.0f, 0.0f),
                20.0f
              );
              probe.pose = fixture.physics.body_pose(ground);
          }
        );
        physics::rigid_body *body = fixture.world.get<physics::rigid_body>(box);
        REQUIRE(body != nullptr);
        body->motionType = physics::motion_type::DYNAMIC;
        testing::expect_assert guard = {};
        fixture.physics.push_transforms(STEP);
        REQUIRE(probe.fired == 1);
        CHECK_FALSE(probe.hit.has_value());
        CHECK(probe.rayHits == 0);
        CHECK(probe.sphereHits == 0);
        CHECK(probe.boxHits == 0);
        CHECK_FALSE(probe.cast.has_value());
        CHECK_FALSE(probe.pose.has_value());
        CHECK(guard.count() == 5 * ASSERTS_PER_VIOLATION);
        fixture.world.unobserve(observer);

        // Between the phases the same queries work and nothing asserts.
        fixture.physics.step(STEP);
        fixture.physics.pull_transforms();
        fixture.physics.collect_contact_events();
        const std::optional<physics::raycast_hit> hit = fixture.physics.raycast(down_from(10.0f), 100.0f);
        REQUIRE(hit.has_value());
        CHECK(hit->entity == box);
        CHECK(fixture.physics.body_pose(ground).has_value());
        CHECK(guard.count() == 5 * ASSERTS_PER_VIOLATION);
    }
}
