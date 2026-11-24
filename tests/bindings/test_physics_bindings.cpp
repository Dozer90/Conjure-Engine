#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"
#include "bindings_test_support.h"

#include "conjure/bindings/bindings_error.h"
#include "conjure/bindings/marshal.h"
#include "conjure/core/error.h"
#include "conjure/core/hash.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/math/math.h"
#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/physics/character_controller.h"
#include "conjure/physics/components.h"
#include "conjure/physics/physics_error.h"
#include "conjure/physics/physics_layers.h"
#include "conjure/physics/physics_world.h"
#include "conjure/scene/components.h"
#include "conjure/scene/scene.h"
#include "conjure/scene/scene_manager.h"
#include "conjure/scripting/script_component.h"

#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

// physics_* (docs/design/15_bindings.md 10, 14_physics.md 8): queries (with the conjure_query_flags of wave 3
// GAP-CSHARP 2.4 and the C# overloads that pass them), velocities, forces, kinematic moves, teleport, sleep,
// gravity, layers and characters over a headless Jolt world; everything NOT_SUPPORTED through the stubs when no
// physics world is present.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::testing;
using namespace bindings_test;

namespace {

    constexpr f32 STEP = 1.0f / 60.0f;
    constexpr std::string_view QUERY_FLAGS_PROBE = "Conjure.TestScripts.QueryFlagsProbe";

    struct physics_fixture {
        bare_fixture base;

        physics_fixture() :
            base(make_options()) {}

        [[nodiscard]] static bare_fixture_options make_options() noexcept {
            bare_fixture_options options = {};
            options.physics = true;
            return options;
        }

        [[nodiscard]] ecs::entity create_at(const math::vec3 &position) {
            const ecs::entity entity = base.create("body");
            scene::local_transform local = {};
            local.position = position;
            CONJURE_REQUIRE_OK(base.active().set_local_transform(entity, local));
            return entity;
        }

        [[nodiscard]] ecs::entity spawn_box(
          const math::vec3 &position,
          physics::motion_type motion = physics::motion_type::DYNAMIC,
          const math::vec3 &halfExtents = math::vec3(0.5f),
          u8 layer = 0
        ) {
            const ecs::entity entity = create_at(position);
            physics::rigid_body body = {};
            body.motionType = motion;
            body.layer = layer;
            CONJURE_REQUIRE_OK(base.world.add<physics::rigid_body>(entity, body));
            physics::box_collider collider = {};
            collider.halfExtents = halfExtents;
            CONJURE_REQUIRE_OK(base.world.add<physics::box_collider>(entity, collider));
            return entity;
        }

        /// <summary>
        /// A static trigger volume (a box sensor) of half extent 0.5 m.
        /// </summary>
        [[nodiscard]] ecs::entity spawn_trigger(const math::vec3 &position) {
            const ecs::entity entity = create_at(position);
            physics::rigid_body body = {};
            body.motionType = physics::motion_type::STATIC;
            CONJURE_REQUIRE_OK(base.world.add<physics::rigid_body>(entity, body));
            physics::box_collider collider = {};
            collider.isTrigger = true;
            CONJURE_REQUIRE_OK(base.world.add<physics::box_collider>(entity, collider));
            return entity;
        }

        [[nodiscard]] ecs::entity spawn_ground(u8 layer = 0) {
            return spawn_box(
              math::vec3(0.0f, -0.5f, 0.0f), physics::motion_type::STATIC, math::vec3(50.0f, 0.5f, 50.0f), layer
            );
        }

        [[nodiscard]] ecs::entity spawn_character(const math::vec3 &position) {
            const ecs::entity entity = create_at(position);
            CONJURE_REQUIRE_OK(base.world.add<physics::character_controller>(entity));
            return entity;
        }

        void run_steps(u32 count) {
            for (u32 index = 0; index < count; ++index) {
                base.physics->push_transforms(STEP);
                base.physics->step(STEP);
                base.physics->pull_transforms();
                base.physics->collect_contact_events();
            }
        }

        [[nodiscard]] conjure_ray_query down_from(
          f32 height,
          u32 mask = 0xFFFFFFFFu,
          uint64_t ignore = 0
        ) const noexcept {
            conjure_ray_query query = {};
            query.origin = {0.0f, height, 0.0f};
            query.direction = {0.0f, -2.0f, 0.0f};        // not unit length: the entry normalises
            query.max_distance = 100.0f;
            query.layer_mask = mask;
            query.ignore_entity = ignore;
            return query;
        }
    };
}

TEST_SUITE("bindings/physics_bindings") {

    TEST_CASE("physics - raycast hits the ground within a millimetre") {
        physics_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity ground = fixture.spawn_ground();
        fixture.run_steps(1);
        const conjure_ray_query query = fixture.down_from(10.0f);
        conjure_raycast_hit hit = {};
        CHECK(api.physics_raycast(&query, &hit) == 1);
        CHECK(hit.entity == ground.to_u64());
        CHECK(std::abs(hit.point.y) < 0.001f);
        CHECK(std::abs(hit.distance - 10.0f) < 0.001f);
        CHECK(hit.normal.y == doctest::Approx(1.0f).epsilon(0.01));
        CHECK(hit.layer == 0);
        conjure_ray_query shortRay = query;
        shortRay.max_distance = 5.0f;
        CHECK(api.physics_raycast(&shortRay, &hit) == 0);
        conjure_ray_query zero = query;
        zero.direction = {0.0f, 0.0f, 0.0f};
        CHECK(api.physics_raycast(&zero, &hit) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.physics_raycast(nullptr, &hit) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.physics_raycast(&query, nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
    }

    TEST_CASE("physics - raycast_all is sorted by distance and follows the count protocol") {
        physics_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        static_cast<void>(fixture.spawn_ground());
        const ecs::entity high = fixture.spawn_box(math::vec3(0.0f, 6.0f, 0.0f), physics::motion_type::STATIC);
        const ecs::entity low = fixture.spawn_box(math::vec3(0.0f, 3.0f, 0.0f), physics::motion_type::STATIC);
        fixture.run_steps(1);
        const conjure_ray_query query = fixture.down_from(10.0f);
        std::array<conjure_raycast_hit, 8> hits = {};
        const int32_t count = api.physics_raycast_all(&query, hits.data(), 8);
        REQUIRE(count == 3);
        CHECK(hits[0].entity == high.to_u64());
        CHECK(hits[1].entity == low.to_u64());
        CHECK(hits[0].distance < hits[1].distance);
        CHECK(hits[1].distance < hits[2].distance);
        CHECK(api.physics_raycast_all(&query, nullptr, 0) == 3);
        std::array<conjure_raycast_hit, 1> one = {};
        CHECK(api.physics_raycast_all(&query, one.data(), 1) == 3);
        CHECK(one[0].entity == high.to_u64());
        // A capacity above the 1024 count ceiling grows the scratch: the caller's buffer is filled completely.
        std::vector<conjure_raycast_hit> wide(2000);
        CHECK(api.physics_raycast_all(&query, wide.data(), static_cast<int32_t>(wide.size())) == 3);
        CHECK(wide[0].entity == high.to_u64());
        CHECK(wide[2].distance > wide[1].distance);
        std::vector<uint64_t> overlaps(1500);
        const conjure_vec3 origin = marshal::from_vec3(math::vec3(0.0f, 3.0f, 0.0f));
        CHECK(api.physics_overlap_sphere(&origin, 0.6f, 0xFFFFFFFFu, 0, 0, overlaps.data(), 1500) >= 1);
        CHECK(overlaps[0] != 0);
        CHECK(api.physics_raycast_all(&query, hits.data(), -1) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.physics_raycast_all(nullptr, hits.data(), 8) == to_status(bindings_error::INVALID_ARGUMENT));
    }

    TEST_CASE("physics - ignore entity and layer mask filter the hits") {
        physics_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity ground = fixture.spawn_ground(0);
        const ecs::entity lid =
          fixture.spawn_box(math::vec3(0.0f, 3.0f, 0.0f), physics::motion_type::STATIC, math::vec3(0.5f), 1);
        fixture.run_steps(1);
        conjure_raycast_hit hit = {};
        const conjure_ray_query ignoring = fixture.down_from(10.0f, 0xFFFFFFFFu, lid.to_u64());
        CHECK(api.physics_raycast(&ignoring, &hit) == 1);
        CHECK(hit.entity == ground.to_u64());
        const conjure_ray_query onlyLayerOne = fixture.down_from(10.0f, 1u << 1);
        CHECK(api.physics_raycast(&onlyLayerOne, &hit) == 1);
        CHECK(hit.entity == lid.to_u64());
        CHECK(hit.layer == 1);
        const conjure_ray_query nothing = fixture.down_from(10.0f, 1u << 5);
        CHECK(api.physics_raycast(&nothing, &hit) == 0);
    }

    TEST_CASE("physics - shape_cast sweeps a sphere, a box and a capsule") {
        physics_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity wall =
          fixture.spawn_box(math::vec3(10.0f, 0.0f, 0.0f), physics::motion_type::STATIC, math::vec3(0.5f, 5.0f, 5.0f));
        fixture.run_steps(1);
        conjure_shape_query query = {};
        query.ray.origin = {0.0f, 0.0f, 0.0f};
        query.ray.direction = {1.0f, 0.0f, 0.0f};
        query.ray.max_distance = 20.0f;
        query.ray.layer_mask = 0xFFFFFFFFu;
        query.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        query.radius = 0.5f;
        query.half_extents = {0.5f, 0.5f, 0.5f};
        conjure_raycast_hit hit = {};
        constexpr std::array<conjure_cast_shape, 3> SHAPES = {
          conjure_cast_shape::SPHERE, conjure_cast_shape::BOX, conjure_cast_shape::CAPSULE
        };
        for (const conjure_cast_shape shape : SHAPES) {
            CAPTURE(static_cast<int>(shape));
            query.shape = static_cast<uint8_t>(shape);
            CHECK(api.physics_shape_cast(&query, &hit) == 1);
            CHECK(hit.entity == wall.to_u64());
            // The shape surface meets the wall face at x = 9.5: the centre travels 9 m.
            CHECK(hit.distance == doctest::Approx(9.0f).epsilon(0.02));
            CHECK(hit.point.x == doctest::Approx(9.5f).epsilon(0.02));
        }
        query.ray.max_distance = 5.0f;
        CHECK(api.physics_shape_cast(&query, &hit) == 0);
        query.shape = 9;
        CHECK(api.physics_shape_cast(&query, &hit) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.physics_shape_cast(nullptr, &hit) == to_status(bindings_error::INVALID_ARGUMENT));
    }

    TEST_CASE("physics - a non-finite or negative max_distance is INVALID_ARGUMENT") {
        physics_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity ground = fixture.spawn_ground();
        fixture.run_steps(1);
        conjure_raycast_hit hit = {};
        std::array<conjure_raycast_hit, 4> hits = {};
        conjure_shape_query cast = {};
        cast.ray = fixture.down_from(10.0f);
        cast.shape = static_cast<uint8_t>(conjure_cast_shape::SPHERE);
        cast.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        cast.radius = 0.5f;
        constexpr std::array<f32, 4> REFUSED = {
          std::numeric_limits<f32>::infinity(), -std::numeric_limits<f32>::infinity(),
          std::numeric_limits<f32>::quiet_NaN(), -1.0f
        };
        for (usize index = 0; index < REFUSED.size(); ++index) {
            CAPTURE(index);
            conjure_ray_query query = fixture.down_from(10.0f);
            query.max_distance = REFUSED[index];
            CHECK(api.physics_raycast(&query, &hit) == to_status(bindings_error::INVALID_ARGUMENT));
            CHECK(api.physics_raycast_all(&query, hits.data(), 4) == to_status(bindings_error::INVALID_ARGUMENT));
            cast.ray.max_distance = REFUSED[index];
            CHECK(api.physics_shape_cast(&cast, &hit) == to_status(bindings_error::INVALID_ARGUMENT));
        }
        // There is no infinite ray: a world-sized distance hits, zero is a valid miss.
        conjure_ray_query far = fixture.down_from(10.0f);
        far.max_distance = 1000.0f;
        CHECK(api.physics_raycast(&far, &hit) == 1);
        CHECK(hit.entity == ground.to_u64());
        CHECK(api.physics_raycast_all(&far, hits.data(), 4) == 1);
        conjure_ray_query zero = fixture.down_from(10.0f);
        zero.max_distance = 0.0f;
        CHECK(api.physics_raycast(&zero, &hit) == 0);
        CHECK(api.physics_raycast_all(&zero, hits.data(), 4) == 0);
    }

    TEST_CASE("physics - a shape_cast with max_distance zero misses from inside a body") {
        physics_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity ground = fixture.spawn_ground();
        fixture.run_steps(1);
        conjure_shape_query cast = {};
        cast.ray = fixture.down_from(0.0f);            // the lower half of the sphere is inside the ground
        cast.shape = static_cast<uint8_t>(conjure_cast_shape::SPHERE);
        cast.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        cast.radius = 0.5f;
        cast.ray.max_distance = 0.0f;
        conjure_raycast_hit hit = {};
        CHECK(api.physics_shape_cast(&cast, &hit) == 0);
        // A positive distance, here sweeping down into the ground, reports the overlap at once.
        cast.ray.max_distance = 1.0f;
        REQUIRE(api.physics_shape_cast(&cast, &hit) == 1);
        CHECK(hit.entity == ground.to_u64());
        CHECK(hit.distance == 0.0f);
    }

    TEST_CASE("physics - overlap counts follow the protocol") {
        physics_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        static_cast<void>(fixture.spawn_ground());
        for (u32 index = 0; index < 4; ++index) {
            static_cast<void>(
              fixture.spawn_box(math::vec3(static_cast<f32>(index) * 1.5f, 0.6f, 0.0f), physics::motion_type::STATIC)
            );
        }
        fixture.run_steps(1);
        const conjure_vec3 center = {0.0f, 0.6f, 0.0f};
        std::array<uint64_t, 8> found = {};
        const int32_t sphereCount = api.physics_overlap_sphere(&center, 0.6f, 0xFFFFFFFFu, 0, 0, found.data(), 8);
        CHECK(sphereCount >= 1);
        CHECK(api.physics_overlap_sphere(&center, 0.6f, 0xFFFFFFFFu, 0, 0, nullptr, 0) == sphereCount);
        const conjure_vec3 wide = {2.0f, 0.6f, 0.0f};
        const conjure_vec3 halfExtents = {3.0f, 0.2f, 0.2f};
        const conjure_quat_xyzw identity = {0.0f, 0.0f, 0.0f, 1.0f};
        const int32_t boxCount =
          api.physics_overlap_box(&wide, &halfExtents, &identity, 0xFFFFFFFFu, 0, 0, found.data(), 8);
        CHECK(boxCount >= 4);
        std::array<uint64_t, 2> small = {};
        CHECK(api.physics_overlap_box(&wide, &halfExtents, nullptr, 0xFFFFFFFFu, 0, 0, small.data(), 2) == boxCount);
        CHECK(small[1] != 0);
        CHECK(
          api.physics_overlap_sphere(nullptr, 1.0f, 0xFFFFFFFFu, 0, 0, found.data(), 8)
          == to_status(bindings_error::INVALID_ARGUMENT)
        );
        CHECK(
          api.physics_overlap_sphere(&center, -1.0f, 0xFFFFFFFFu, 0, 0, found.data(), 8)
          == to_status(bindings_error::INVALID_ARGUMENT)
        );
        CHECK(
          api.physics_overlap_box(&wide, nullptr, &identity, 0xFFFFFFFFu, 0, 0, found.data(), 8)
          == to_status(bindings_error::INVALID_ARGUMENT)
        );
        CHECK(
          api.physics_overlap_box(&wide, &halfExtents, &identity, 0xFFFFFFFFu, 0, 0, found.data(), -1)
          == to_status(bindings_error::INVALID_ARGUMENT)
        );
    }

    TEST_CASE("regression - code sweep W3-CS-L1 a query with INCLUDE_TRIGGERS hits a trigger volume") {
        physics_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity ground = fixture.spawn_ground();
        const ecs::entity trigger = fixture.spawn_trigger(math::vec3(0.0f, 3.0f, 0.0f));
        fixture.run_steps(1);
        conjure_ray_query query = fixture.down_from(10.0f);
        query.flags = static_cast<uint32_t>(conjure_query_flags::INCLUDE_TRIGGERS);
        conjure_raycast_hit hit = {};
        REQUIRE(api.physics_raycast(&query, &hit) == 1);
        CHECK(hit.entity == trigger.to_u64());
        CHECK(hit.is_trigger == 1);

        std::array<conjure_raycast_hit, 4> hits = {};
        REQUIRE(api.physics_raycast_all(&query, hits.data(), 4) == 2);
        CHECK(hits[0].entity == trigger.to_u64());
        CHECK(hits[0].is_trigger == 1);
        CHECK(hits[1].entity == ground.to_u64());
        CHECK(hits[1].is_trigger == 0);

        const conjure_vec3 center = marshal::from_vec3(math::vec3(0.0f, 3.0f, 0.0f));
        std::array<uint64_t, 4> found = {};
        const uint32_t includeTriggers = static_cast<uint32_t>(conjure_query_flags::INCLUDE_TRIGGERS);
        CHECK(api.physics_overlap_sphere(&center, 0.2f, 0xFFFFFFFFu, includeTriggers, 0, found.data(), 4) == 1);
        CHECK(found[0] == trigger.to_u64());
        const conjure_vec3 halfExtents = {0.2f, 0.2f, 0.2f};
        found = {};
        CHECK(
          api.physics_overlap_box(&center, &halfExtents, nullptr, 0xFFFFFFFFu, includeTriggers, 0, found.data(), 4)
          == 1
        );
        CHECK(found[0] == trigger.to_u64());
    }

    TEST_CASE("physics - a query without flags skips a trigger volume and reports no trigger") {
        physics_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity ground = fixture.spawn_ground();
        static_cast<void>(fixture.spawn_trigger(math::vec3(0.0f, 3.0f, 0.0f)));
        fixture.run_steps(1);
        const conjure_ray_query query = fixture.down_from(10.0f);
        conjure_raycast_hit hit = {};
        hit.is_trigger = 7;                                     // the entry writes the whole record
        REQUIRE(api.physics_raycast(&query, &hit) == 1);
        CHECK(hit.entity == ground.to_u64());
        CHECK(hit.is_trigger == 0);
        std::array<conjure_raycast_hit, 4> hits = {};
        CHECK(api.physics_raycast_all(&query, hits.data(), 4) == 1);

        conjure_shape_query cast = {};
        cast.ray = query;
        cast.shape = static_cast<uint8_t>(conjure_cast_shape::SPHERE);
        cast.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        cast.radius = 0.25f;
        REQUIRE(api.physics_shape_cast(&cast, &hit) == 1);
        CHECK(hit.entity == ground.to_u64());

        const conjure_vec3 center = marshal::from_vec3(math::vec3(0.0f, 3.0f, 0.0f));
        const conjure_vec3 halfExtents = {0.2f, 0.2f, 0.2f};
        std::array<uint64_t, 4> found = {};
        CHECK(api.physics_overlap_sphere(&center, 0.2f, 0xFFFFFFFFu, 0, 0, found.data(), 4) == 0);
        CHECK(api.physics_overlap_box(&center, &halfExtents, nullptr, 0xFFFFFFFFu, 0, 0, found.data(), 4) == 0);
    }

    TEST_CASE("physics - shape_cast with INCLUDE_TRIGGERS reports is_trigger") {
        physics_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity ground = fixture.spawn_ground();
        const ecs::entity trigger = fixture.spawn_trigger(math::vec3(0.0f, 3.0f, 0.0f));
        fixture.run_steps(1);
        conjure_shape_query cast = {};
        cast.ray = fixture.down_from(10.0f);
        cast.ray.flags = static_cast<uint32_t>(conjure_query_flags::INCLUDE_TRIGGERS);
        cast.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        cast.radius = 0.25f;
        cast.half_extents = {0.25f, 0.25f, 0.25f};
        constexpr std::array<conjure_cast_shape, 3> SHAPES = {
          conjure_cast_shape::SPHERE, conjure_cast_shape::BOX, conjure_cast_shape::CAPSULE
        };
        for (const conjure_cast_shape shape : SHAPES) {
            CAPTURE(static_cast<int>(shape));
            cast.shape = static_cast<uint8_t>(shape);
            conjure_raycast_hit hit = {};
            REQUIRE(api.physics_shape_cast(&cast, &hit) == 1);
            CHECK(hit.entity == trigger.to_u64());
            CHECK(hit.is_trigger == 1);
        }

        // Below the trigger only the ground is left, which is no trigger.
        cast.ray.origin = {0.0f, 1.0f, 0.0f};
        cast.shape = static_cast<uint8_t>(conjure_cast_shape::SPHERE);
        conjure_raycast_hit hit = {};
        REQUIRE(api.physics_shape_cast(&cast, &hit) == 1);
        CHECK(hit.entity == ground.to_u64());
        CHECK(hit.is_trigger == 0);
    }

    TEST_CASE("physics - EXCLUDE_STATIC skips static bodies in every query") {
        physics_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity ground = fixture.spawn_ground();
        const ecs::entity mover = fixture.spawn_box(math::vec3(4.0f, 3.0f, 0.0f), physics::motion_type::KINEMATIC);
        fixture.run_steps(1);
        const uint32_t excludeStatic = static_cast<uint32_t>(conjure_query_flags::EXCLUDE_STATIC);

        conjure_ray_query query = fixture.down_from(10.0f);
        conjure_raycast_hit hit = {};
        REQUIRE(api.physics_raycast(&query, &hit) == 1);
        CHECK(hit.entity == ground.to_u64());
        query.flags = excludeStatic;
        CHECK(api.physics_raycast(&query, &hit) == 0);
        std::array<conjure_raycast_hit, 4> hits = {};
        CHECK(api.physics_raycast_all(&query, hits.data(), 4) == 0);
        conjure_shape_query cast = {};
        cast.ray = query;
        cast.shape = static_cast<uint8_t>(conjure_cast_shape::SPHERE);
        cast.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        cast.radius = 0.25f;
        CHECK(api.physics_shape_cast(&cast, &hit) == 0);

        // Above the kinematic box the ray still hits it.
        query.origin = {4.0f, 10.0f, 0.0f};
        REQUIRE(api.physics_raycast(&query, &hit) == 1);
        CHECK(hit.entity == mover.to_u64());

        const conjure_vec3 onGround = {0.0f, 0.0f, 0.0f};
        const conjure_vec3 onMover = {4.0f, 3.0f, 0.0f};
        const conjure_vec3 halfExtents = {0.2f, 0.2f, 0.2f};
        std::array<uint64_t, 4> found = {};
        CHECK(api.physics_overlap_sphere(&onGround, 0.2f, 0xFFFFFFFFu, 0, 0, found.data(), 4) == 1);
        CHECK(api.physics_overlap_sphere(&onGround, 0.2f, 0xFFFFFFFFu, excludeStatic, 0, found.data(), 4) == 0);
        CHECK(
          api.physics_overlap_box(&onGround, &halfExtents, nullptr, 0xFFFFFFFFu, excludeStatic, 0, found.data(), 4)
          == 0
        );
        CHECK(api.physics_overlap_sphere(&onMover, 0.2f, 0xFFFFFFFFu, excludeStatic, 0, found.data(), 4) == 1);
        CHECK(found[0] == mover.to_u64());
    }

    TEST_CASE("physics - the overlap entries leave the ignored entity out") {
        physics_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity left = fixture.spawn_box(math::vec3(-0.4f, 3.0f, 0.0f), physics::motion_type::KINEMATIC);
        const ecs::entity right = fixture.spawn_box(math::vec3(0.4f, 3.0f, 0.0f), physics::motion_type::KINEMATIC);
        fixture.run_steps(1);
        const conjure_vec3 center = {0.0f, 3.0f, 0.0f};
        const conjure_vec3 halfExtents = {0.2f, 0.2f, 0.2f};
        std::array<uint64_t, 4> found = {};
        CHECK(api.physics_overlap_sphere(&center, 0.2f, 0xFFFFFFFFu, 0, 0, found.data(), 4) == 2);
        found = {};
        CHECK(api.physics_overlap_sphere(&center, 0.2f, 0xFFFFFFFFu, 0, left.to_u64(), found.data(), 4) == 1);
        CHECK(found[0] == right.to_u64());
        found = {};
        CHECK(
          api.physics_overlap_box(&center, &halfExtents, nullptr, 0xFFFFFFFFu, 0, right.to_u64(), found.data(), 4)
          == 1
        );
        CHECK(found[0] == left.to_u64());
        // An ignored entity that is not alive changes nothing.
        CHECK(api.physics_overlap_sphere(&center, 0.2f, 0xFFFFFFFFu, 0, 0xFFFFFFFFFFull, found.data(), 4) == 2);
    }

    TEST_CASE("physics - an unknown query flag bit is INVALID_ARGUMENT in every query") {
        physics_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        static_cast<void>(fixture.spawn_ground());
        fixture.run_steps(1);
        constexpr std::array<uint32_t, 3> UNKNOWN = {4u, 0x80000000u, 4u | 1u};
        for (usize index = 0; index < UNKNOWN.size(); ++index) {
            CAPTURE(index);
            clear_last_error();
            conjure_ray_query query = fixture.down_from(10.0f);
            query.flags = UNKNOWN[index];
            conjure_raycast_hit hit = {};
            std::array<conjure_raycast_hit, 4> hits = {};
            CHECK(api.physics_raycast(&query, &hit) == to_status(bindings_error::INVALID_ARGUMENT));
            CHECK(last_error_message() == "physics_raycast: flags");
            CHECK(api.physics_raycast_all(&query, hits.data(), 4) == to_status(bindings_error::INVALID_ARGUMENT));
            conjure_shape_query cast = {};
            cast.ray = query;
            cast.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
            cast.radius = 0.25f;
            CHECK(api.physics_shape_cast(&cast, &hit) == to_status(bindings_error::INVALID_ARGUMENT));
            const conjure_vec3 center = {0.0f, 0.0f, 0.0f};
            const conjure_vec3 halfExtents = {0.2f, 0.2f, 0.2f};
            std::array<uint64_t, 4> found = {};
            CHECK(
              api.physics_overlap_sphere(&center, 0.2f, 0xFFFFFFFFu, UNKNOWN[index], 0, found.data(), 4)
              == to_status(bindings_error::INVALID_ARGUMENT)
            );
            CHECK(
              api.physics_overlap_box(&center, &halfExtents, nullptr, 0xFFFFFFFFu, UNKNOWN[index], 0, found.data(), 4)
              == to_status(bindings_error::INVALID_ARGUMENT)
            );
            CHECK(last_error_message() == "physics_overlap_box: flags");
        }
        clear_last_error();
    }

    TEST_CASE(
      "physics - the C# overloads without QueryFlags skip triggers, the ones with it reach them [dotnet]" *
      doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        managed_app fixture = {};
        scene::scene &active = fixture.app().context().scenes->active();
        ecs::world &world = *fixture.app().context().world;

        const auto spawn = [&](
          std::string_view name,
          const math::vec3 &position,
          const math::vec3 &halfExtents,
          bool trigger
        ) {
            const core::result<ecs::entity> created = active.create_entity(core::string_id(name));
            REQUIRE(created.has_value());
            scene::local_transform local = {};
            local.position = position;
            CONJURE_REQUIRE_OK(active.set_local_transform(created.value(), local));
            physics::rigid_body body = {};
            body.motionType = physics::motion_type::STATIC;
            CONJURE_REQUIRE_OK(world.add<physics::rigid_body>(created.value(), body));
            physics::box_collider collider = {};
            collider.halfExtents = halfExtents;
            collider.isTrigger = trigger;
            CONJURE_REQUIRE_OK(world.add<physics::box_collider>(created.value(), collider));
            return created.value();
        };
        const ecs::entity ground =
          spawn("ground", math::vec3(0.0f, -0.5f, 0.0f), math::vec3(50.0f, 0.5f, 50.0f), false);
        const ecs::entity trigger = spawn("trigger", math::vec3(0.0f, 3.0f, 0.0f), math::vec3(0.5f), true);
        CONJURE_REQUIRE_OK(fixture.app().step_frame());

        const scripting::script_entry_id probe = fixture.attach(fixture.create("probe"), QUERY_FLAGS_PROBE);
        core::result<core::json::document> collected = fixture.runtime().collect_entry_fields(probe);
        REQUIRE(collected.has_value());
        const core::json::value fields = collected.value().root();
        CHECK(fields["Ran"].as_i64() == 1);
        CHECK(static_cast<u64>(fields["DefaultHit"].as_i64()) == ground.to_u64());
        CHECK(fields["DefaultHitIsTrigger"].as_i64() == 0);
        CHECK(static_cast<u64>(fields["TriggerHit"].as_i64()) == trigger.to_u64());
        CHECK(fields["TriggerHitIsTrigger"].as_i64() == 1);
        CHECK(fields["DefaultCastHitIsTrigger"].as_i64() == 0);
        CHECK(fields["TriggerCastHitIsTrigger"].as_i64() == 1);
        CHECK(fields["DefaultOverlaps"].as_i64() == 0);
        CHECK(fields["TriggerOverlaps"].as_i64() == 1);
        CHECK(fields["IgnoredOverlaps"].as_i64() == 0);
        CHECK(fields["DefaultBoxOverlaps"].as_i64() == 0);
        CHECK(fields["TriggerBoxOverlaps"].as_i64() == 1);
        CHECK(fields["IgnoredBoxOverlaps"].as_i64() == 0);
        CHECK(fields["StaticExcludedOverlaps"].as_i64() == 0);
        CHECK(fields["UnknownFlagThrows"].as_string() == "ArgumentException");
        fixture.shutdown();
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE("physics - velocity set and get after a step") {
        physics_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity body = fixture.spawn_box(math::vec3(0.0f, 10.0f, 0.0f));
        fixture.run_steps(1);
        const conjure_vec3 velocity = {3.0f, 0.0f, 0.0f};
        CHECK(api.physics_set_linear_velocity(body.to_u64(), &velocity) == 0);
        conjure_vec3 read = {};
        CHECK(api.physics_get_linear_velocity(body.to_u64(), &read) == 0);
        CHECK(read.x == 3.0f);                                        // the mirror answers until the pull
        fixture.run_steps(1);
        CHECK(api.physics_get_linear_velocity(body.to_u64(), &read) == 0);
        CHECK(read.x == doctest::Approx(3.0f).epsilon(0.05));
        CHECK(read.y < 0.0f);                                         // gravity acted for one step
        const conjure_vec3 spin = {0.0f, 2.0f, 0.0f};
        CHECK(api.physics_set_angular_velocity(body.to_u64(), &spin) == 0);
        fixture.run_steps(1);
        CHECK(api.physics_get_angular_velocity(body.to_u64(), &read) == 0);
        CHECK(read.y == doctest::Approx(2.0f).epsilon(0.1));
        CHECK(api.physics_get_linear_velocity(body.to_u64(), nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.physics_set_linear_velocity(body.to_u64(), nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.physics_get_angular_velocity(999, &read) == to_status(bindings_error::ENTITY_NOT_ALIVE));
    }

    TEST_CASE("physics - a refused velocity write leaves the mirror unchanged") {
        log_capture logs = {};
        physics_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity body = fixture.spawn_box(math::vec3(0.0f, 10.0f, 0.0f));
        fixture.run_steps(1);
        conjure_vec3 linear = {};
        conjure_vec3 angular = {};
        REQUIRE(api.physics_get_linear_velocity(body.to_u64(), &linear) == 0);
        REQUIRE(api.physics_get_angular_velocity(body.to_u64(), &angular) == 0);
        conjure_vec3 read = {};

        // A non-finite value is refused by physics_world before it queues anything.
        const conjure_vec3 poisoned = {std::numeric_limits<f32>::quiet_NaN(), 0.0f, 0.0f};
        CHECK(
          api.physics_set_linear_velocity(body.to_u64(), &poisoned) == to_status(bindings_error::INVALID_ARGUMENT)
        );
        CHECK(
          api.physics_set_angular_velocity(body.to_u64(), &poisoned) == to_status(bindings_error::INVALID_ARGUMENT)
        );
        REQUIRE(api.physics_get_linear_velocity(body.to_u64(), &read) == 0);
        CHECK(std::isfinite(read.x));
        CHECK(read.x == linear.x);

        // A full command queue (the fixture's maxQueuedCommands is 256) refuses with COMMAND_QUEUE_FULL (INTERNAL).
        const conjure_vec3 nudge = {0.0f, 0.0f, 1.0f};
        for (u32 index = 0; index < 256; ++index) {
            REQUIRE(
              api.physics_add_force(body.to_u64(), &nudge, static_cast<uint8_t>(conjure_force_mode::FORCE)) == 0
            );
        }
        const conjure_vec3 velocity = {3.0f, 0.0f, 0.0f};
        CHECK(api.physics_set_linear_velocity(body.to_u64(), &velocity) == to_status(bindings_error::INTERNAL));
        CHECK(api.physics_set_angular_velocity(body.to_u64(), &velocity) == to_status(bindings_error::INTERNAL));
        REQUIRE(api.physics_get_linear_velocity(body.to_u64(), &read) == 0);
        CHECK(read.x == linear.x);
        CHECK(read.y == linear.y);
        CHECK(read.z == linear.z);
        REQUIRE(api.physics_get_angular_velocity(body.to_u64(), &read) == 0);
        CHECK(read.x == angular.x);
        CHECK(read.y == angular.y);
        CHECK(read.z == angular.z);
        CHECK(logs.contains(core::log_level::WARN, "command queue full"));
    }

    TEST_CASE("physics - force modes change the velocity as expected") {
        physics_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity body = fixture.spawn_box(math::vec3(0.0f, 50.0f, 0.0f));
        fixture.base.world.get<physics::rigid_body>(body)->mass = 2.0f;
        fixture.base.world.get<physics::rigid_body>(body)->gravityFactor = 0.0f;
        fixture.run_steps(1);
        conjure_vec3 read = {};
        const conjure_vec3 push = {4.0f, 0.0f, 0.0f};
        CHECK(
          api.physics_add_force(body.to_u64(), &push, static_cast<uint8_t>(conjure_force_mode::VELOCITY_CHANGE)) == 0
        );
        fixture.run_steps(1);
        REQUIRE(api.physics_get_linear_velocity(body.to_u64(), &read) == 0);
        CHECK(read.x == doctest::Approx(4.0f).epsilon(0.05));
        const conjure_vec3 impulse = {2.0f, 0.0f, 0.0f};              // 2 Ns on 2 kg: +1 m/s
        CHECK(api.physics_add_force(body.to_u64(), &impulse, static_cast<uint8_t>(conjure_force_mode::IMPULSE)) == 0);
        fixture.run_steps(1);
        REQUIRE(api.physics_get_linear_velocity(body.to_u64(), &read) == 0);
        CHECK(read.x == doctest::Approx(5.0f).epsilon(0.05));
        const conjure_vec3 acceleration = {0.0f, 60.0f, 0.0f};        // 60 m/s^2 for one step: +1 m/s
        CHECK(
          api.physics_add_force(body.to_u64(), &acceleration, static_cast<uint8_t>(conjure_force_mode::ACCELERATION))
          == 0
        );
        fixture.run_steps(1);
        REQUIRE(api.physics_get_linear_velocity(body.to_u64(), &read) == 0);
        CHECK(read.y == doctest::Approx(1.0f).epsilon(0.1));
        const conjure_vec3 force = {0.0f, 0.0f, 120.0f};              // 120 N on 2 kg for one step: +1 m/s
        CHECK(api.physics_add_force(body.to_u64(), &force, static_cast<uint8_t>(conjure_force_mode::FORCE)) == 0);
        fixture.run_steps(1);
        REQUIRE(api.physics_get_linear_velocity(body.to_u64(), &read) == 0);
        CHECK(read.z == doctest::Approx(1.0f).epsilon(0.1));
        const conjure_vec3 point = {0.0f, 50.0f, 0.5f};
        CHECK(
          api.physics_add_force_at(body.to_u64(), &force, &point, static_cast<uint8_t>(conjure_force_mode::FORCE)) == 0
        );
        const conjure_vec3 torque = {0.0f, 1.0f, 0.0f};
        CHECK(api.physics_add_torque(body.to_u64(), &torque, static_cast<uint8_t>(conjure_force_mode::IMPULSE)) == 0);
        CHECK(api.physics_add_torque(body.to_u64(), &torque, static_cast<uint8_t>(conjure_force_mode::FORCE)) == 0);
        fixture.run_steps(1);
        REQUIRE(api.physics_get_angular_velocity(body.to_u64(), &read) == 0);
        CHECK(read.y > 0.0f);
        // Failure paths: bad mode, null force, missing body, derived mass.
        CHECK(api.physics_add_force(body.to_u64(), &force, 9) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.physics_add_force(body.to_u64(), nullptr, 0) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(
          api.physics_add_force_at(body.to_u64(), &force, nullptr, 0) == to_status(bindings_error::INVALID_ARGUMENT)
        );
        CHECK(
          api.physics_add_torque(body.to_u64(), &torque, static_cast<uint8_t>(conjure_force_mode::ACCELERATION))
          == to_status(bindings_error::INVALID_ARGUMENT)
        );
        const ecs::entity plain = fixture.base.create("plain");
        CHECK(api.physics_add_force(plain.to_u64(), &force, 0) == to_status(bindings_error::COMPONENT_MISSING));
        fixture.base.world.get<physics::rigid_body>(body)->mass = 0.0f;
        CHECK(
          api.physics_add_force(body.to_u64(), &push, static_cast<uint8_t>(conjure_force_mode::VELOCITY_CHANGE))
          == to_status(bindings_error::INVALID_ARGUMENT)
        );
    }

    TEST_CASE("physics - move_kinematic is applied by push_transforms") {
        physics_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity platform = fixture.spawn_box(math::vec3(0.0f, 1.0f, 0.0f), physics::motion_type::KINEMATIC);
        fixture.run_steps(1);
        conjure_transform target = {};
        target.position = {0.0f, 2.0f, 0.0f};
        target.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        target.scale = {1.0f, 1.0f, 1.0f};
        CHECK(api.physics_move_kinematic(platform.to_u64(), &target) == 0);
        fixture.run_steps(1);                                             // MoveKinematic reaches the pose in one step
        const std::optional<math::transform> pose = fixture.base.physics->body_pose(platform);
        REQUIRE(pose.has_value());
        CHECK(pose->position.y == doctest::Approx(2.0f).epsilon(0.01));
        CHECK(api.physics_move_kinematic(platform.to_u64(), nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
        const ecs::entity plain = fixture.base.create("plain");
        CHECK(api.physics_move_kinematic(plain.to_u64(), &target) == to_status(bindings_error::COMPONENT_MISSING));
    }

    TEST_CASE("physics - a command the kind of body cannot take is INVALID_ARGUMENT and changes nothing") {
        // physics_world refuses these at the call with WRONG_BODY_KIND (code sweep F413); the caller passed an
        // entity the command does not apply to, which is INVALID_ARGUMENT (ArgumentException in C#).
        CHECK(
          map_error(core::make_error(physics::physics_error::WRONG_BODY_KIND)) == bindings_error::INVALID_ARGUMENT
        );
        physics_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity wall = fixture.spawn_box(math::vec3(0.0f, 1.0f, 0.0f), physics::motion_type::STATIC);
        const ecs::entity crate = fixture.spawn_box(math::vec3(5.0f, 1.0f, 0.0f));
        fixture.run_steps(1);
        const int32_t refused = to_status(bindings_error::INVALID_ARGUMENT);
        const conjure_vec3 push = {3.0f, 0.0f, 0.0f};
        const uint8_t force = static_cast<uint8_t>(conjure_force_mode::FORCE);
        const uint8_t impulse = static_cast<uint8_t>(conjure_force_mode::IMPULSE);
        conjure_transform target = {};
        target.position = {0.0f, 2.0f, 0.0f};
        target.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        target.scale = {1.0f, 1.0f, 1.0f};

        // A static body takes no force, torque, velocity or kinematic move.
        CHECK(api.physics_add_force(wall.to_u64(), &push, force) == refused);
        CHECK(last_error_code() == bindings_error::INVALID_ARGUMENT);
        CHECK(last_error_message().starts_with("physics_add_force: physics.WRONG_BODY_KIND: add_force"));
        CHECK(api.physics_add_force(wall.to_u64(), &push, impulse) == refused);
        CHECK(api.physics_add_force_at(wall.to_u64(), &push, &push, force) == refused);
        CHECK(api.physics_add_torque(wall.to_u64(), &push, force) == refused);
        CHECK(api.physics_add_torque(wall.to_u64(), &push, impulse) == refused);
        CHECK(api.physics_move_kinematic(wall.to_u64(), &target) == refused);
        CHECK(api.physics_set_linear_velocity(wall.to_u64(), &push) == refused);
        CHECK(api.physics_set_angular_velocity(wall.to_u64(), &push) == refused);
        // The refused velocity writes left the rigid_body mirror alone.
        conjure_vec3 read = {1.0f, 1.0f, 1.0f};
        REQUIRE(api.physics_get_linear_velocity(wall.to_u64(), &read) == 0);
        CHECK(read.x == 0.0f);
        REQUIRE(api.physics_get_angular_velocity(wall.to_u64(), &read) == 0);
        CHECK(read.x == 0.0f);
        // It can still be teleported, and wake is a successful no-op on it.
        CHECK(api.physics_teleport(wall.to_u64(), &target) == 0);
        CHECK(api.physics_wake(wall.to_u64()) == 0);

        // A dynamic body takes everything but the kinematic move.
        CHECK(api.physics_move_kinematic(crate.to_u64(), &target) == refused);
        CHECK(last_error_message().starts_with("physics_move_kinematic: physics.WRONG_BODY_KIND: move_kinematic"));
        CHECK(api.physics_add_force(crate.to_u64(), &push, force) == 0);

        fixture.run_steps(1);
        CHECK(fixture.base.physics->stats().droppedCommands == 0);
        const std::optional<math::transform> pose = fixture.base.physics->body_pose(wall);
        REQUIRE(pose.has_value());
        CHECK(pose->position.y == doctest::Approx(2.0f));
        clear_last_error();
    }

    TEST_CASE("physics - teleport resets the velocity") {
        physics_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity body = fixture.spawn_box(math::vec3(0.0f, 20.0f, 0.0f));
        fixture.run_steps(10);
        conjure_vec3 velocity = {};
        REQUIRE(api.physics_get_linear_velocity(body.to_u64(), &velocity) == 0);
        CHECK(velocity.y < -1.0f);
        conjure_transform target = {};
        target.position = {5.0f, 30.0f, 0.0f};
        target.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        target.scale = {1.0f, 1.0f, 1.0f};
        CHECK(api.physics_teleport(body.to_u64(), &target) == 0);
        fixture.run_steps(1);
        const std::optional<math::transform> pose = fixture.base.physics->body_pose(body);
        REQUIRE(pose.has_value());
        CHECK(pose->position.x == doctest::Approx(5.0f).epsilon(0.01));
        CHECK(pose->position.y == doctest::Approx(30.0f).epsilon(0.02));
        REQUIRE(api.physics_get_linear_velocity(body.to_u64(), &velocity) == 0);
        CHECK(std::abs(velocity.y) < 0.5f);                                // one step of gravity at most
        CHECK(api.physics_teleport(body.to_u64(), nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.physics_teleport(999, &target) == to_status(bindings_error::ENTITY_NOT_ALIVE));
    }

    TEST_CASE("physics - sleep and wake") {
        physics_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        static_cast<void>(fixture.spawn_ground());
        const ecs::entity body = fixture.spawn_box(math::vec3(0.0f, 0.5f, 0.0f));
        fixture.run_steps(120);
        CHECK(api.physics_is_sleeping(body.to_u64()) == 1);
        CHECK(api.physics_wake(body.to_u64()) == 0);
        fixture.run_steps(1);
        CHECK(api.physics_is_sleeping(body.to_u64()) == 0);
        const ecs::entity plain = fixture.base.create("plain");
        CHECK(api.physics_is_sleeping(plain.to_u64()) == to_status(bindings_error::COMPONENT_MISSING));
        CHECK(api.physics_wake(plain.to_u64()) == to_status(bindings_error::COMPONENT_MISSING));
        CHECK(api.physics_is_sleeping(999) == to_status(bindings_error::ENTITY_NOT_ALIVE));
    }

    TEST_CASE("physics - gravity roundtrip") {
        physics_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        conjure_vec3 gravity = {};
        CHECK(api.physics_get_gravity(&gravity) == 0);
        CHECK(gravity.y == doctest::Approx(-9.81f));
        const conjure_vec3 moon = {0.0f, -1.62f, 0.0f};
        CHECK(api.physics_set_gravity(&moon) == 0);
        fixture.run_steps(1);                                             // queued until the push
        CHECK(api.physics_get_gravity(&gravity) == 0);
        CHECK(gravity.y == doctest::Approx(-1.62f));
        CHECK(api.physics_get_gravity(nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.physics_set_gravity(nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
    }

    TEST_CASE("physics - layer index, name and collide come from the layer table") {
        physics_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const utf8_argument defaultName = utf8("default");
        CHECK(api.physics_layer_index(defaultName.data, defaultName.length) == 0);
        const utf8_argument player = utf8("player");
        CHECK(api.physics_layer_index(player.data, player.length) == 1);
        const std::string second = read_string([&](char *buffer, int32_t capacity) {
            return api.physics_layer_name(1, buffer, capacity);
        });
        CHECK(second == "player");
        CHECK(api.physics_layers_collide(0, 1) == 1);
        CHECK(api.physics_layers_collide(1, 1) == 0);                   // the fixture's table ignores player/player
        const utf8_argument unknown = utf8("ghost");
        CHECK(api.physics_layer_index(unknown.data, unknown.length) == to_status(bindings_error::NOT_FOUND));
        CHECK(core::string_id::from_hash(core::hash::fnv1a64("ghost")).view().empty());   // a lookup never interns
        CHECK(api.physics_layer_index(nullptr, 3) == to_status(bindings_error::INVALID_ARGUMENT));
        const std::string name = read_string([&](char *buffer, int32_t capacity) {
            return api.physics_layer_name(0, buffer, capacity);
        });
        CHECK(name == "default");
        CHECK(api.physics_layer_name(0, nullptr, 0) == 7);
        CHECK(api.physics_layer_name(5, nullptr, 0) == to_status(bindings_error::NOT_FOUND));
        CHECK(api.physics_layer_name(-1, nullptr, 0) == to_status(bindings_error::NOT_FOUND));
        CHECK(api.physics_layer_name(0, nullptr, -1) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(api.physics_layers_collide(0, 0) == 1);
        // An unknown layer is NOT_FOUND in every layer entry (15_bindings.md 2.2), as in physics_layer_name.
        CHECK(api.physics_layers_collide(0, 3) == to_status(bindings_error::NOT_FOUND));
        CHECK(api.physics_layers_collide(-1, 0) == to_status(bindings_error::NOT_FOUND));
    }

    TEST_CASE("physics - character move and grounded") {
        physics_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        static_cast<void>(fixture.spawn_ground());
        const ecs::entity character = fixture.spawn_character(math::vec3(0.0f, 0.05f, 0.0f));
        fixture.run_steps(30);
        CHECK(api.physics_character_is_grounded(character.to_u64()) == 1);
        const conjure_vec3 walk = {2.0f, 0.0f, 0.0f};
        CHECK(api.physics_character_move(character.to_u64(), &walk) == 0);
        conjure_vec3 velocity = {};
        CHECK(api.physics_character_velocity(character.to_u64(), &velocity) == 0);
        CHECK(velocity.x == 2.0f);
        fixture.run_steps(10);
        const std::optional<math::transform> pose = fixture.base.physics->body_pose(character);
        REQUIRE(pose.has_value());
        CHECK(pose->position.x > 0.1f);
        const ecs::entity plain = fixture.base.create("plain");
        CHECK(api.physics_character_move(plain.to_u64(), &walk) == to_status(bindings_error::COMPONENT_MISSING));
        CHECK(api.physics_character_is_grounded(plain.to_u64()) == to_status(bindings_error::COMPONENT_MISSING));
        CHECK(
          api.physics_character_velocity(plain.to_u64(), &velocity) == to_status(bindings_error::COMPONENT_MISSING)
        );
        CHECK(api.physics_character_move(character.to_u64(), nullptr) == to_status(bindings_error::INVALID_ARGUMENT));
        CHECK(
          api.physics_character_velocity(character.to_u64(), nullptr) == to_status(bindings_error::INVALID_ARGUMENT)
        );
    }

    TEST_CASE("physics - a refused character_move leaves the controller velocity unchanged") {
        physics_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        static_cast<void>(fixture.spawn_ground());
        const ecs::entity character = fixture.spawn_character(math::vec3(0.0f, 0.05f, 0.0f));
        const ecs::entity box = fixture.spawn_box(math::vec3(5.0f, 10.0f, 0.0f));
        fixture.run_steps(30);
        const physics::character_controller *controller =
          fixture.base.world.get<physics::character_controller>(character);
        REQUIRE(controller != nullptr);
        const math::vec3 before = controller->velocity;
        REQUIRE(std::isfinite(before.x));

        // A non-finite velocity is refused by physics_world before it queues anything. No move is queued, so the
        // step reads the component velocity: a NaN left there would reach Jolt.
        const conjure_vec3 poisoned = {std::numeric_limits<f32>::quiet_NaN(), 0.0f, 0.0f};
        CHECK(
          api.physics_character_move(character.to_u64(), &poisoned) == to_status(bindings_error::INVALID_ARGUMENT)
        );
        controller = fixture.base.world.get<physics::character_controller>(character);
        CHECK(controller->velocity.x == before.x);
        CHECK(controller->velocity.y == before.y);
        CHECK(controller->velocity.z == before.z);
        fixture.run_steps(1);
        controller = fixture.base.world.get<physics::character_controller>(character);
        CHECK(std::isfinite(controller->velocity.x));
        CHECK(std::isfinite(controller->velocity.y));
        CHECK(std::isfinite(controller->velocity.z));
        const std::optional<math::transform> pose = fixture.base.physics->body_pose(character);
        REQUIRE(pose.has_value());
        CHECK(std::isfinite(pose->position.x));
        CHECK(std::isfinite(pose->position.y));

        // A full command queue (maxQueuedCommands is 256) refuses with COMMAND_QUEUE_FULL (INTERNAL).
        const math::vec3 settled = controller->velocity;
        const conjure_vec3 nudge = {0.0f, 0.0f, 1.0f};
        for (u32 index = 0; index < 256; ++index) {
            REQUIRE(api.physics_add_force(box.to_u64(), &nudge, static_cast<uint8_t>(conjure_force_mode::FORCE)) == 0);
        }
        const conjure_vec3 walk = {2.0f, 0.0f, 0.0f};
        CHECK(api.physics_character_move(character.to_u64(), &walk) == to_status(bindings_error::INTERNAL));
        controller = fixture.base.world.get<physics::character_controller>(character);
        CHECK(controller->velocity.x == settled.x);
        CHECK(controller->velocity.y == settled.y);
        CHECK(controller->velocity.z == settled.z);
    }

    TEST_CASE("physics - an entity without a body is COMPONENT_MISSING") {
        physics_fixture fixture = {};
        conjure_native_api &api = fixture.base.api();
        const ecs::entity plain = fixture.base.create("plain");
        conjure_vec3 value = {};
        CHECK(api.physics_get_linear_velocity(plain.to_u64(), &value) == to_status(bindings_error::COMPONENT_MISSING));
        CHECK(last_error_message() == "physics_get_linear_velocity: rigid_body");
        CHECK(api.physics_set_linear_velocity(plain.to_u64(), &value) == to_status(bindings_error::COMPONENT_MISSING));
        CHECK(api.physics_set_angular_velocity(plain.to_u64(), &value) == to_status(bindings_error::COMPONENT_MISSING));
        conjure_transform target = {};
        CHECK(api.physics_teleport(plain.to_u64(), &target) == to_status(bindings_error::COMPONENT_MISSING));
    }

    TEST_CASE("physics - without a physics world every entry is NOT_SUPPORTED") {
        bare_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const ecs::entity plain = fixture.create("plain");
        conjure_vec3 value = {};
        const conjure_ray_query query = {};
        conjure_raycast_hit hit = {};
        CHECK(api.physics_raycast(&query, &hit) == to_status(bindings_error::NOT_SUPPORTED));
        CHECK(api.physics_get_gravity(&value) == to_status(bindings_error::NOT_SUPPORTED));
        CHECK(api.physics_get_linear_velocity(plain.to_u64(), &value) == to_status(bindings_error::NOT_SUPPORTED));
        CHECK(api.physics_layers_collide(0, 0) == to_status(bindings_error::NOT_SUPPORTED));
        CHECK(api.physics_character_is_grounded(plain.to_u64()) == to_status(bindings_error::NOT_SUPPORTED));
        CHECK(last_error_message() == "physics_character_is_grounded: subsystem unavailable");
    }
}
