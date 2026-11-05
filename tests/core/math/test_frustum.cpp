#include "conjure/testing/doctest.h"

#include "math_test.h"

#include "conjure/testing/assert_capture.h"

#include "conjure/core/math/frustum.h"

#include "conjure/core/math/math.h"

#include <array>
#include <cmath>

using namespace conjure;
using namespace conjure::math;

namespace {
    constexpr f32 FOV_Y = HALF_PI;         // 90 degrees: the frustum half-extent equals the depth
    constexpr f32 ASPECT = 1.0f;
    constexpr f32 NEAR_DISTANCE = 1.0f;
    constexpr f32 FAR_DISTANCE = 100.0f;

    // Camera at the origin looking down -Z: inside means |x| <= -z, |y| <= -z, near <= -z <= far.
    frustum camera_frustum() {
        return frustum::from_view_projection(perspective(FOV_Y, ASPECT, NEAR_DISTANCE, FAR_DISTANCE));
    }

    aabb box(vec3 min, vec3 max) {
        aabb result = {};
        result.min = min;
        result.max = max;
        return result;
    }

    bool all_planes_unit_length(const frustum &value) {
        for (const plane &side : value.planes) {
            if (!nearly_equal(glm::length(side.normal), 1.0f, math_test::TIGHT_EPSILON)) {
                return false;
            }
        }
        return true;
    }
}

TEST_SUITE("core/frustum") {

    TEST_CASE("frustum - plane indices are stable") {
        CHECK(static_cast<u8>(frustum_plane::LEFT) == 0);
        CHECK(static_cast<u8>(frustum_plane::RIGHT) == 1);
        CHECK(static_cast<u8>(frustum_plane::BOTTOM) == 2);
        CHECK(static_cast<u8>(frustum_plane::TOP) == 3);
        CHECK(static_cast<u8>(frustum_plane::NEAR_PLANE) == 4);
        CHECK(static_cast<u8>(frustum_plane::FAR_PLANE) == 5);
        CHECK(static_cast<u8>(frustum_plane::COUNT) == 6);
        static_assert(FRUSTUM_PLANE_COUNT == 6);
        static_assert(std::tuple_size_v<decltype(frustum::planes)> == FRUSTUM_PLANE_COUNT);

        const frustum value = camera_frustum();
        for (usize index = 0; index < FRUSTUM_PLANE_COUNT; ++index) {
            CHECK(value.get(static_cast<frustum_plane>(index)) == value.planes[index]);
        }
    }

    TEST_CASE("frustum - the identity matrix yields the clip-space unit volume with 0..1 depth") {
        const frustum clip = frustum::from_view_projection(mat4(1.0f));
        CHECK(all_planes_unit_length(clip));

        CHECK_NEARLY(clip.get(frustum_plane::LEFT).normal, vec3(1.0f, 0.0f, 0.0f), math_test::TIGHT_EPSILON);
        CHECK(nearly_equal(clip.get(frustum_plane::LEFT).distance, 1.0f, math_test::TIGHT_EPSILON));
        CHECK_NEARLY(clip.get(frustum_plane::RIGHT).normal, vec3(-1.0f, 0.0f, 0.0f), math_test::TIGHT_EPSILON);
        CHECK(nearly_equal(clip.get(frustum_plane::RIGHT).distance, 1.0f, math_test::TIGHT_EPSILON));
        CHECK_NEARLY(clip.get(frustum_plane::BOTTOM).normal, vec3(0.0f, 1.0f, 0.0f), math_test::TIGHT_EPSILON);
        CHECK_NEARLY(clip.get(frustum_plane::TOP).normal, vec3(0.0f, -1.0f, 0.0f), math_test::TIGHT_EPSILON);
        CHECK_NEARLY(clip.get(frustum_plane::NEAR_PLANE).normal, vec3(0.0f, 0.0f, 1.0f), math_test::TIGHT_EPSILON);
        CHECK(nearly_equal(clip.get(frustum_plane::NEAR_PLANE).distance, 0.0f, math_test::TIGHT_EPSILON));
        CHECK_NEARLY(clip.get(frustum_plane::FAR_PLANE).normal, vec3(0.0f, 0.0f, -1.0f), math_test::TIGHT_EPSILON);
        CHECK(nearly_equal(clip.get(frustum_plane::FAR_PLANE).distance, 1.0f, math_test::TIGHT_EPSILON));

        CHECK(clip.contains(vec3(0.0f, 0.0f, 0.5f)));
        CHECK(clip.contains(vec3(1.0f, -1.0f, 1.0f)));
        CHECK(clip.contains(vec3(0.0f, 0.0f, 0.0f)));
        CHECK_FALSE(clip.contains(vec3(0.0f, 0.0f, -0.01f)));   // in front of depth 0
        CHECK_FALSE(clip.contains(vec3(0.0f, 0.0f, 1.01f)));
        CHECK_FALSE(clip.contains(vec3(1.01f, 0.0f, 0.5f)));
        CHECK_FALSE(clip.contains(vec3(0.0f, -1.01f, 0.5f)));
    }

    TEST_CASE("frustum - planes extracted from a perspective projection are normalised and inward facing") {
        const frustum value = camera_frustum();
        CHECK(all_planes_unit_length(value));

        // With a 90 degree field of view the side planes are at 45 degrees and pass through the eye.
        const f32 diagonal = std::sqrt(0.5f);
        CHECK_NEARLY(value.get(frustum_plane::LEFT).normal, vec3(diagonal, 0.0f, -diagonal), math_test::TIGHT_EPSILON);
        const vec3 rightNormal = vec3(-diagonal, 0.0f, -diagonal);
        CHECK_NEARLY(value.get(frustum_plane::RIGHT).normal, rightNormal, math_test::TIGHT_EPSILON);
        const vec3 bottomNormal = vec3(0.0f, diagonal, -diagonal);
        CHECK_NEARLY(value.get(frustum_plane::BOTTOM).normal, bottomNormal, math_test::TIGHT_EPSILON);
        CHECK_NEARLY(value.get(frustum_plane::TOP).normal, vec3(0.0f, -diagonal, -diagonal), math_test::TIGHT_EPSILON);
        const std::array<frustum_plane, 4> sides = {
          frustum_plane::LEFT,
          frustum_plane::RIGHT,
          frustum_plane::BOTTOM,
          frustum_plane::TOP
        };
        for (const frustum_plane side : sides) {
            CHECK(nearly_zero(value.get(side).distance, math_test::TIGHT_EPSILON));
        }

        // Near plane: -z >= near. Far plane: -z <= far.
        CHECK_NEARLY(value.get(frustum_plane::NEAR_PLANE).normal, vec3(0.0f, 0.0f, -1.0f), math_test::TIGHT_EPSILON);
        CHECK(nearly_equal(value.get(frustum_plane::NEAR_PLANE).distance, -NEAR_DISTANCE, math_test::LOOSE_EPSILON));
        CHECK_NEARLY(value.get(frustum_plane::FAR_PLANE).normal, vec3(0.0f, 0.0f, 1.0f), math_test::TIGHT_EPSILON);
        CHECK(nearly_equal(value.get(frustum_plane::FAR_PLANE).distance, FAR_DISTANCE, 1.0e-2f));
    }

    TEST_CASE("frustum - extraction matches glm::perspective with 0..1 depth") {
        const mat4 fromGlm = glm::perspective(radians(60.0f), 16.0f / 9.0f, 0.1f, 500.0f);
        const mat4 fromEngine = perspective(radians(60.0f), 16.0f / 9.0f, 0.1f, 500.0f);
        CHECK_NEARLY(fromGlm, fromEngine, math_test::TIGHT_EPSILON);

        const frustum value = frustum::from_view_projection(fromGlm);
        CHECK(value == frustum::from_view_projection(fromEngine));

        // Every point whose projection lands in the clip volume is inside the frustum and vice versa.
        const std::array<vec3, 6> samples = {
          vec3(0.0f, 0.0f, -0.11f),     // just inside the near plane
          vec3(0.0f, 0.0f, -250.0f),
          vec3(0.0f, 0.0f, -0.05f),     // closer than near
          vec3(0.0f, 0.0f, -600.0f),    // beyond far
          vec3(50.0f, 0.0f, -10.0f),    // far to the right
          vec3(0.0f, 0.0f, 5.0f)        // behind the camera
        };
        for (const vec3 &point : samples) {
            const std::optional<vec3> projected = project(point, fromGlm);
            REQUIRE(projected.has_value());
            const vec4 clip = fromGlm * vec4(point, 1.0f);
            // The samples sit well away from the planes, so exact clip-space comparisons are safe.
            const bool insideClip = clip.w > 0.0f && absolute(projected->x) <= 1.0f && absolute(projected->y) <= 1.0f
              && projected->z >= 0.0f && projected->z <= 1.0f;
            CHECK_MESSAGE(value.contains(point) == insideClip, std::format("point {} ndc {}", point, *projected));
        }
    }

    TEST_CASE("frustum - contains(point) respects every plane of a perspective camera") {
        const frustum value = camera_frustum();
        CHECK(value.contains(vec3(0.0f, 0.0f, -50.0f)));
        CHECK(value.contains(vec3(0.0f, 0.0f, -NEAR_DISTANCE - 0.001f)));
        CHECK(value.contains(vec3(0.0f, 0.0f, -FAR_DISTANCE + 0.01f)));
        CHECK(value.contains(vec3(9.99f, 0.0f, -10.0f)));
        CHECK(value.contains(vec3(-9.99f, 0.0f, -10.0f)));
        CHECK(value.contains(vec3(0.0f, 9.99f, -10.0f)));
        CHECK(value.contains(vec3(0.0f, -9.99f, -10.0f)));

        CHECK_FALSE(value.contains(vec3(0.0f, 0.0f, -NEAR_DISTANCE + 0.01f)));   // before near
        CHECK_FALSE(value.contains(vec3(0.0f, 0.0f, -FAR_DISTANCE - 0.1f)));     // beyond far
        CHECK_FALSE(value.contains(vec3(0.0f, 0.0f, 5.0f)));                      // behind
        CHECK_FALSE(value.contains(vec3(0.0f)));                                  // at the eye
        CHECK_FALSE(value.contains(vec3(10.01f, 0.0f, -10.0f)));                  // right
        CHECK_FALSE(value.contains(vec3(-10.01f, 0.0f, -10.0f)));                 // left
        CHECK_FALSE(value.contains(vec3(0.0f, 10.01f, -10.0f)));                  // top
        CHECK_FALSE(value.contains(vec3(0.0f, -10.01f, -10.0f)));                 // bottom
    }

    TEST_CASE("frustum - a view matrix moves the frustum with the camera") {
        const vec3 eye = vec3(10.0f, 5.0f, 0.0f);
        const vec3 target = vec3(10.0f, 5.0f, -20.0f);
        const mat4 view = look_at(eye, target, vec3(0.0f, 1.0f, 0.0f));
        const mat4 projection = perspective(FOV_Y, ASPECT, NEAR_DISTANCE, FAR_DISTANCE);
        const frustum value = frustum::from_view_projection(projection * view);

        CHECK(value.contains(eye + vec3(0.0f, 0.0f, -10.0f)));
        CHECK(value.contains(eye + vec3(9.0f, -9.0f, -10.0f)));
        CHECK_FALSE(value.contains(eye));
        CHECK_FALSE(value.contains(vec3(-5.0f, 0.0f, -10.0f)));   // 15 units left at depth 10: outside
        CHECK_FALSE(value.contains(eye + vec3(0.0f, 0.0f, 10.0f)));
    }

    TEST_CASE("frustum - corners of a perspective frustum lie on the planes") {
        const mat4 projection = perspective(FOV_Y, ASPECT, NEAR_DISTANCE, FAR_DISTANCE);
        const frustum value = frustum::from_view_projection(projection);
        const std::array<vec3, 8> corners = frustum::corners(glm::inverse(projection));

        CHECK_NEARLY(corners[0], vec3(-NEAR_DISTANCE, -NEAR_DISTANCE, -NEAR_DISTANCE), math_test::LOOSE_EPSILON);
        CHECK_NEARLY(corners[1], vec3(NEAR_DISTANCE, -NEAR_DISTANCE, -NEAR_DISTANCE), math_test::LOOSE_EPSILON);
        CHECK_NEARLY(corners[2], vec3(NEAR_DISTANCE, NEAR_DISTANCE, -NEAR_DISTANCE), math_test::LOOSE_EPSILON);
        CHECK_NEARLY(corners[3], vec3(-NEAR_DISTANCE, NEAR_DISTANCE, -NEAR_DISTANCE), math_test::LOOSE_EPSILON);
        CHECK_NEARLY(corners[4], vec3(-FAR_DISTANCE, -FAR_DISTANCE, -FAR_DISTANCE), 1.0e-2f);
        CHECK_NEARLY(corners[7], vec3(-FAR_DISTANCE, FAR_DISTANCE, -FAR_DISTANCE), 1.0e-2f);

        for (const vec3 &corner : corners) {
            for (const plane &side : value.planes) {
                CHECK(side.signed_distance(corner) >= -1.0e-2f);
            }
        }
        // Each corner touches three planes.
        for (const vec3 &corner : corners) {
            u32 touching = 0;
            for (const plane &side : value.planes) {
                if (nearly_zero(side.signed_distance(corner), 1.0e-2f)) {
                    ++touching;
                }
            }
            CHECK(touching == 3);
        }
    }

    TEST_CASE("frustum - orthographic projections produce axis-aligned planes") {
        const frustum value = frustum::from_view_projection(orthographic(-2.0f, 2.0f, -1.0f, 1.0f, 0.5f, 10.0f));
        CHECK(all_planes_unit_length(value));
        CHECK_NEARLY(value.get(frustum_plane::LEFT).normal, vec3(1.0f, 0.0f, 0.0f), math_test::TIGHT_EPSILON);
        CHECK(nearly_equal(value.get(frustum_plane::LEFT).distance, 2.0f, math_test::TIGHT_EPSILON));
        CHECK_NEARLY(value.get(frustum_plane::TOP).normal, vec3(0.0f, -1.0f, 0.0f), math_test::TIGHT_EPSILON);
        CHECK(nearly_equal(value.get(frustum_plane::TOP).distance, 1.0f, math_test::TIGHT_EPSILON));
        CHECK_NEARLY(value.get(frustum_plane::NEAR_PLANE).normal, vec3(0.0f, 0.0f, -1.0f), math_test::TIGHT_EPSILON);
        CHECK(nearly_equal(value.get(frustum_plane::NEAR_PLANE).distance, -0.5f, math_test::TIGHT_EPSILON));
        CHECK_NEARLY(value.get(frustum_plane::FAR_PLANE).normal, vec3(0.0f, 0.0f, 1.0f), math_test::TIGHT_EPSILON);
        CHECK(nearly_equal(value.get(frustum_plane::FAR_PLANE).distance, 10.0f, math_test::TIGHT_EPSILON));

        CHECK(value.contains(vec3(1.9f, -0.9f, -5.0f)));
        CHECK_FALSE(value.contains(vec3(2.1f, 0.0f, -5.0f)));
        CHECK_FALSE(value.contains(vec3(0.0f, 0.0f, -0.4f)));
        CHECK_FALSE(value.contains(vec3(0.0f, 0.0f, -10.1f)));

        const mat4 projection = orthographic(-2.0f, 2.0f, -1.0f, 1.0f, 0.5f, 10.0f);
        const std::array<vec3, 8> corners = frustum::corners(glm::inverse(projection));
        CHECK_NEARLY(corners[0], vec3(-2.0f, -1.0f, -0.5f), math_test::LOOSE_EPSILON);
        CHECK_NEARLY(corners[6], vec3(2.0f, 1.0f, -10.0f), math_test::LOOSE_EPSILON);
    }

    TEST_CASE("frustum - infinite projections have no far plane") {
        const frustum value = frustum::from_view_projection(infinite_perspective(FOV_Y, ASPECT, NEAR_DISTANCE));
        CHECK(value.contains(vec3(0.0f, 0.0f, -1.0e6f)));
        CHECK(value.contains(vec3(5.0e5f, 0.0f, -1.0e6f)));
        CHECK_FALSE(value.contains(vec3(0.0f, 0.0f, -0.5f)));
        CHECK_FALSE(value.contains(vec3(0.0f, 0.0f, 1.0f)));
        CHECK(value.intersects(box(vec3(-1.0f, -1.0f, -1.0e7f), vec3(1.0f, 1.0f, -1.0e7f + 1.0f))));
        CHECK(value.intersects_sphere(vec3(0.0f, 0.0f, -1.0e8f), 1.0f));

        // The far plane degenerates to "always in front" (zero normal, positive distance).
        const plane farPlane = value.get(frustum_plane::FAR_PLANE);
        CHECK(nearly_zero(farPlane.normal, math_test::LOOSE_EPSILON));
        CHECK(farPlane.distance > 0.0f);
    }

    TEST_CASE("frustum - corners of an infinite projection assert once and keep the near corners") {
        const mat4 projection = infinite_perspective(FOV_Y, ASPECT, NEAR_DISTANCE);
        testing::expect_assert asserts = {};
        const std::array<vec3, 8> corners = frustum::corners(glm::inverse(projection));
        CHECK(asserts.count() == 1);

        // The near corners are finite and correct; the far corners have no finite position.
        CHECK_NEARLY(corners[0], vec3(-NEAR_DISTANCE, -NEAR_DISTANCE, -NEAR_DISTANCE), math_test::LOOSE_EPSILON);
        CHECK_NEARLY(corners[2], vec3(NEAR_DISTANCE, NEAR_DISTANCE, -NEAR_DISTANCE), math_test::LOOSE_EPSILON);
        for (usize index = 0; index < 8; ++index) {
            CHECK(std::isfinite(corners[index].x));
            CHECK(std::isfinite(corners[index].y));
            CHECK(std::isfinite(corners[index].z));
        }

        // A finite projection never asserts.
        asserts.reset();
        const std::array<vec3, 8> finite = frustum::corners(glm::inverse(perspective(FOV_Y, ASPECT, 1.0f, 10.0f)));
        CHECK(asserts.count() == 0);
        CHECK_NEARLY(finite[4], vec3(-10.0f, -10.0f, -10.0f), 1.0e-3f);
    }

    TEST_CASE("frustum - intersects(aabb) accepts boxes inside, straddling and enclosing the frustum") {
        const frustum value = camera_frustum();
        CHECK(value.intersects(box(vec3(-1.0f, -1.0f, -11.0f), vec3(1.0f, 1.0f, -9.0f))));          // inside
        CHECK(value.intersects(box(vec3(9.0f, -1.0f, -11.0f), vec3(20.0f, 1.0f, -9.0f))));          // straddles right
        CHECK(value.intersects(box(vec3(-1.0f, -1.0f, -2.0f), vec3(1.0f, 1.0f, 2.0f))));            // straddles near
        CHECK(value.intersects(box(vec3(-1.0f, -1.0f, -200.0f), vec3(1.0f, 1.0f, -50.0f))));        // straddles far
        CHECK(value.intersects(box(vec3(-1000.0f), vec3(1000.0f))));                                // encloses
        CHECK(value.intersects(box(vec3(-1.0f, -1.0f, -1.01f), vec3(1.0f, 1.0f, 2.0f))));          // just past near
    }

    TEST_CASE("frustum - intersects(aabb) rejects boxes entirely behind a plane") {
        const frustum value = camera_frustum();
        CHECK_FALSE(value.intersects(box(vec3(-1.0f, -1.0f, 1.0f), vec3(1.0f, 1.0f, 3.0f))));        // behind camera
        CHECK_FALSE(value.intersects(box(vec3(-1.0f, -1.0f, -0.9f), vec3(1.0f, 1.0f, -0.1f))));      // before near
        CHECK_FALSE(value.intersects(box(vec3(-1.0f, -1.0f, -300.0f), vec3(1.0f, 1.0f, -101.0f)))); // beyond far
        CHECK_FALSE(value.intersects(box(vec3(11.0f, -1.0f, -10.0f), vec3(12.0f, 1.0f, -9.0f))));    // right of right
        CHECK_FALSE(value.intersects(box(vec3(-12.0f, -1.0f, -10.0f), vec3(-11.0f, 1.0f, -9.0f))));  // left of left
        CHECK_FALSE(value.intersects(box(vec3(-1.0f, 11.0f, -10.0f), vec3(1.0f, 12.0f, -9.0f))));    // above top
        CHECK_FALSE(value.intersects(box(vec3(-1.0f, -12.0f, -10.0f), vec3(1.0f, -11.0f, -9.0f))));  // below bottom
        CHECK_FALSE(value.intersects(aabb{}));                                                        // invalid
    }

    TEST_CASE("frustum - intersects(aabb) is conservative near the far corners") {
        // The box is outside the volume (x >= 101 while the frustum reaches x = 100 at the far
        // plane) but no single plane separates it: the p-vertex test reports an intersection.
        const frustum value = camera_frustum();
        const aabb corner = box(vec3(101.0f, -1.0f, -150.0f), vec3(200.0f, 1.0f, -90.0f));
        CHECK(value.intersects(corner));
        CHECK_FALSE(value.contains(corner));
        for (const vec3 &point : corner.corners()) {
            CHECK_FALSE(value.contains(point));
        }
    }

    TEST_CASE("frustum - contains(aabb) requires the whole box to be inside") {
        const frustum value = camera_frustum();
        CHECK(value.contains(box(vec3(-1.0f, -1.0f, -11.0f), vec3(1.0f, 1.0f, -9.0f))));
        CHECK(value.contains(box(vec3(-9.0f, -9.0f, -99.0f), vec3(9.0f, 9.0f, -9.0f))));
        CHECK_FALSE(value.contains(box(vec3(9.0f, -1.0f, -11.0f), vec3(20.0f, 1.0f, -9.0f))));   // straddles right
        CHECK_FALSE(value.contains(box(vec3(-1.0f, -1.0f, -2.0f), vec3(1.0f, 1.0f, 2.0f))));     // straddles near
        CHECK_FALSE(value.contains(box(vec3(-1000.0f), vec3(1000.0f))));                         // encloses
        CHECK_FALSE(value.contains(box(vec3(-1.0f, -1.0f, 1.0f), vec3(1.0f, 1.0f, 3.0f))));      // behind
        CHECK_FALSE(value.contains(aabb{}));
    }

    TEST_CASE("frustum - intersects_sphere") {
        const frustum value = camera_frustum();
        CHECK(value.intersects_sphere(vec3(0.0f, 0.0f, -10.0f), 1.0f));          // inside
        CHECK(value.intersects_sphere(vec3(10.5f, 0.0f, -10.0f), 1.0f));         // crosses the right plane
        CHECK(value.intersects_sphere(vec3(0.0f, 0.0f, -0.5f), 1.0f));           // crosses near
        CHECK(value.intersects_sphere(vec3(0.0f, 0.0f, -100.5f), 1.0f));         // crosses far
        CHECK(value.intersects_sphere(vec3(0.0f), 1000.0f));                      // encloses everything
        CHECK(value.intersects_sphere(vec3(0.0f, 0.0f, -10.0f), 0.0f));          // a point inside

        CHECK_FALSE(value.intersects_sphere(vec3(0.0f, 0.0f, 5.0f), 1.0f));      // behind
        CHECK_FALSE(value.intersects_sphere(vec3(0.0f, 0.0f, -0.5f), 0.1f));     // before near
        CHECK_FALSE(value.intersects_sphere(vec3(0.0f, 0.0f, -105.0f), 1.0f));   // beyond far
        CHECK_FALSE(value.intersects_sphere(vec3(15.0f, 0.0f, -10.0f), 1.0f));   // right of right
        CHECK_FALSE(value.intersects_sphere(vec3(0.0f, -15.0f, -10.0f), 1.0f));  // below bottom

        // A negative radius behaves like a point.
        CHECK(value.intersects_sphere(vec3(0.0f, 0.0f, -10.0f), -5.0f));
        CHECK_FALSE(value.intersects_sphere(vec3(0.0f, 0.0f, 5.0f), -5.0f));
    }

    TEST_CASE("frustum - default frustum and equality") {
        const frustum value = {};
        for (const plane &side : value.planes) {
            CHECK(side == plane{});
        }
        CHECK(value == frustum{});
        CHECK(camera_frustum() == camera_frustum());
        CHECK(camera_frustum() != value);
    }
}
