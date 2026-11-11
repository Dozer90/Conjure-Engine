#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"

#include "conjure/core/json/document.h"
#include "conjure/core/math.h"
#include "conjure/editor/editor_camera.h"
#include "conjure/editor/editor_error.h"

#include <cmath>

// editor_camera.h: the orbit camera that view.* commands and the viewport drive.

using namespace conjure;
using namespace conjure::editor;

TEST_SUITE("editor/editor_camera") {

    TEST_CASE("editor_camera - the centre screen ray follows the forward direction") {
        editor_camera camera = {};
        camera.set_look_at(math::vec3(0.0f, 0.0f, 10.0f), math::vec3(0.0f));
        const math::ray ray = camera.screen_ray(math::vec2(0.5f, 0.5f), 16.0f / 9.0f);
        CONJURE_CHECK_APPROX(ray.direction, camera.forward());
        CONJURE_CHECK_APPROX(ray.direction, math::vec3(0.0f, 0.0f, -1.0f));
        // The origin lies on the near plane in front of the camera.
        CHECK(ray.origin.z < 10.0f);
        CHECK(ray.origin.z > 9.0f);
        // The top-left corner points up and left.
        const math::ray corner = camera.screen_ray(math::vec2(0.0f, 0.0f), 1.0f);
        CHECK(corner.direction.x < 0.0f);
        CHECK(corner.direction.y > 0.0f);
    }

    TEST_CASE("editor_camera - focus frames the bounds in front of the camera") {
        editor_camera camera = {};
        camera.set_look_at(math::vec3(5.0f, 5.0f, 5.0f), math::vec3(0.0f));
        math::aabb bounds = {};
        bounds.min = math::vec3(9.0f, -1.0f, -1.0f);
        bounds.max = math::vec3(11.0f, 1.0f, 1.0f);
        camera.focus(bounds);
        CONJURE_CHECK_APPROX(camera.target(), math::vec3(10.0f, 0.0f, 0.0f));
        const f32 distance = glm::length(camera.position() - camera.target());
        CHECK(distance > glm::length(bounds.extents()));
        // The direction is kept: the camera still looks along the old forward vector.
        const math::vec3 expectedDirection = glm::normalize(math::vec3(0.0f) - math::vec3(5.0f, 5.0f, 5.0f));
        CONJURE_CHECK_APPROX(camera.forward(), expectedDirection);
        // Every corner projects inside the viewport.
        const math::mat4 clipFromWorld = camera.projection_matrix(1.0f) * camera.view_matrix();
        for (const math::vec3 corner : {bounds.min, bounds.max}) {
            math::vec4 clip = clipFromWorld * math::vec4(corner, 1.0f);
            clip /= clip.w;
            CHECK(std::abs(clip.x) <= 1.0f);
            CHECK(std::abs(clip.y) <= 1.0f);
        }
        math::aabb invalid = {};
        camera.focus(invalid);
        CONJURE_CHECK_APPROX(camera.target(), math::vec3(10.0f, 0.0f, 0.0f));
    }

    TEST_CASE("editor_camera - orbit, pan and zoom keep the target relationship sane") {
        editor_camera camera = {};
        camera.set_look_at(math::vec3(0.0f, 0.0f, 10.0f), math::vec3(0.0f));
        const f32 before = glm::length(camera.position() - camera.target());
        camera.orbit(math::radians(90.0f), 0.0f);
        CONJURE_CHECK_APPROX(glm::length(camera.position() - camera.target()), before);
        CHECK(std::abs(camera.position().z) < 0.01f);
        camera.zoom(0.5f);
        CONJURE_CHECK_APPROX(glm::length(camera.position() - camera.target()), before * 0.5f);
        const math::vec3 target = camera.target();
        camera.pan(math::vec2(1.0f, 0.0f));
        CHECK(glm::length(camera.target() - target) > 0.0f);
        camera.set_orthographic(true);
        camera.set_orthographic_height(4.0f);
        CHECK(camera.is_orthographic());
        camera.zoom(2.0f);
        CONJURE_CHECK_APPROX(camera.orthographic_height(), 8.0f);
    }

    TEST_CASE("editor_camera - projection is reverse-Z and respects the aspect ratio") {
        editor_camera camera = {};
        camera.set_planes(0.1f, 100.0f);
        const math::mat4 projection = camera.projection_matrix(2.0f);
        const math::vec4 nearPoint = projection * math::vec4(0.0f, 0.0f, -0.1f, 1.0f);
        const math::vec4 farPoint = projection * math::vec4(0.0f, 0.0f, -100.0f, 1.0f);
        CHECK(nearPoint.z / nearPoint.w > farPoint.z / farPoint.w);
        CONJURE_CHECK_APPROX(nearPoint.z / nearPoint.w, 1.0f);
        CHECK(std::abs(farPoint.z / farPoint.w) < 1.0e-4f);
        CHECK(projection[0][0] < projection[1][1]);
        CHECK(camera.near_plane() == doctest::Approx(0.1f));
        CHECK(camera.far_plane() == doctest::Approx(100.0f));
    }

    TEST_CASE("editor_camera - to_json and apply_json round trip and reject malformed members") {
        editor_camera camera = {};
        camera.set_look_at(math::vec3(1.0f, 2.0f, 3.0f), math::vec3(0.0f, 0.0f, -1.0f));
        camera.set_vertical_fov(math::radians(50.0f));
        const core::json::document json = camera.to_json();
        CHECK(json.root()["position"].size() == 3);
        CHECK(json.root()["target"].size() == 3);
        CHECK(json.root().contains("fov"));
        CHECK(json.root().contains("near"));
        editor_camera other = {};
        CONJURE_REQUIRE_OK(other.apply_json(json.root()));
        CONJURE_CHECK_APPROX(other.position(), camera.position());
        CONJURE_CHECK_APPROX(other.target(), camera.target());
        CHECK(other.vertical_fov() == doctest::Approx(camera.vertical_fov()));
        const core::json::document bad = core::json::object({{"position", "nope"}});
        CONJURE_CHECK_ERROR(other.apply_json(bad.root()), protocol_error_code::INVALID_ARGS);
        const core::json::document badFov = core::json::object({{"fov", -1.0}});
        CONJURE_CHECK_ERROR(other.apply_json(badFov.root()), protocol_error_code::INVALID_ARGS);
        const core::json::document notObject = core::json::array({1, 2, 3});
        CONJURE_CHECK_ERROR(other.apply_json(notObject.root()), protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("regression - the fov crosses the protocol in degrees, as the catalogue says") {
        editor_camera camera = {};
        CONJURE_REQUIRE_OK(camera.apply_json(core::json::object({{"fov", 60.0}}).root()));
        CHECK(camera.vertical_fov() == doctest::Approx(math::radians(60.0f)));
        CHECK(camera.to_json().root()["fov"].as_f32() == doctest::Approx(60.0f));
        CONJURE_REQUIRE_OK(camera.apply_json(core::json::object({{"fov", 1.0}}).root()));
        CHECK(camera.vertical_fov() == doctest::Approx(math::radians(1.0f)));
        // The radian-sized values an older client would send are still angles, just tiny ones; 180 and above
        // are not a perspective.
        CONJURE_CHECK_ERROR(
          camera.apply_json(core::json::object({{"fov", 180.0}}).root()),
          protocol_error_code::INVALID_ARGS
        );
        CONJURE_CHECK_ERROR(
          camera.apply_json(core::json::object({{"fov", 0.0}}).root()),
          protocol_error_code::INVALID_ARGS
        );
        CHECK(camera.vertical_fov() == doctest::Approx(math::radians(1.0f)));
    }
}
