#include "conjure/testing/doctest.h"

#include "math_test.h"

#include "conjure/core/math/plane.h"

#include <format>
#include <string>

using namespace conjure;
using namespace conjure::math;

TEST_SUITE("core/plane") {

    TEST_CASE("plane - default plane is the ground plane facing up") {
        const plane ground = {};
        CHECK(ground.normal == vec3(0.0f, 1.0f, 0.0f));
        CHECK(ground.distance == 0.0f);
        CHECK(ground.signed_distance(vec3(0.0f, 3.0f, 0.0f)) == 3.0f);
        CHECK(ground.signed_distance(vec3(5.0f, -2.0f, 7.0f)) == -2.0f);
        CHECK(ground.signed_distance(vec3(1.0f, 0.0f, 1.0f)) == 0.0f);
    }

    TEST_CASE("plane - from_point_normal places the point on the plane and normalises") {
        const plane value = plane::from_point_normal(vec3(0.0f, 0.0f, -5.0f), vec3(0.0f, 0.0f, 2.0f));
        CHECK_NEARLY(value.normal, vec3(0.0f, 0.0f, 1.0f), math_test::TIGHT_EPSILON);
        CHECK(nearly_equal(value.distance, 5.0f, math_test::TIGHT_EPSILON));
        CHECK(nearly_zero(value.signed_distance(vec3(3.0f, 4.0f, -5.0f)), math_test::TIGHT_EPSILON));
        CHECK(nearly_equal(value.signed_distance(vec3(0.0f, 0.0f, 0.0f)), 5.0f, math_test::TIGHT_EPSILON));
        CHECK(nearly_equal(value.signed_distance(vec3(0.0f, 0.0f, -7.0f)), -2.0f, math_test::TIGHT_EPSILON));

        const plane tilted = plane::from_point_normal(vec3(1.0f, 1.0f, 1.0f), vec3(1.0f, 1.0f, 1.0f));
        CHECK(nearly_equal(glm::length(tilted.normal), 1.0f, math_test::TIGHT_EPSILON));
        CHECK(nearly_zero(tilted.signed_distance(vec3(1.0f, 1.0f, 1.0f)), math_test::TIGHT_EPSILON));
        CHECK(nearly_zero(tilted.signed_distance(vec3(3.0f, 0.0f, 0.0f)), math_test::TIGHT_EPSILON));
    }

    TEST_CASE("plane - from_point_normal with a zero normal falls back to the ground orientation") {
        const plane value = plane::from_point_normal(vec3(0.0f, 2.0f, 0.0f), vec3(0.0f));
        CHECK(value.normal == vec3(0.0f, 1.0f, 0.0f));
        CHECK(value.distance == -2.0f);
    }

    TEST_CASE("plane - from_points uses the counter-clockwise winding for the normal") {
        // a -> b -> c is counter-clockwise when viewed from above (+Y): the normal points up.
        const vec3 a = vec3(0.0f, 1.0f, 0.0f);
        const vec3 b = vec3(1.0f, 1.0f, -1.0f);
        const vec3 c = vec3(0.0f, 1.0f, -1.0f);
        const plane ground = plane::from_points(a, b, c);
        CHECK_NEARLY(ground.normal, vec3(0.0f, 1.0f, 0.0f), math_test::TIGHT_EPSILON);
        CHECK(nearly_equal(ground.distance, -1.0f, math_test::TIGHT_EPSILON));

        // Reversed winding flips the normal.
        const plane flipped = plane::from_points(a, c, b);
        CHECK_NEARLY(flipped.normal, vec3(0.0f, -1.0f, 0.0f), math_test::TIGHT_EPSILON);

        // Collinear points cannot define a plane: ground orientation through the first point.
        const vec3 start = vec3(0.0f, 3.0f, 0.0f);
        const vec3 step = vec3(1.0f, 0.0f, 0.0f);
        const plane collinear = plane::from_points(start, start + step, start + step * 2.0f);
        CHECK(collinear.normal == vec3(0.0f, 1.0f, 0.0f));
        CHECK(collinear.distance == -3.0f);
    }

    TEST_CASE("plane - normalize scales the whole equation and leaves a zero normal alone") {
        plane value = {};
        value.normal = vec3(0.0f, 3.0f, 4.0f);
        value.distance = 10.0f;
        const vec3 point = vec3(1.0f, 2.0f, 3.0f);
        const f32 sideBefore = value.signed_distance(point);

        value.normalize();
        CHECK_NEARLY(value.normal, vec3(0.0f, 0.6f, 0.8f), math_test::TIGHT_EPSILON);
        CHECK(nearly_equal(value.distance, 2.0f, math_test::TIGHT_EPSILON));
        CHECK(nearly_equal(value.signed_distance(point), sideBefore / 5.0f, math_test::TIGHT_EPSILON));

        plane degenerate = {};
        degenerate.normal = vec3(0.0f);
        degenerate.distance = 4.0f;
        const plane normalized = degenerate.normalized();
        CHECK(normalized == degenerate);
    }

    TEST_CASE("plane - project drops a point onto the plane") {
        const plane value = plane::from_point_normal(vec3(0.0f, 2.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
        CHECK_NEARLY(value.project(vec3(3.0f, 7.0f, -1.0f)), vec3(3.0f, 2.0f, -1.0f), math_test::TIGHT_EPSILON);
        CHECK_NEARLY(value.project(vec3(3.0f, -7.0f, -1.0f)), vec3(3.0f, 2.0f, -1.0f), math_test::TIGHT_EPSILON);
        CHECK_NEARLY(value.project(vec3(3.0f, 2.0f, -1.0f)), vec3(3.0f, 2.0f, -1.0f), math_test::TIGHT_EPSILON);

        const plane tilted = plane::from_point_normal(vec3(0.0f), vec3(1.0f, 1.0f, 0.0f));
        const vec3 projected = tilted.project(vec3(2.0f, 0.0f, 5.0f));
        CHECK(nearly_zero(tilted.signed_distance(projected), math_test::TIGHT_EPSILON));
        CHECK_NEARLY(projected, vec3(1.0f, -1.0f, 5.0f), math_test::TIGHT_EPSILON);
    }

    TEST_CASE("plane - equality, nearly_equal and formatting") {
        const plane a = plane::from_point_normal(vec3(0.0f, 1.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
        plane b = a;
        CHECK(a == b);
        b.distance += 1.0e-5f;
        CHECK(a != b);
        CHECK(nearly_equal(a, b, math_test::LOOSE_EPSILON));
        CHECK_FALSE(nearly_equal(a, b, 1.0e-7f));
        b.normal.x = 0.5f;
        CHECK_FALSE(nearly_equal(a, b, math_test::LOOSE_EPSILON));

        CHECK(std::format("{}", a) == "{normal=(0, 1, 0), distance=-1}");
        CHECK(std::format("{:.1f}", a) == "{normal=(0.0, 1.0, 0.0), distance=-1.0}");
    }
}
