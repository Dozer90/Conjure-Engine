#include "conjure/testing/doctest.h"

#include "math_test.h"

#include "conjure/testing/assert_capture.h"

#include "conjure/core/math/ray.h"

#include "conjure/core/math/math.h"

#include <cmath>
#include <optional>

using namespace conjure;
using namespace conjure::math;

namespace {
    ray make_ray(vec3 origin, vec3 direction) {
        ray result = {};
        result.origin = origin;
        result.direction = glm::normalize(direction);
        return result;
    }

    aabb unit_box_at(vec3 center) {
        return aabb::from_center_extents(center, vec3(0.5f));
    }

    const vec3 FORWARD = vec3(0.0f, 0.0f, -1.0f);
}

TEST_SUITE("core/ray") {

    TEST_CASE("ray - default ray points down -Z from the origin and at() walks along it") {
        const ray value = {};
        CHECK(value.origin == vec3(0.0f));
        CHECK(value.direction == FORWARD);
        CHECK(value.at(0.0f) == vec3(0.0f));
        CHECK(value.at(2.5f) == vec3(0.0f, 0.0f, -2.5f));
        CHECK(make_ray(vec3(1.0f, 2.0f, 3.0f), vec3(0.0f, 2.0f, 0.0f)).at(4.0f) == vec3(1.0f, 6.0f, 3.0f));
    }

    TEST_CASE("ray - from_points normalises the direction") {
        const ray value = ray::from_points(vec3(1.0f, 0.0f, 0.0f), vec3(1.0f, 0.0f, -10.0f));
        CHECK(value.origin == vec3(1.0f, 0.0f, 0.0f));
        CHECK_NEARLY(value.direction, FORWARD, math_test::TIGHT_EPSILON);

        const ray degenerate = ray::from_points(vec3(2.0f), vec3(2.0f));
        CHECK(degenerate.origin == vec3(2.0f));
        CHECK(degenerate.direction == FORWARD);
    }

    TEST_CASE("ray - ray/aabb hit from outside returns the distance to the entry face") {
        const aabb box = unit_box_at(vec3(0.0f, 0.0f, -5.0f));
        const std::optional<f32> straight = ray{}.intersect(box);
        REQUIRE(straight.has_value());
        CHECK(nearly_equal(*straight, 4.5f, math_test::TIGHT_EPSILON));

        const ray diagonal = make_ray(vec3(-5.0f, 0.0f, -5.0f), vec3(1.0f, 0.0f, 0.0f));
        const std::optional<f32> side = diagonal.intersect(box);
        REQUIRE(side.has_value());
        CHECK(nearly_equal(*side, 4.5f, math_test::TIGHT_EPSILON));
        CHECK_NEARLY(diagonal.at(*side), vec3(-0.5f, 0.0f, -5.0f), math_test::TIGHT_EPSILON);

        const ray tilted = make_ray(vec3(3.0f, 3.0f, 3.0f), vec3(-1.0f, -1.0f, -1.0f));
        const std::optional<f32> cornerHit = tilted.intersect(unit_box_at(vec3(0.0f)));
        REQUIRE(cornerHit.has_value());
        CHECK_NEARLY(tilted.at(*cornerHit), vec3(0.5f), math_test::LOOSE_EPSILON);
    }

    TEST_CASE("ray - ray/aabb with the origin inside hits at t = 0") {
        const aabb box = unit_box_at(vec3(0.0f));
        CHECK(ray{}.intersect(box) == std::optional<f32>(0.0f));
        CHECK(make_ray(vec3(0.4f, -0.4f, 0.1f), vec3(1.0f, 1.0f, 0.0f)).intersect(box) == std::optional<f32>(0.0f));
        // On the surface counts as inside.
        CHECK(make_ray(vec3(0.5f, 0.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f)).intersect(box) == std::optional<f32>(0.0f));
    }

    TEST_CASE("ray - ray/aabb behind the origin misses") {
        const aabb box = unit_box_at(vec3(0.0f, 0.0f, 5.0f));   // behind a ray looking down -Z
        CHECK_FALSE(ray{}.intersect(box).has_value());
        CHECK_FALSE(make_ray(vec3(0.0f, 0.0f, 10.0f), vec3(0.0f, 0.0f, 1.0f)).intersect(box).has_value());
    }

    TEST_CASE("ray - ray/aabb parallel to a slab") {
        const aabb box = unit_box_at(vec3(0.0f, 0.0f, -5.0f));
        // Parallel to the x and y slabs and inside both: hits the near face.
        const std::optional<f32> aligned = make_ray(vec3(0.25f, -0.25f, 0.0f), FORWARD).intersect(box);
        REQUIRE(aligned.has_value());
        CHECK(nearly_equal(*aligned, 4.5f, math_test::TIGHT_EPSILON));
        // Parallel to the x slab but outside it: misses regardless of the other axes.
        CHECK_FALSE(make_ray(vec3(0.75f, 0.0f, 0.0f), FORWARD).intersect(box).has_value());
        CHECK_FALSE(make_ray(vec3(0.0f, -0.75f, 0.0f), FORWARD).intersect(box).has_value());
        // Grazing a face exactly is a hit (inclusive).
        const std::optional<f32> grazing = make_ray(vec3(0.5f, 0.0f, 0.0f), FORWARD).intersect(box);
        REQUIRE(grazing.has_value());
        CHECK(nearly_equal(*grazing, 4.5f, math_test::TIGHT_EPSILON));
    }

    TEST_CASE("ray - ray/aabb misses to the side and rejects invalid boxes") {
        const aabb box = unit_box_at(vec3(0.0f, 0.0f, -5.0f));
        CHECK_FALSE(make_ray(vec3(2.0f, 0.0f, 0.0f), FORWARD).intersect(box).has_value());
        CHECK_FALSE(make_ray(vec3(0.0f, 0.0f, 0.0f), vec3(1.0f, 0.0f, -1.0f)).intersect(box).has_value());
        CHECK_FALSE(ray{}.intersect(aabb{}).has_value());
    }

    TEST_CASE("ray - ray/sphere hits, misses and tangents") {
        const vec3 center = vec3(0.0f, 0.0f, -10.0f);
        const std::optional<f32> straight = ray{}.intersect_sphere(center, 2.0f);
        REQUIRE(straight.has_value());
        CHECK(nearly_equal(*straight, 8.0f, math_test::TIGHT_EPSILON));

        const std::optional<f32> offCenter = make_ray(vec3(1.0f, 0.0f, 0.0f), FORWARD).intersect_sphere(center, 2.0f);
        REQUIRE(offCenter.has_value());
        CHECK(nearly_equal(*offCenter, 10.0f - std::sqrt(3.0f), math_test::LOOSE_EPSILON));

        const std::optional<f32> tangent = make_ray(vec3(2.0f, 0.0f, 0.0f), FORWARD).intersect_sphere(center, 2.0f);
        REQUIRE(tangent.has_value());
        CHECK(nearly_equal(*tangent, 10.0f, 1.0e-3f));

        CHECK_FALSE(make_ray(vec3(2.1f, 0.0f, 0.0f), FORWARD).intersect_sphere(center, 2.0f).has_value());
        CHECK_FALSE(make_ray(vec3(0.0f), vec3(0.0f, 1.0f, 0.0f)).intersect_sphere(center, 2.0f).has_value());
    }

    TEST_CASE("ray - ray/sphere inside, behind and degenerate") {
        CHECK(ray{}.intersect_sphere(vec3(0.0f), 1.0f) == std::optional<f32>(0.0f));
        CHECK(ray{}.intersect_sphere(vec3(0.0f, 0.0f, 0.5f), 1.0f) == std::optional<f32>(0.0f));
        CHECK(ray{}.intersect_sphere(vec3(0.0f, 0.0f, -1.0f), 1.0f) == std::optional<f32>(0.0f));   // on the surface

        CHECK_FALSE(ray{}.intersect_sphere(vec3(0.0f, 0.0f, 10.0f), 2.0f).has_value());   // behind
        CHECK_FALSE(ray{}.intersect_sphere(vec3(0.0f, 0.0f, -10.0f), -1.0f).has_value()); // negative radius
        CHECK(ray{}.intersect_sphere(vec3(0.0f, 0.0f, -10.0f), 0.0f) == std::optional<f32>(10.0f));   // a point

        ray stationary = {};
        stationary.direction = vec3(0.0f);
        CHECK_FALSE(stationary.intersect_sphere(vec3(0.0f, 0.0f, -10.0f), 1.0f).has_value());
    }

    TEST_CASE("ray - ray/sphere with a non-unit direction scales t") {
        ray fast = {};
        fast.direction = vec3(0.0f, 0.0f, -2.0f);
        const std::optional<f32> hit = fast.intersect_sphere(vec3(0.0f, 0.0f, -10.0f), 2.0f);
        REQUIRE(hit.has_value());
        CHECK(nearly_equal(*hit, 4.0f, math_test::TIGHT_EPSILON));
        CHECK_NEARLY(fast.at(*hit), vec3(0.0f, 0.0f, -8.0f), math_test::TIGHT_EPSILON);
    }

    TEST_CASE("ray - ray/plane from both sides, parallel and behind") {
        const plane ground = plane::from_point_normal(vec3(0.0f, -2.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));

        const ray downward = make_ray(vec3(1.0f, 3.0f, 1.0f), vec3(0.0f, -1.0f, 0.0f));
        const std::optional<f32> fromAbove = downward.intersect_plane(ground);
        REQUIRE(fromAbove.has_value());
        CHECK(nearly_equal(*fromAbove, 5.0f, math_test::TIGHT_EPSILON));
        CHECK_NEARLY(downward.at(*fromAbove), vec3(1.0f, -2.0f, 1.0f), math_test::TIGHT_EPSILON);

        const ray upward = make_ray(vec3(0.0f, -5.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
        const std::optional<f32> fromBelow = upward.intersect_plane(ground);
        REQUIRE(fromBelow.has_value());
        CHECK(nearly_equal(*fromBelow, 3.0f, math_test::TIGHT_EPSILON));

        const ray slanted = make_ray(vec3(0.0f, 0.0f, 0.0f), vec3(1.0f, -1.0f, 0.0f));
        const std::optional<f32> slantedHit = slanted.intersect_plane(ground);
        REQUIRE(slantedHit.has_value());
        CHECK_NEARLY(slanted.at(*slantedHit), vec3(2.0f, -2.0f, 0.0f), math_test::TIGHT_EPSILON);

        const vec3 upDirection = vec3(0.0f, 1.0f, 0.0f);
        CHECK_FALSE(make_ray(vec3(0.0f, 3.0f, 0.0f), upDirection).intersect_plane(ground).has_value());   // away
        CHECK_FALSE(make_ray(vec3(0.0f, 3.0f, 0.0f), FORWARD).intersect_plane(ground).has_value());       // parallel
        CHECK_FALSE(make_ray(vec3(0.0f, -2.0f, 0.0f), FORWARD).intersect_plane(ground).has_value());      // in plane

        // Starting exactly on the plane and leaving it hits at t = 0.
        const ray leaving = make_ray(vec3(0.0f, -2.0f, 0.0f), upDirection);
        CHECK(leaving.intersect_plane(ground) == std::optional<f32>(0.0f));
    }

    TEST_CASE("ray - ray/triangle front face hit with culling") {
        // Counter-clockwise when seen from +Z, so the front faces +Z.
        const vec3 a = vec3(-1.0f, -1.0f, -5.0f);
        const vec3 b = vec3(1.0f, -1.0f, -5.0f);
        const vec3 c = vec3(0.0f, 1.0f, -5.0f);

        const std::optional<f32> hit = ray{}.intersect_triangle(a, b, c);
        REQUIRE(hit.has_value());
        CHECK(nearly_equal(*hit, 5.0f, math_test::TIGHT_EPSILON));

        const ray offCenter = make_ray(vec3(0.5f, -0.5f, 0.0f), FORWARD);
        REQUIRE(offCenter.intersect_triangle(a, b, c).has_value());
        CHECK(nearly_equal(*offCenter.intersect_triangle(a, b, c), 5.0f, math_test::TIGHT_EPSILON));

        // Misses outside the triangle.
        CHECK_FALSE(make_ray(vec3(0.9f, 0.9f, 0.0f), FORWARD).intersect_triangle(a, b, c).has_value());
        CHECK_FALSE(make_ray(vec3(0.0f, -1.5f, 0.0f), FORWARD).intersect_triangle(a, b, c).has_value());
        CHECK_FALSE(make_ray(vec3(5.0f, 0.0f, 0.0f), FORWARD).intersect_triangle(a, b, c).has_value());
    }

    TEST_CASE("ray - ray/triangle back faces are culled unless asked otherwise") {
        const vec3 a = vec3(-1.0f, -1.0f, -5.0f);
        const vec3 b = vec3(1.0f, -1.0f, -5.0f);
        const vec3 c = vec3(0.0f, 1.0f, -5.0f);

        // Swapping two vertices flips the winding so the ray now sees the back face.
        CHECK_FALSE(ray{}.intersect_triangle(a, c, b).has_value());
        const std::optional<f32> twoSided = ray{}.intersect_triangle(a, c, b, false);
        REQUIRE(twoSided.has_value());
        CHECK(nearly_equal(*twoSided, 5.0f, math_test::TIGHT_EPSILON));

        // Looking from behind the triangle (origin at z = -10 looking +Z) sees the back face too.
        const ray fromBehind = make_ray(vec3(0.0f, 0.0f, -10.0f), vec3(0.0f, 0.0f, 1.0f));
        CHECK_FALSE(fromBehind.intersect_triangle(a, b, c).has_value());
        CHECK(fromBehind.intersect_triangle(a, b, c, false) == std::optional<f32>(5.0f));
    }

    TEST_CASE("ray - ray/triangle edges, vertices, behind and degenerate") {
        const vec3 a = vec3(-1.0f, -1.0f, -5.0f);
        const vec3 b = vec3(1.0f, -1.0f, -5.0f);
        const vec3 c = vec3(0.0f, 1.0f, -5.0f);

        CHECK(make_ray(vec3(0.0f, -1.0f, 0.0f), FORWARD).intersect_triangle(a, b, c).has_value());   // on edge ab
        CHECK(make_ray(vec3(0.0f, 1.0f, 0.0f), FORWARD).intersect_triangle(a, b, c).has_value());    // on vertex c
        CHECK(make_ray(vec3(-1.0f, -1.0f, 0.0f), FORWARD).intersect_triangle(a, b, c).has_value());  // on vertex a

        // Triangle behind the ray.
        CHECK_FALSE(make_ray(vec3(0.0f, 0.0f, -10.0f), FORWARD).intersect_triangle(a, b, c, false).has_value());

        // Ray parallel to the triangle plane.
        const ray inPlane = make_ray(vec3(0.0f, 0.0f, -5.0f), vec3(1.0f, 0.0f, 0.0f));
        CHECK_FALSE(inPlane.intersect_triangle(a, b, c, false).has_value());

        // Degenerate triangles never hit.
        CHECK_FALSE(ray{}.intersect_triangle(a, a, a, false).has_value());
        CHECK_FALSE(ray{}.intersect_triangle(a, b, vec3(0.0f, -1.0f, -5.0f), false).has_value());   // collinear
    }

    TEST_CASE("ray - ray/triangle hits millimetre-sized triangles (regression: absolute determinant epsilon)") {
        // A facing triangle with 0.5 mm edges: the Moller-Trumbore determinant is about 1e-7, below
        // the old absolute 1e-6 cut-off, so it used to be culled as degenerate in both modes.
        const f32 size = 0.0005f;
        const vec3 a = vec3(-size, -size, -5.0f);
        const vec3 b = vec3(size, -size, -5.0f);
        const vec3 c = vec3(0.0f, size, -5.0f);

        const std::optional<f32> culled = ray{}.intersect_triangle(a, b, c);
        REQUIRE(culled.has_value());
        CHECK(nearly_equal(*culled, 5.0f, math_test::TIGHT_EPSILON));
        const std::optional<f32> twoSided = ray{}.intersect_triangle(a, b, c, false);
        REQUIRE(twoSided.has_value());
        CHECK(nearly_equal(*twoSided, 5.0f, math_test::TIGHT_EPSILON));

        // The back face is still culled and still visible two-sided at that size.
        CHECK_FALSE(ray{}.intersect_triangle(a, c, b).has_value());
        CHECK(ray{}.intersect_triangle(a, c, b, false).has_value());

        // A genuinely collinear millimetre triangle still misses.
        CHECK_FALSE(ray{}.intersect_triangle(a, b, vec3(0.0f, -size, -5.0f), false).has_value());
        CHECK_FALSE(ray{}.intersect_triangle(a, a, a).has_value());
    }

    TEST_CASE("ray - ray/triangle hits a small triangle at a grazing angle") {
        // A 5 mm triangle in the z = -5 plane seen 85 degrees off its normal: the ray comes in
        // almost parallel to the surface and the determinant shrinks by cos(85 degrees).
        const f32 size = 0.005f;
        const vec3 a = vec3(-size, -size, -5.0f);
        const vec3 b = vec3(size, -size, -5.0f);
        const vec3 c = vec3(0.0f, size, -5.0f);

        const f32 grazing = radians(85.0f);
        const vec3 direction = vec3(std::sin(grazing), 0.0f, -std::cos(grazing));
        const vec3 hitPoint = vec3(0.0f, -0.5f * size, -5.0f);   // inside the triangle
        const ray slanted = make_ray(hitPoint - direction * 2.0f, direction);

        const std::optional<f32> hit = slanted.intersect_triangle(a, b, c);
        REQUIRE(hit.has_value());
        CHECK(nearly_equal(*hit, 2.0f, math_test::LOOSE_EPSILON));
        CHECK_NEARLY(slanted.at(*hit), hitPoint, math_test::LOOSE_EPSILON);
        CHECK(slanted.intersect_triangle(a, b, c, false).has_value());
    }

    TEST_CASE("ray - ray/triangle with a non-unit direction scales t") {
        const vec3 a = vec3(-1.0f, -1.0f, -5.0f);
        const vec3 b = vec3(1.0f, -1.0f, -5.0f);
        const vec3 c = vec3(0.0f, 1.0f, -5.0f);

        ray fast = {};
        fast.direction = vec3(0.0f, 0.0f, -2.0f);
        const std::optional<f32> hit = fast.intersect_triangle(a, b, c);
        REQUIRE(hit.has_value());
        CHECK(nearly_equal(*hit, 2.5f, math_test::TIGHT_EPSILON));
        CHECK_NEARLY(fast.at(*hit), vec3(0.0f, 0.0f, -5.0f), math_test::TIGHT_EPSILON);

        ray slow = {};
        slow.direction = vec3(0.0f, 0.0f, -0.001f);
        const std::optional<f32> slowHit = slow.intersect_triangle(a, b, c, false);
        REQUIRE(slowHit.has_value());
        CHECK(nearly_equal(*slowHit, 5000.0f, 1.0e-2f));

        ray stationary = {};
        stationary.direction = vec3(0.0f);
        CHECK_FALSE(stationary.intersect_triangle(a, b, c, false).has_value());
    }

    TEST_CASE("ray - from_viewport with a perspective camera") {
        const mat4 projection = perspective(HALF_PI, 2.0f, 0.5f, 100.0f);   // 90 degrees vertical, aspect 2
        const mat4 view = look_at(vec3(0.0f), vec3(0.0f, 0.0f, -1.0f), vec3(0.0f, 1.0f, 0.0f));
        const mat4 inverseViewProjection = glm::inverse(projection * view);
        const uvec2 viewport = uvec2(800u, 400u);

        const ray center = ray::from_viewport(vec2(400.0f, 200.0f), viewport, inverseViewProjection);
        CHECK_NEARLY(center.origin, vec3(0.0f, 0.0f, -0.5f), math_test::LOOSE_EPSILON);
        CHECK_NEARLY(center.direction, FORWARD, math_test::LOOSE_EPSILON);
        CHECK(nearly_equal(glm::length(center.direction), 1.0f, math_test::TIGHT_EPSILON));

        // The top-left pixel corner goes up and to the left; with a 90 degree fov the near-plane
        // corner is at (-aspect * near, near, -near).
        const ray topLeft = ray::from_viewport(vec2(0.0f, 0.0f), viewport, inverseViewProjection);
        CHECK_NEARLY(topLeft.origin, vec3(-1.0f, 0.5f, -0.5f), math_test::LOOSE_EPSILON);
        CHECK_NEARLY(topLeft.direction, glm::normalize(vec3(-1.0f, 0.5f, -0.5f)), math_test::LOOSE_EPSILON);

        const ray bottomRight = ray::from_viewport(vec2(800.0f, 400.0f), viewport, inverseViewProjection);
        CHECK_NEARLY(bottomRight.origin, vec3(1.0f, -0.5f, -0.5f), math_test::LOOSE_EPSILON);
        CHECK(bottomRight.direction.x > 0.0f);
        CHECK(bottomRight.direction.y < 0.0f);

        // A point projected to a pixel is hit by the ray through that pixel.
        const vec3 target = vec3(3.0f, -2.0f, -20.0f);
        const std::optional<vec3> ndc = project(target, projection * view);
        REQUIRE(ndc.has_value());
        const vec2 pixel = vec2((ndc->x + 1.0f) * 0.5f * 800.0f, (1.0f - ndc->y) * 0.5f * 400.0f);
        const ray pick = ray::from_viewport(pixel, viewport, inverseViewProjection);
        const std::optional<f32> hit = pick.intersect_sphere(target, 0.01f);
        REQUIRE(hit.has_value());
        CHECK_NEARLY(pick.at(*hit), target, 1.0e-2f);
    }

    TEST_CASE("ray - from_viewport with a moved camera, an orthographic and an infinite projection") {
        const vec3 eye = vec3(5.0f, 2.0f, 7.0f);
        const vec3 target = vec3(0.0f, 0.0f, 0.0f);
        const mat4 view = look_at(eye, target, vec3(0.0f, 1.0f, 0.0f));
        const uvec2 viewport = uvec2(640u, 480u);

        const mat4 projection = perspective(radians(60.0f), 640.0f / 480.0f, 0.1f, 50.0f);
        const ray centerRay = ray::from_viewport(vec2(320.0f, 240.0f), viewport, glm::inverse(projection * view));
        CHECK_NEARLY(centerRay.direction, glm::normalize(target - eye), math_test::LOOSE_EPSILON);
        CHECK(nearly_equal(glm::distance(centerRay.origin, eye), 0.1f, 1.0e-3f));

        const mat4 ortho = orthographic(-4.0f, 4.0f, -3.0f, 3.0f, 0.1f, 50.0f);
        const ray orthoCorner = ray::from_viewport(vec2(0.0f, 0.0f), viewport, glm::inverse(ortho * view));
        CHECK_NEARLY(orthoCorner.direction, glm::normalize(target - eye), math_test::LOOSE_EPSILON);
        const vec3 expectedOrigin = eye + glm::normalize(target - eye) * 0.1f
          + transform::look_at(eye, target, vec3(0.0f, 1.0f, 0.0f)).transform_direction(vec3(-4.0f, 3.0f, 0.0f));
        CHECK_NEARLY(orthoCorner.origin, expectedOrigin, 1.0e-3f);

        const mat4 infinite = infinite_perspective(radians(60.0f), 640.0f / 480.0f, 0.1f);
        const ray infiniteRay = ray::from_viewport(vec2(100.0f, 400.0f), viewport, glm::inverse(infinite * view));
        CHECK(nearly_equal(glm::length(infiniteRay.direction), 1.0f, math_test::TIGHT_EPSILON));
        CHECK(std::isfinite(infiniteRay.origin.x));
        CHECK(std::isfinite(infiniteRay.origin.y));
        CHECK(std::isfinite(infiniteRay.origin.z));
        const ray finiteRay = ray::from_viewport(vec2(100.0f, 400.0f), viewport, glm::inverse(projection * view));
        CHECK_NEARLY(infiniteRay.direction, finiteRay.direction, math_test::LOOSE_EPSILON);
        CHECK_NEARLY(infiniteRay.origin, finiteRay.origin, 1.0e-3f);
    }

    TEST_CASE("ray - from_viewport with a zero-sized viewport asserts and uses the centre") {
        const mat4 projection = perspective(HALF_PI, 1.0f, 0.5f, 100.0f);
        testing::expect_assert asserts = {};
        const ray value = ray::from_viewport(vec2(10.0f, 10.0f), uvec2(0u, 100u), glm::inverse(projection));
        CHECK(asserts.count() == 1);
        CHECK_NEARLY(value.direction, FORWARD, math_test::LOOSE_EPSILON);
        CHECK_NEARLY(value.origin, vec3(0.0f, 0.0f, -0.5f), math_test::LOOSE_EPSILON);
    }

    TEST_CASE("ray - from_viewport with a singular matrix asserts and returns the default ray") {
        testing::expect_assert asserts = {};
        const ray value = ray::from_viewport(vec2(1.0f, 1.0f), uvec2(10u, 10u), mat4(0.0f));
        CHECK(asserts.count() == 1);
        CHECK(value == ray{});
    }

    TEST_CASE("ray - equality") {
        CHECK(ray{} == ray{});
        CHECK(make_ray(vec3(1.0f), FORWARD) != ray{});
        CHECK(make_ray(vec3(0.0f), vec3(0.0f, 0.0f, -3.0f)) == ray{});
    }
}
