#include "conjure/testing/doctest.h"

#include "math_test.h"

#include "conjure/core/math/aabb.h"

#include <array>
#include <cmath>
#include <format>
#include <string>

using namespace conjure;
using namespace conjure::math;

namespace {
    aabb unit_box() {
        return aabb::from_center_extents(vec3(0.0f), vec3(0.5f));
    }

    aabb box(vec3 min, vec3 max) {
        aabb result = {};
        result.min = min;
        result.max = max;
        return result;
    }
}

TEST_SUITE("core/aabb") {

    TEST_CASE("aabb - default box is inverted and invalid until a point is added") {
        aabb empty = {};
        CHECK(empty.min == vec3(F32_MAX));
        CHECK(empty.max == vec3(-F32_MAX));
        CHECK_FALSE(empty.is_valid());
        CHECK(empty.surface_area() == 0.0f);
        CHECK(empty.volume() == 0.0f);
        CHECK_FALSE(empty.contains(vec3(0.0f)));
        CHECK_FALSE(empty.intersects(unit_box()));
        CHECK_FALSE(unit_box().intersects(empty));

        empty.expand(vec3(1.0f, 2.0f, 3.0f));
        CHECK(empty.is_valid());
        CHECK(empty.min == vec3(1.0f, 2.0f, 3.0f));
        CHECK(empty.max == vec3(1.0f, 2.0f, 3.0f));
        CHECK(empty.contains(vec3(1.0f, 2.0f, 3.0f)));
        CHECK(empty.size() == vec3(0.0f));
    }

    TEST_CASE("aabb - center, extents and size") {
        const aabb value = box(vec3(-1.0f, 0.0f, 2.0f), vec3(3.0f, 4.0f, 6.0f));
        CHECK(value.center() == vec3(1.0f, 2.0f, 4.0f));
        CHECK(value.extents() == vec3(2.0f));
        CHECK(value.size() == vec3(4.0f));
        CHECK(value.is_valid());
    }

    TEST_CASE("aabb - expand grows to include points and boxes") {
        aabb value = unit_box();
        value.expand(vec3(2.0f, 0.0f, 0.0f));
        CHECK(value.max == vec3(2.0f, 0.5f, 0.5f));
        CHECK(value.min == vec3(-0.5f));

        value.expand(vec3(0.0f, -3.0f, 0.0f));
        CHECK(value.min == vec3(-0.5f, -3.0f, -0.5f));

        value.expand(box(vec3(-10.0f), vec3(-9.0f)));
        CHECK(value.min == vec3(-10.0f));
        CHECK(value.max == vec3(2.0f, 0.5f, 0.5f));

        const aabb before = value;
        value.expand(aabb{});   // invalid boxes are ignored
        CHECK(value == before);

        aabb fromInvalid = {};
        fromInvalid.expand(unit_box());
        CHECK(fromInvalid == unit_box());
    }

    TEST_CASE("aabb - inflated grows or shrinks every side") {
        const aabb grown = unit_box().inflated(0.5f);
        CHECK(grown.min == vec3(-1.0f));
        CHECK(grown.max == vec3(1.0f));
        const aabb shrunk = unit_box().inflated(-0.25f);
        CHECK(shrunk.min == vec3(-0.25f));
        CHECK(shrunk.max == vec3(0.25f));
        CHECK_FALSE(unit_box().inflated(-1.0f).is_valid());
    }

    TEST_CASE("aabb - contains is inclusive of the faces") {
        const aabb value = unit_box();
        CHECK(value.contains(vec3(0.0f)));
        CHECK(value.contains(vec3(0.5f, 0.5f, 0.5f)));
        CHECK(value.contains(vec3(-0.5f, 0.0f, 0.5f)));
        CHECK_FALSE(value.contains(vec3(0.5001f, 0.0f, 0.0f)));
        CHECK_FALSE(value.contains(vec3(0.0f, -0.6f, 0.0f)));
        CHECK_FALSE(value.contains(vec3(0.0f, 0.0f, 10.0f)));

        CHECK(value.contains(value));
        CHECK(value.contains(unit_box().inflated(-0.1f)));
        CHECK_FALSE(value.contains(unit_box().inflated(0.1f)));
        CHECK_FALSE(value.contains(aabb{}));
        CHECK_FALSE(aabb{}.contains(value));
    }

    TEST_CASE("aabb - intersects is inclusive of touching faces") {
        const aabb value = unit_box();
        CHECK(value.intersects(value));
        CHECK(value.intersects(box(vec3(0.5f, 0.0f, 0.0f), vec3(2.0f, 0.1f, 0.1f))));    // touching face
        CHECK(value.intersects(box(vec3(0.5f, 0.5f, 0.5f), vec3(1.0f))));                // touching corner
        CHECK(value.intersects(box(vec3(0.25f), vec3(2.0f))));                           // overlapping
        CHECK(value.intersects(box(vec3(-10.0f), vec3(10.0f))));                         // enclosing
        CHECK_FALSE(value.intersects(box(vec3(0.6f, 0.0f, 0.0f), vec3(2.0f, 0.1f, 0.1f))));
        CHECK_FALSE(value.intersects(box(vec3(0.0f, 0.0f, -5.0f), vec3(1.0f, 1.0f, -4.0f))));
        CHECK_FALSE(value.intersects(box(vec3(0.0f, 2.0f, 0.0f), vec3(1.0f, 3.0f, 1.0f))));
    }

    TEST_CASE("aabb - intersection is the overlap and invalid when disjoint") {
        const aabb a = box(vec3(0.0f), vec3(2.0f));
        const aabb b = box(vec3(1.0f, -1.0f, 1.0f), vec3(3.0f, 1.0f, 3.0f));
        const aabb overlap = a.intersection(b);
        CHECK(overlap.min == vec3(1.0f, 0.0f, 1.0f));
        CHECK(overlap.max == vec3(2.0f, 1.0f, 2.0f));
        CHECK(overlap == b.intersection(a));

        CHECK_FALSE(a.intersection(box(vec3(5.0f), vec3(6.0f))).is_valid());
        CHECK(a.intersection(a) == a);
    }

    TEST_CASE("aabb - closest_point and distance_squared") {
        const aabb value = unit_box();
        CHECK(value.closest_point(vec3(0.1f, 0.2f, 0.3f)) == vec3(0.1f, 0.2f, 0.3f));
        CHECK(value.closest_point(vec3(5.0f, 0.0f, 0.0f)) == vec3(0.5f, 0.0f, 0.0f));
        CHECK(value.closest_point(vec3(5.0f, -5.0f, 5.0f)) == vec3(0.5f, -0.5f, 0.5f));
        CHECK(value.distance_squared(vec3(0.0f)) == 0.0f);
        CHECK(value.distance_squared(vec3(0.5f, 0.0f, 0.0f)) == 0.0f);
        CHECK(nearly_equal(value.distance_squared(vec3(2.5f, 0.0f, 0.0f)), 4.0f));
        CHECK(nearly_equal(value.distance_squared(vec3(1.5f, 1.5f, 0.0f)), 2.0f));

        // An invalid box has no surface: the point is returned unchanged and the distance is 0
        // (clamping into the inverted default box used to return -F32_MAX and infinity).
        const aabb invalid = {};
        CHECK(invalid.closest_point(vec3(1.0f, -2.0f, 3.0f)) == vec3(1.0f, -2.0f, 3.0f));
        CHECK(invalid.distance_squared(vec3(1.0f, -2.0f, 3.0f)) == 0.0f);
        CHECK(unit_box().inflated(-1.0f).distance_squared(vec3(0.0f)) == 0.0f);
    }

    TEST_CASE("aabb - corners are enumerated with x, y, z as bits 0, 1, 2") {
        const aabb value = box(vec3(0.0f), vec3(1.0f, 2.0f, 3.0f));
        const std::array<vec3, 8> corners = value.corners();
        CHECK(corners[0] == vec3(0.0f, 0.0f, 0.0f));
        CHECK(corners[1] == vec3(1.0f, 0.0f, 0.0f));
        CHECK(corners[2] == vec3(0.0f, 2.0f, 0.0f));
        CHECK(corners[3] == vec3(1.0f, 2.0f, 0.0f));
        CHECK(corners[4] == vec3(0.0f, 0.0f, 3.0f));
        CHECK(corners[5] == vec3(1.0f, 0.0f, 3.0f));
        CHECK(corners[6] == vec3(0.0f, 2.0f, 3.0f));
        CHECK(corners[7] == vec3(1.0f, 2.0f, 3.0f));
        for (const vec3 &corner : corners) {
            CHECK(value.contains(corner));
        }
    }

    TEST_CASE("aabb - transformed by a translation moves the box") {
        const aabb moved = unit_box().transformed(glm::translate(mat4(1.0f), vec3(1.0f, 2.0f, 3.0f)));
        CHECK_NEARLY(moved.min, vec3(0.5f, 1.5f, 2.5f), math_test::TIGHT_EPSILON);
        CHECK_NEARLY(moved.max, vec3(1.5f, 2.5f, 3.5f), math_test::TIGHT_EPSILON);
    }

    TEST_CASE("aabb - transformed by a rotation encloses the rotated corners") {
        // A 45 degree yaw of the unit cube widens x and z to sqrt(2) and leaves y alone.
        const mat4 yaw = glm::mat4_cast(math_test::rotation_about(vec3(0.0f, 1.0f, 0.0f), QUARTER_PI));
        const aabb rotated = unit_box().transformed(yaw);
        const f32 halfDiagonal = std::sqrt(2.0f) * 0.5f;
        CHECK_NEARLY(rotated.min, vec3(-halfDiagonal, -0.5f, -halfDiagonal), math_test::TIGHT_EPSILON);
        CHECK_NEARLY(rotated.max, vec3(halfDiagonal, 0.5f, halfDiagonal), math_test::TIGHT_EPSILON);

        // A 90 degree yaw of an elongated box swaps its x and z sizes.
        const aabb elongated = box(vec3(-2.0f, -1.0f, -0.5f), vec3(2.0f, 1.0f, 0.5f));
        const mat4 quarterYaw = glm::mat4_cast(math_test::rotation_about(vec3(0.0f, 1.0f, 0.0f), HALF_PI));
        const aabb swapped = elongated.transformed(quarterYaw);
        CHECK_NEARLY(swapped.min, vec3(-0.5f, -1.0f, -2.0f), math_test::TIGHT_EPSILON);
        CHECK_NEARLY(swapped.max, vec3(0.5f, 1.0f, 2.0f), math_test::TIGHT_EPSILON);

        // Every transformed corner lies inside the result.
        const mat4 arbitrary = math_test::sample_transform().to_matrix();
        const aabb arbitraryBox = elongated.transformed(arbitrary);
        for (const vec3 &corner : elongated.corners()) {
            CHECK(arbitraryBox.inflated(math_test::LOOSE_EPSILON).contains(vec3(arbitrary * vec4(corner, 1.0f))));
        }
    }

    TEST_CASE("aabb - transformed scaling and mirroring keeps min below max") {
        const aabb scaled = unit_box().transformed(glm::scale(mat4(1.0f), vec3(2.0f, -4.0f, 1.0f)));
        CHECK(scaled.is_valid());
        CHECK_NEARLY(scaled.min, vec3(-1.0f, -2.0f, -0.5f), math_test::TIGHT_EPSILON);
        CHECK_NEARLY(scaled.max, vec3(1.0f, 2.0f, 0.5f), math_test::TIGHT_EPSILON);
    }

    TEST_CASE("aabb - transformed keeps an invalid box invalid") {
        const aabb invalid = aabb{}.transformed(glm::translate(mat4(1.0f), vec3(1.0f)));
        CHECK_FALSE(invalid.is_valid());
        CHECK(invalid == aabb{});
    }

    TEST_CASE("aabb - surface_area and volume") {
        CHECK(nearly_equal(unit_box().surface_area(), 6.0f));
        CHECK(nearly_equal(unit_box().volume(), 1.0f));

        const aabb value = box(vec3(0.0f), vec3(1.0f, 2.0f, 3.0f));
        CHECK(nearly_equal(value.surface_area(), 2.0f * (2.0f + 6.0f + 3.0f)));
        CHECK(nearly_equal(value.volume(), 6.0f));

        const aabb flat = box(vec3(0.0f), vec3(2.0f, 0.0f, 3.0f));
        CHECK(nearly_equal(flat.surface_area(), 12.0f));
        CHECK(flat.volume() == 0.0f);

        aabb point = {};
        point.expand(vec3(1.0f));
        CHECK(point.surface_area() == 0.0f);
    }

    TEST_CASE("aabb - from_center_extents and from_points build ordered boxes") {
        const aabb centered = aabb::from_center_extents(vec3(1.0f, 2.0f, 3.0f), vec3(1.0f, 0.5f, 2.0f));
        CHECK(centered.min == vec3(0.0f, 1.5f, 1.0f));
        CHECK(centered.max == vec3(2.0f, 2.5f, 5.0f));
        CHECK(centered.center() == vec3(1.0f, 2.0f, 3.0f));

        const aabb negativeExtents = aabb::from_center_extents(vec3(0.0f), vec3(-1.0f, 2.0f, -3.0f));
        CHECK(negativeExtents.is_valid());
        CHECK(negativeExtents.extents() == vec3(1.0f, 2.0f, 3.0f));

        const aabb unordered = aabb::from_points(vec3(3.0f, -1.0f, 2.0f), vec3(-1.0f, 4.0f, 0.0f));
        CHECK(unordered.min == vec3(-1.0f, -1.0f, 0.0f));
        CHECK(unordered.max == vec3(3.0f, 4.0f, 2.0f));
    }

    TEST_CASE("aabb - equality, nearly_equal and formatting") {
        const aabb a = unit_box();
        aabb b = unit_box();
        CHECK(a == b);
        b.max.z += 1.0e-5f;
        CHECK(a != b);
        CHECK(nearly_equal(a, b, math_test::LOOSE_EPSILON));
        CHECK_FALSE(nearly_equal(a, b, 1.0e-7f));

        CHECK(std::format("{}", box(vec3(0.0f), vec3(1.0f, 2.0f, 3.0f))) == "{min=(0, 0, 0), max=(1, 2, 3)}");
        CHECK(std::format("{:.1f}", unit_box()) == "{min=(-0.5, -0.5, -0.5), max=(0.5, 0.5, 0.5)}");
    }
}
