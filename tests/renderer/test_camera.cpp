#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"
#include "conjure/testing/assert_capture.h"

#include "conjure/renderer/camera.h"

#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <string_view>

// camera.h (docs/design/02_renderer.md R1, 4.2): reverse-Z projections for both camera types, the
// frustum extraction that knows which plane is near, and viewport unprojection.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;

namespace {

    [[nodiscard]] f32 projected_depth(const math::mat4 &clipFromView, f32 viewZ) {
        const math::vec4 clip = clipFromView * math::vec4(0.0f, 0.0f, viewZ, 1.0f);
        return clip.z / clip.w;
    }

    [[nodiscard]] camera orthographic_camera() {
        camera value = {};
        value.projection = projection_type::ORTHOGRAPHIC;
        value.orthographicHeight = 20.0f;
        value.nearPlane = 1.0f;
        value.farPlane = 50.0f;
        return value;
    }
}

TEST_SUITE("renderer/camera") {

    TEST_CASE("camera - the default is a 60 degree perspective camera from 0.1 to 1000 metres") {
        const camera value = {};
        CHECK(value.projection == projection_type::PERSPECTIVE);
        CHECK(value.verticalFovRadians == doctest::Approx(1.0472f));
        CHECK(value.orthographicHeight == doctest::Approx(10.0f));
        CHECK(value.nearPlane == doctest::Approx(0.1f));
        CHECK(value.farPlane == doctest::Approx(1000.0f));
        CHECK(value == camera());
    }

    TEST_CASE("clip_from_view - a perspective projection maps near to depth 1 and far to depth 0") {
        const camera value = {};
        const math::mat4 clipFromView = clip_from_view(value, 16.0f / 9.0f);
        CHECK(projected_depth(clipFromView, -value.nearPlane) == doctest::Approx(1.0f));
        CHECK(projected_depth(clipFromView, -value.farPlane) == doctest::Approx(0.0f).epsilon(1.0e-4));
        // Depth decreases monotonically with distance and stays inside [0, 1].
        f32 previous = 1.0f;
        for (f32 distance = 0.2f; distance < value.farPlane; distance *= 1.7f) {
            const f32 depth = projected_depth(clipFromView, -distance);
            CHECK(depth < previous);
            CHECK(depth >= 0.0f);
            CHECK(depth <= 1.0f);
            previous = depth;
        }
        // The vertical field of view: a point on the top edge at distance d lands on y = +1.
        const f32 halfHeight = std::tan(value.verticalFovRadians * 0.5f) * 10.0f;
        const math::vec4 top = clipFromView * math::vec4(0.0f, halfHeight, -10.0f, 1.0f);
        CHECK(top.y / top.w == doctest::Approx(1.0f));
        // Aspect ratio widens x: the right edge at distance d is halfHeight * aspect.
        const math::vec4 right = clipFromView * math::vec4(halfHeight * 16.0f / 9.0f, 0.0f, -10.0f, 1.0f);
        CHECK(right.x / right.w == doctest::Approx(1.0f));
        CHECK(clipFromView[2][3] == doctest::Approx(-1.0f));
    }

    TEST_CASE("clip_from_view - an orthographic projection is reverse-Z too and spans the height") {
        const camera value = orthographic_camera();
        const math::mat4 clipFromView = clip_from_view(value, 2.0f);
        CHECK(projected_depth(clipFromView, -value.nearPlane) == doctest::Approx(1.0f));
        CHECK(projected_depth(clipFromView, -value.farPlane) == doctest::Approx(0.0f));
        CHECK(projected_depth(clipFromView, -25.5f) == doctest::Approx(0.5f));
        const math::vec4 top = clipFromView * math::vec4(0.0f, 10.0f, -10.0f, 1.0f);
        CHECK(top.y / top.w == doctest::Approx(1.0f));
        const math::vec4 right = clipFromView * math::vec4(20.0f, 0.0f, -10.0f, 1.0f);
        CHECK(right.x / right.w == doctest::Approx(1.0f));
        CHECK(clipFromView[3][3] == doctest::Approx(1.0f));
        CHECK(clipFromView[2][3] == doctest::Approx(0.0f));
    }

    TEST_CASE("clip_from_view - an invalid camera asserts and yields the identity") {
        camera value = {};
        CONJURE_CHECK_ASSERTS(CHECK(clip_from_view(value, 0.0f) == math::mat4(1.0f)));
        value.nearPlane = 0.0f;
        CONJURE_CHECK_ASSERTS(CHECK(clip_from_view(value, 1.0f) == math::mat4(1.0f)));
        value = camera();
        value.farPlane = value.nearPlane;
        CONJURE_CHECK_ASSERTS(CHECK(clip_from_view(value, 1.0f) == math::mat4(1.0f)));
        value = camera();
        value.verticalFovRadians = 0.0f;
        CONJURE_CHECK_ASSERTS(CHECK(clip_from_view(value, 1.0f) == math::mat4(1.0f)));
        value = camera();
        value.verticalFovRadians = math::PI;
        CONJURE_CHECK_ASSERTS(CHECK(clip_from_view(value, 1.0f) == math::mat4(1.0f)));
        value = orthographic_camera();
        value.orthographicHeight = -1.0f;
        CONJURE_CHECK_ASSERTS(CHECK(clip_from_view(value, 1.0f) == math::mat4(1.0f)));
    }

    TEST_CASE("frustum_from_clip - the near plane is the one at depth 1 and the far plane at depth 0") {
        const camera value = {};
        const math::mat4 clipFromWorld = clip_from_view(value, 1.0f);
        const math::frustum frustum = frustum_from_clip(clipFromWorld);
        const math::plane &nearPlane = frustum.planes[static_cast<usize>(math::frustum_plane::NEAR_PLANE)];
        const math::plane &farPlane = frustum.planes[static_cast<usize>(math::frustum_plane::FAR_PLANE)];
        // Just beyond the near plane is inside both; between the eye and the near plane is outside near.
        CHECK(nearPlane.signed_distance(math::vec3(0.0f, 0.0f, -0.2f)) > 0.0f);
        CHECK(nearPlane.signed_distance(math::vec3(0.0f, 0.0f, -0.05f)) < 0.0f);
        CHECK(farPlane.signed_distance(math::vec3(0.0f, 0.0f, -0.2f)) > 0.0f);
        CHECK(farPlane.signed_distance(math::vec3(0.0f, 0.0f, -1500.0f)) < 0.0f);
        // The near plane faces away from the eye (-Z), the far plane toward it (+Z).
        CHECK(nearPlane.normal.z < 0.0f);
        CHECK(farPlane.normal.z > 0.0f);
        CHECK(frustum.contains(math::vec3(0.0f, 0.0f, -10.0f)));
        CHECK_FALSE(frustum.contains(math::vec3(0.0f, 0.0f, 10.0f)));
        CHECK_FALSE(frustum.contains(math::vec3(100.0f, 0.0f, -10.0f)));
        // The orthographic case classifies the same way.
        const math::frustum ortho = frustum_from_clip(clip_from_view(orthographic_camera(), 1.0f));
        CHECK(ortho.contains(math::vec3(0.0f, 0.0f, -25.0f)));
        CHECK_FALSE(ortho.contains(math::vec3(0.0f, 0.0f, -0.5f)));
        CHECK_FALSE(ortho.contains(math::vec3(0.0f, 0.0f, -60.0f)));
    }

    TEST_CASE("ray_from_viewport - the centre of the viewport looks down -Z from the near plane") {
        const camera value = {};
        const math::ray centre = ray_from_viewport(value, math::mat4(1.0f), math::vec2(0.5f, 0.5f), 1.5f);
        CONJURE_CHECK_APPROX(centre.origin, math::vec3(0.0f, 0.0f, -value.nearPlane));
        CONJURE_CHECK_APPROX(centre.direction, math::vec3(0.0f, 0.0f, -1.0f));
        // Right of centre points +X, the top row points +Y (viewport y is down).
        const math::ray right = ray_from_viewport(value, math::mat4(1.0f), math::vec2(1.0f, 0.5f), 1.5f);
        CHECK(right.direction.x > 0.0f);
        CHECK(right.direction.y == doctest::Approx(0.0f).epsilon(1.0e-4));
        const math::ray top = ray_from_viewport(value, math::mat4(1.0f), math::vec2(0.5f, 0.0f), 1.5f);
        CHECK(top.direction.y > 0.0f);
        CHECK(top.direction.x == doctest::Approx(0.0f).epsilon(1.0e-4));
        CHECK(glm::length(top.direction) == doctest::Approx(1.0f));
        // A perspective ray passes through the eye: origin + direction * t hits the eye for t = -|origin|.
        const f32 toEye = -glm::length(right.origin) / glm::dot(right.direction, glm::normalize(right.origin));
        CONJURE_CHECK_APPROX(right.at(toEye), math::vec3(0.0f));
    }

    TEST_CASE("ray_from_viewport - the camera transform moves the ray and an orthographic ray stays parallel") {
        const camera value = {};
        const math::mat4 worldFromView = glm::translate(math::mat4(1.0f), math::vec3(5.0f, 2.0f, 1.0f));
        const math::ray moved = ray_from_viewport(value, worldFromView, math::vec2(0.5f, 0.5f), 1.0f);
        CONJURE_CHECK_APPROX(moved.origin, math::vec3(5.0f, 2.0f, 1.0f - value.nearPlane));
        CONJURE_CHECK_APPROX(moved.direction, math::vec3(0.0f, 0.0f, -1.0f));

        const camera ortho = orthographic_camera();
        const math::ray corner = ray_from_viewport(ortho, math::mat4(1.0f), math::vec2(0.0f, 0.0f), 2.0f);
        CONJURE_CHECK_APPROX(corner.direction, math::vec3(0.0f, 0.0f, -1.0f));
        CONJURE_CHECK_APPROX(corner.origin, math::vec3(-20.0f, 10.0f, -ortho.nearPlane));
    }

    TEST_CASE("projection_type - to_string names both projections") {
        CHECK(to_string(projection_type::PERSPECTIVE) == "PERSPECTIVE");
        CHECK(to_string(projection_type::ORTHOGRAPHIC) == "ORTHOGRAPHIC");
        CHECK_FALSE(to_string(static_cast<projection_type>(7)).empty());
    }
}
