#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"
#include "bindings_test_support.h"

#include "conjure/bindings/bindings_error.h"
#include "conjure/bindings/call_counters.h"
#include "conjure/bindings/native_api_entries.h"
#include "conjure/core/math.h"
#include "conjure/core/reflect/type_registry.h"
#include "conjure/engine/main_camera.h"
#include "conjure/platform/null_backend.h"
#include "conjure/renderer/camera.h"
#include "conjure/renderer/components.h"
#include "conjure/scene/components.h"
#include "conjure/scene/scene.h"
#include "engine/subsystems/renderer_components.h"

#include <array>
#include <cmath>
#include <format>
#include <limits>

// camera_* (wave 3 GAP-CSHARP 2.2, 8 L3): camera_main follows engine::find_main_camera; the picking entries map
// game screen pixels (the 1280 x 720 null window, origin top-left, y down) and depth along the forward axis to the
// world and back through the camera's current pose; a zero screen and a point behind the camera answer 0 with the
// out values written; the refusals and the stubs of a context without the world and scenes.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::testing;
using namespace bindings_test;

namespace {

    constexpr f32 SCREEN_WIDTH = 1280.0f;
    constexpr f32 SCREEN_HEIGHT = 720.0f;
    constexpr f32 ROUND_TRIP_TOLERANCE = 1.0e-4f;
    // A world point 0.1 m in front of a camera 5 m from the origin carries float rounding of about 5e-7 m, about
    // 1e-3 px once projected: the pixel side of a round trip is compared to a two-hundredth of a pixel.
    constexpr f32 PIXEL_TOLERANCE = 5.0e-3f;

    /// <summary>
    /// bare_fixture (the null window included) with the renderer's camera component registered, as the engine's
    /// renderer subsystem registers it.
    /// </summary>
    struct camera_fixture {
        bare_fixture base;      // no default: built from the options in the constructor

        camera_fixture() :
            camera_fixture(bare_fixture_options{}) {}

        explicit camera_fixture(const bare_fixture_options &options) :
            base(options) {
            CONJURE_REQUIRE_OK(renderer::register_components(core::reflect::registry()));
            CONJURE_REQUIRE_OK(base.registry.register_component<renderer::camera_component>());
        }

        /// <summary>
        /// A scene entity at 'position' turned by 'rotation' carrying 'camera'.
        /// </summary>
        [[nodiscard]] ecs::entity add_camera(
          std::string_view name,
          const renderer::camera_component &camera,
          math::vec3 position = math::vec3(0.0f),
          math::quat rotation = math::quat(1.0f, 0.0f, 0.0f, 0.0f)
        ) {
            const ecs::entity entity = base.create(name);
            scene::local_transform pose = {};
            pose.position = position;
            pose.rotation = rotation;
            CONJURE_REQUIRE_OK(base.active().set_local_transform(entity, pose));
            CONJURE_REQUIRE_OK(base.world.add<renderer::camera_component>(entity, camera));
            return entity;
        }

        [[nodiscard]] conjure_native_api &api() noexcept {
            return base.api();
        }
    };

    [[nodiscard]] renderer::camera_component orthographic(f32 height) noexcept {
        renderer::camera_component camera = {};
        camera.cameraData.projection = renderer::projection_type::ORTHOGRAPHIC;
        camera.cameraData.orthographicHeight = height;
        camera.cameraData.farPlane = 100.0f;
        return camera;
    }

    [[nodiscard]] math::vec3 to_math(const conjure_vec3 &value) noexcept {
        return math::vec3(value.x, value.y, value.z);
    }

    [[nodiscard]] math::quat turn_about_y(f32 radians) noexcept {
        return glm::angleAxis(radians, math::vec3(0.0f, 1.0f, 0.0f));
    }

    /// <summary>
    /// camera_screen_to_world of (x, y, depth); REQUIREs the status 'expected'.
    /// </summary>
    [[nodiscard]] math::vec3 screen_to_world(
      conjure_native_api &api,
      ecs::entity camera,
      f32 x,
      f32 y,
      f32 depth,
      int32_t expected = 1
    ) {
        const conjure_vec3 input = {x, y, depth};
        conjure_vec3 world = {};
        REQUIRE(api.camera_screen_to_world(camera.to_u64(), &input, &world) == expected);
        return to_math(world);
    }

    /// <summary>
    /// camera_world_to_screen of 'world'; REQUIREs the status 'expected'.
    /// </summary>
    [[nodiscard]] math::vec3 world_to_screen(
      conjure_native_api &api,
      ecs::entity camera,
      math::vec3 world,
      int32_t expected = 1
    ) {
        const conjure_vec3 input = {world.x, world.y, world.z};
        conjure_vec3 screen = {};
        REQUIRE(api.camera_world_to_screen(camera.to_u64(), &input, &screen) == expected);
        return to_math(screen);
    }
}

TEST_SUITE("bindings/camera_bindings") {

    TEST_CASE("camera - camera_main follows the main-camera rule") {
        camera_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        CHECK(api.camera_main() == 0);
        CHECK(last_error_code() == bindings_error::NONE);

        renderer::camera_component other = {};
        other.isMain = false;
        other.priority = 5;
        const ecs::entity side = fixture.add_camera("side", other);
        CHECK(api.camera_main() == side.to_u64());

        const ecs::entity main = fixture.add_camera("main", renderer::camera_component{});
        CHECK(api.camera_main() == main.to_u64());
        CHECK(api.camera_main() == engine::find_main_camera(fixture.base.world).to_u64());

        CONJURE_REQUIRE_OK(fixture.base.active().set_enabled(main, false));
        CHECK(api.camera_main() == side.to_u64());
        CHECK(call_count(native_function_id::camera_main) == 5);
    }

    TEST_CASE("camera - the centre ray of a perspective camera is its forward axis from the near plane") {
        camera_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const math::vec3 eye = math::vec3(1.0f, 2.0f, 3.0f);
        const ecs::entity camera =
          fixture.add_camera("eye", renderer::camera_component{}, eye, turn_about_y(math::PI * 0.5f));
        const conjure_vec2 centre = {SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f};
        conjure_vec3 origin = {};
        conjure_vec3 direction = {};
        CHECK(api.camera_screen_ray(camera.to_u64(), &centre, &origin, &direction) == 1);
        const math::vec3 forward = math::vec3(-1.0f, 0.0f, 0.0f);
        CONJURE_CHECK_APPROX(to_math(direction), forward);
        CONJURE_CHECK_APPROX(to_math(origin), eye + forward * renderer::camera{}.nearPlane);

        // Off centre the ray still passes through the eye and leans towards the pixel (up-left of the centre).
        const conjure_vec2 corner = {0.0f, 0.0f};
        CHECK(api.camera_screen_ray(camera.to_u64(), &corner, &origin, &direction) == 1);
        const math::vec3 along = to_math(direction);
        const math::vec3 back = to_math(origin) - along * glm::dot(to_math(origin) - eye, along);
        CHECK(testing::approx_equal(back, eye, ROUND_TRIP_TOLERANCE));
        CHECK(direction.y > 0.0f);
        CHECK(direction.z > 0.0f);     // the camera's left is +z after the turn
        CHECK(std::abs(glm::length(to_math(direction)) - 1.0f) < ROUND_TRIP_TOLERANCE);
    }

    TEST_CASE("camera - orthographic corners span the orthographic height times the aspect ratio") {
        camera_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const ecs::entity camera = fixture.add_camera("flat", orthographic(10.0f), math::vec3(0.0f, 0.0f, 10.0f));
        const f32 halfHeight = 5.0f;
        const f32 halfWidth = halfHeight * SCREEN_WIDTH / SCREEN_HEIGHT;
        CONJURE_CHECK_APPROX(screen_to_world(api, camera, 0.0f, 0.0f, 10.0f), math::vec3(-halfWidth, halfHeight, 0.0f));
        CONJURE_CHECK_APPROX(
          screen_to_world(api, camera, SCREEN_WIDTH, SCREEN_HEIGHT, 10.0f),
          math::vec3(halfWidth, -halfHeight, 0.0f)
        );
        // Depth moves along the forward axis only: an orthographic pixel keeps its x and y.
        CONJURE_CHECK_APPROX(screen_to_world(api, camera, 0.0f, 0.0f, 2.0f), math::vec3(-halfWidth, halfHeight, 8.0f));

        const conjure_vec2 corner = {SCREEN_WIDTH, 0.0f};
        conjure_vec3 origin = {};
        conjure_vec3 direction = {};
        CHECK(api.camera_screen_ray(camera.to_u64(), &corner, &origin, &direction) == 1);
        CONJURE_CHECK_APPROX(to_math(origin), math::vec3(halfWidth, halfHeight, 9.9f));
        CONJURE_CHECK_APPROX(to_math(direction), math::vec3(0.0f, 0.0f, -1.0f));
    }

    TEST_CASE("camera - screen_to_world and world_to_screen round trip within 1e-4") {
        camera_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const math::vec3 eye = math::vec3(-3.0f, 1.5f, 4.0f);
        const math::quat turn = turn_about_y(0.7f) * glm::angleAxis(-0.3f, math::vec3(1.0f, 0.0f, 0.0f));
        renderer::camera_component camera = {};
        SUBCASE("perspective") {
            camera.cameraData.verticalFovRadians = 0.9f;
        }
        SUBCASE("orthographic") {
            camera = orthographic(7.0f);
        }
        const ecs::entity entity = fixture.add_camera("camera", camera, eye, turn);
        const std::array<math::vec3, 5> samples = {
          math::vec3(640.0f, 360.0f, 5.0f),
          math::vec3(0.0f, 0.0f, 0.1f),
          math::vec3(1280.0f, 720.0f, 50.0f),
          math::vec3(-200.0f, 900.0f, 12.5f),    // off the screen is still a point
          math::vec3(321.25f, 17.75f, 0.75f),
        };
        for (usize index = 0; index < samples.size(); ++index) {
            CAPTURE(index);
            const math::vec3 &sample = samples[index];
            const math::vec3 world = screen_to_world(api, entity, sample.x, sample.y, sample.z);
            const math::vec3 back = world_to_screen(api, entity, world);
            CHECK_MESSAGE(testing::approx_equal(back, sample, PIXEL_TOLERANCE), std::format("{} -> {}", sample, back));
            const math::vec3 again = screen_to_world(api, entity, back.x, back.y, back.z);
            const bool same = testing::approx_equal(again, world, ROUND_TRIP_TOLERANCE);
            CHECK_MESSAGE(same, std::format("{} -> {}", world, again));
        }
    }

    TEST_CASE("camera - a point behind the camera gives 0 with its pixel and negative depth written") {
        camera_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const math::vec2 centre = math::vec2(SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f);
        // Each camera looks down -z from the origin: +z is behind it, the camera plane is z = 0.
        SUBCASE("perspective") {
            const ecs::entity camera = fixture.add_camera("camera", renderer::camera_component{});
            const math::vec3 behind = world_to_screen(api, camera, math::vec3(0.0f, 0.0f, 4.0f), 0);
            CHECK(behind.z == doctest::Approx(-4.0f));
            CONJURE_CHECK_APPROX(math::vec2(behind), centre);
            // Off the axis, the division by a negative depth mirrors the pixel through the screen centre.
            const math::vec3 offAxis = world_to_screen(api, camera, math::vec3(1.0f, 1.0f, 4.0f), 0);
            CHECK(offAxis.z == doctest::Approx(-4.0f));
            CHECK(offAxis.x < centre.x);
            CHECK(offAxis.y > centre.y);
            const math::vec3 mirrored = world_to_screen(api, camera, math::vec3(-1.0f, -1.0f, -4.0f));
            CONJURE_CHECK_APPROX(math::vec2(offAxis), math::vec2(mirrored));
            const math::vec3 onPlane = world_to_screen(api, camera, math::vec3(1.0f, 0.0f, 0.0f), 0);
            CHECK(onPlane.z == doctest::Approx(0.0f));
            CONJURE_CHECK_APPROX(math::vec2(onPlane), centre);
            // Within EPSILON of the plane but in front of the eye: still the centre, so still 0, not a projection.
            const math::vec3 nearPlane = world_to_screen(api, camera, math::vec3(1.0f, 0.0f, -5e-7f), 0);
            CHECK(nearPlane.z > 0.0f);
            CONJURE_CHECK_APPROX(math::vec2(nearPlane), centre);
            const math::vec3 ahead = world_to_screen(api, camera, math::vec3(0.0f, 0.0f, -4.0f));
            CHECK(ahead.z == doctest::Approx(4.0f));
        }
        SUBCASE("orthographic") {
            const ecs::entity camera = fixture.add_camera("camera", orthographic(10.0f));
            // An orthographic pixel does not depend on the depth, so a point behind keeps the pixel it has ahead.
            const math::vec3 behind = world_to_screen(api, camera, math::vec3(1.0f, 1.0f, 4.0f), 0);
            CHECK(behind.z == doctest::Approx(-4.0f));
            const math::vec3 ahead = world_to_screen(api, camera, math::vec3(1.0f, 1.0f, -4.0f));
            CHECK(ahead.z == doctest::Approx(4.0f));
            CONJURE_CHECK_APPROX(math::vec2(behind), math::vec2(ahead));
            CHECK(ahead.x > centre.x);
            CHECK(ahead.y < centre.y);
            // depth <= 0 gives 0 for an orthographic camera too, the camera plane included.
            const math::vec3 onPlane = world_to_screen(api, camera, math::vec3(1.0f, 1.0f, 0.0f), 0);
            CHECK(onPlane.z == doctest::Approx(0.0f));
            CONJURE_CHECK_APPROX(math::vec2(onPlane), math::vec2(ahead));
        }
    }

    TEST_CASE("camera - a screen without area gives 0 with the camera's own pose written") {
        bare_fixture_options options = {};
        bool minimise = false;
        SUBCASE("minimised window") {
            minimise = true;
        }
        SUBCASE("no window") {
            options.window = false;
        }
        CAPTURE(minimise);
        camera_fixture fixture(options);
        conjure_native_api &api = fixture.api();
        if (minimise) {
            platform::null_backend *window = fixture.base.window->as_null();
            REQUIRE(window != nullptr);
            window->simulate_minimise(true);
        }
        const math::vec3 eye = math::vec3(2.0f, 0.0f, 0.0f);
        const ecs::entity camera =
          fixture.add_camera("camera", renderer::camera_component{}, eye, turn_about_y(math::PI * 0.5f));
        const math::vec3 forward = math::vec3(-1.0f, 0.0f, 0.0f);

        const conjure_vec2 pixel = {100.0f, 100.0f};
        conjure_vec3 origin = {9.0f, 9.0f, 9.0f};
        conjure_vec3 direction = {};
        CHECK(api.camera_screen_ray(camera.to_u64(), &pixel, &origin, &direction) == 0);
        CONJURE_CHECK_APPROX(to_math(origin), eye);
        CONJURE_CHECK_APPROX(to_math(direction), forward);
        CONJURE_CHECK_APPROX(screen_to_world(api, camera, 100.0f, 100.0f, 3.0f, 0), eye + forward * 3.0f);
        const math::vec3 projected = world_to_screen(api, camera, math::vec3(-4.0f, 0.0f, 0.0f), 0);
        CONJURE_CHECK_APPROX(projected, math::vec3(0.0f, 0.0f, 6.0f));
        CHECK(last_error_code() == bindings_error::NONE);
    }

    TEST_CASE("camera - the current pose: a moved camera is seen before any propagation or extraction") {
        camera_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const conjure_vec2 centre = {SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f};
        conjure_vec3 origin = {};
        conjure_vec3 direction = {};

        SUBCASE("a scene camera: the local transforms composed now, a parent's included") {
            const ecs::entity rig = fixture.base.create("rig");
            const ecs::entity camera = fixture.base.create("camera", rig);
            CONJURE_REQUIRE_OK(fixture.base.world.add<renderer::camera_component>(camera));
            fixture.base.step();
            CHECK(api.camera_screen_ray(camera.to_u64(), &centre, &origin, &direction) == 1);
            CONJURE_CHECK_APPROX(to_math(direction), math::vec3(0.0f, 0.0f, -1.0f));

            // The rig turns and moves without a step: render_world_transform still holds the old pose.
            scene::local_transform rigPose = {};
            rigPose.position = math::vec3(0.0f, 0.0f, 5.0f);
            rigPose.rotation = turn_about_y(-math::PI * 0.5f);
            CONJURE_REQUIRE_OK(fixture.base.active().set_local_transform(rig, rigPose));
            CHECK(api.camera_screen_ray(camera.to_u64(), &centre, &origin, &direction) == 1);
            CONJURE_CHECK_APPROX(to_math(direction), math::vec3(1.0f, 0.0f, 0.0f));
            CONJURE_CHECK_APPROX(to_math(origin), math::vec3(0.1f, 0.0f, 5.0f));
        }
        SUBCASE("a camera without a local transform uses its render world transform") {
            const core::result<ecs::entity> created = fixture.base.world.create_entity();
            CONJURE_REQUIRE_OK(created);
            scene::render_world_transform rendered = {};
            rendered.matrix = glm::translate(math::mat4(1.0f), math::vec3(0.0f, 7.0f, 0.0f));
            CONJURE_REQUIRE_OK(fixture.base.world.add<scene::render_world_transform>(created.value(), rendered));
            CONJURE_REQUIRE_OK(fixture.base.world.add<renderer::camera_component>(created.value()));
            CHECK(api.camera_screen_ray(created.value().to_u64(), &centre, &origin, &direction) == 1);
            CONJURE_CHECK_APPROX(to_math(origin), math::vec3(0.0f, 7.0f, -0.1f));
            CONJURE_CHECK_APPROX(to_math(direction), math::vec3(0.0f, 0.0f, -1.0f));
        }
    }

    TEST_CASE("camera - the refusals name what is wrong and write nothing") {
        camera_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const ecs::entity camera = fixture.add_camera("camera", renderer::camera_component{});
        const u64 id = camera.to_u64();
        const conjure_vec2 pixel = {10.0f, 10.0f};
        const conjure_vec3 point = {10.0f, 10.0f, 1.0f};
        conjure_vec3 out = {};
        constexpr int32_t INVALID = to_status(bindings_error::INVALID_ARGUMENT);

        SUBCASE("null pointers") {
            CHECK(api.camera_screen_ray(id, nullptr, &out, &out) == INVALID);
            CHECK(api.camera_screen_ray(id, &pixel, nullptr, &out) == INVALID);
            CHECK(api.camera_screen_ray(id, &pixel, &out, nullptr) == INVALID);
            CHECK(api.camera_screen_to_world(id, nullptr, &out) == INVALID);
            CHECK(api.camera_screen_to_world(id, &point, nullptr) == INVALID);
            CHECK(api.camera_world_to_screen(id, nullptr, &out) == INVALID);
            CHECK(api.camera_world_to_screen(id, &point, nullptr) == INVALID);
            CHECK(last_error_message() == "camera_world_to_screen: null pointer");
        }
        SUBCASE("non-finite input") {
            const f32 nan = std::numeric_limits<f32>::quiet_NaN();
            const f32 infinity = std::numeric_limits<f32>::infinity();
            const conjure_vec2 badPixel = {nan, 0.0f};
            const conjure_vec3 badDepth = {0.0f, 0.0f, infinity};
            const conjure_vec3 badWorld = {0.0f, nan, 0.0f};
            CHECK(api.camera_screen_ray(id, &badPixel, &out, &out) == INVALID);
            CHECK(api.camera_screen_to_world(id, &badDepth, &out) == INVALID);
            CHECK(api.camera_world_to_screen(id, &badWorld, &out) == INVALID);
            CHECK(last_error_message() == "camera_world_to_screen: non-finite input");
        }
        SUBCASE("a dead entity, and the null entity of camera_main without a camera") {
            CHECK(api.camera_screen_ray(0, &pixel, &out, &out) == to_status(bindings_error::ENTITY_NOT_ALIVE));
            CONJURE_REQUIRE_OK(fixture.base.active().destroy_now(camera));
            CHECK(api.camera_screen_to_world(id, &point, &out) == to_status(bindings_error::ENTITY_NOT_ALIVE));
            CHECK(api.camera_world_to_screen(id, &point, &out) == to_status(bindings_error::ENTITY_NOT_ALIVE));
            CHECK(last_error_message() == "camera_world_to_screen");
        }
        SUBCASE("no camera component") {
            const ecs::entity plain = fixture.base.create("plain");
            const int32_t status = api.camera_screen_ray(plain.to_u64(), &pixel, &out, &out);
            CHECK(status == to_status(bindings_error::COMPONENT_MISSING));
            CHECK(last_error_message() == "camera_screen_ray: camera");
        }
        SUBCASE("neither a local nor a render world transform") {
            const core::result<ecs::entity> bare = fixture.base.world.create_entity();
            CONJURE_REQUIRE_OK(bare);
            CONJURE_REQUIRE_OK(fixture.base.world.add<renderer::camera_component>(bare.value()));
            const int32_t status = api.camera_screen_to_world(bare.value().to_u64(), &point, &out);
            CHECK(status == to_status(bindings_error::COMPONENT_MISSING));
            CHECK(last_error_message() == "camera_screen_to_world: transform");
        }
        SUBCASE("camera data the projection refuses") {
            renderer::camera_component &settings = *fixture.base.world.get<renderer::camera_component>(camera);
            const std::array<renderer::camera, 6> broken = [] {
                std::array<renderer::camera, 6> values = {};
                values[0].nearPlane = 0.0f;
                values[1].farPlane = values[1].nearPlane;
                values[2].verticalFovRadians = math::PI;
                values[3].nearPlane = std::numeric_limits<f32>::quiet_NaN();
                values[4].projection = renderer::projection_type::ORTHOGRAPHIC;
                values[4].orthographicHeight = 0.0f;
                values[5].projection = renderer::projection_type::ORTHOGRAPHIC;
                values[5].orthographicHeight = std::numeric_limits<f32>::infinity();
                return values;
            }();
            for (usize index = 0; index < broken.size(); ++index) {
                CAPTURE(index);
                settings.cameraData = broken[index];
                CHECK(api.camera_screen_ray(id, &pixel, &out, &out) == INVALID);
                CHECK(api.camera_screen_to_world(id, &point, &out) == INVALID);
                CHECK(api.camera_world_to_screen(id, &point, &out) == INVALID);
            }
        }
        CHECK(out.x == 0.0f);
        CHECK(out.y == 0.0f);
        CHECK(out.z == 0.0f);
    }

    TEST_CASE("camera - without the world and scenes every entry is a NOT_SUPPORTED stub") {
        bare_fixture_options options = {};
        options.fill = false;
        camera_fixture fixture(options);
        fixture.base.context.scenes = nullptr;
        fixture.base.fill();
        conjure_native_api &api = fixture.api();
        const ecs::entity camera = fixture.add_camera("camera", renderer::camera_component{});
        const conjure_vec2 pixel = {10.0f, 10.0f};
        const conjure_vec3 point = {10.0f, 10.0f, 1.0f};
        conjure_vec3 out = {};
        constexpr int32_t UNSUPPORTED = to_status(bindings_error::NOT_SUPPORTED);
        CHECK(api.camera_main() == 0);
        CHECK(last_error_code() == bindings_error::NOT_SUPPORTED);
        CHECK(api.camera_screen_ray(camera.to_u64(), &pixel, &out, &out) == UNSUPPORTED);
        CHECK(api.camera_screen_to_world(camera.to_u64(), &point, &out) == UNSUPPORTED);
        CHECK(api.camera_world_to_screen(camera.to_u64(), &point, &out) == UNSUPPORTED);
        CHECK(last_error_message() == "camera_world_to_screen: subsystem unavailable");
    }

    TEST_CASE("regression - code sweep W3-CS-L3 the camera entries pick instead of answering NOT_SUPPORTED") {
        camera_fixture fixture = {};
        conjure_native_api &api = fixture.api();
        const ecs::entity camera = fixture.add_camera("camera", renderer::camera_component{});
        CHECK(api.camera_main() == camera.to_u64());
        const conjure_vec2 centre = {SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f};
        conjure_vec3 origin = {};
        conjure_vec3 direction = {};
        CHECK(api.camera_screen_ray(camera.to_u64(), &centre, &origin, &direction) == 1);
        CHECK(last_error_code() == bindings_error::NONE);
    }
}
