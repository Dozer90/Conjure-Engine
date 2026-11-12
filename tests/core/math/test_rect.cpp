#include "conjure/testing/doctest.h"

#include "math_test.h"

#include "conjure/testing/assert_capture.h"

#include "conjure/core/math/rect.h"

#include <format>
#include <string>

using namespace conjure;
using namespace conjure::math;

namespace {
    rect make_rect(f32 minX, f32 minY, f32 maxX, f32 maxY) {
        rect result = {};
        result.min = vec2(minX, minY);
        result.max = vec2(maxX, maxY);
        return result;
    }

    urect make_urect(u32 x, u32 y, u32 width, u32 height) {
        urect result = {};
        result.x = x;
        result.y = y;
        result.width = width;
        result.height = height;
        return result;
    }
}

TEST_SUITE("core/rect") {

    TEST_CASE("rect - default rect is empty, valid and at the origin") {
        const rect value = {};
        CHECK(value.min == vec2(0.0f));
        CHECK(value.max == vec2(0.0f));
        CHECK(value.is_valid());
        CHECK(value.is_empty());
        CHECK(value.area() == 0.0f);
        CHECK(value.contains(vec2(0.0f)));
        CHECK_FALSE(value.contains(vec2(0.1f, 0.0f)));
    }

    TEST_CASE("rect - size, center, width, height and area") {
        const rect value = make_rect(1.0f, 2.0f, 4.0f, 8.0f);
        CHECK(value.width() == 3.0f);
        CHECK(value.height() == 6.0f);
        CHECK(value.size() == vec2(3.0f, 6.0f));
        CHECK(value.center() == vec2(2.5f, 5.0f));
        CHECK(value.area() == 18.0f);
        CHECK_FALSE(value.is_empty());

        const rect inverted = make_rect(4.0f, 2.0f, 1.0f, 8.0f);
        CHECK_FALSE(inverted.is_valid());
        CHECK(inverted.is_empty());
        CHECK(inverted.area() == 0.0f);

        CHECK(make_rect(0.0f, 0.0f, 5.0f, 0.0f).is_empty());
    }

    TEST_CASE("rect - contains is inclusive of the edges") {
        const rect value = make_rect(0.0f, 0.0f, 10.0f, 5.0f);
        CHECK(value.contains(vec2(0.0f, 0.0f)));
        CHECK(value.contains(vec2(10.0f, 5.0f)));
        CHECK(value.contains(vec2(5.0f, 2.5f)));
        CHECK_FALSE(value.contains(vec2(10.1f, 2.5f)));
        CHECK_FALSE(value.contains(vec2(5.0f, -0.1f)));

        CHECK(value.contains(value));
        CHECK(value.contains(make_rect(1.0f, 1.0f, 9.0f, 4.0f)));
        CHECK_FALSE(value.contains(make_rect(1.0f, 1.0f, 11.0f, 4.0f)));
        CHECK_FALSE(value.contains(make_rect(5.0f, 5.0f, 1.0f, 1.0f)));   // invalid other
        CHECK_FALSE(make_rect(5.0f, 5.0f, 1.0f, 1.0f).contains(value));
    }

    TEST_CASE("rect - intersects is inclusive of touching edges") {
        const rect value = make_rect(0.0f, 0.0f, 10.0f, 5.0f);
        CHECK(value.intersects(value));
        CHECK(value.intersects(make_rect(10.0f, 0.0f, 20.0f, 5.0f)));     // touching edge
        CHECK(value.intersects(make_rect(10.0f, 5.0f, 20.0f, 10.0f)));    // touching corner
        CHECK(value.intersects(make_rect(5.0f, 2.0f, 15.0f, 3.0f)));
        CHECK(value.intersects(make_rect(-1.0f, -1.0f, 11.0f, 6.0f)));
        CHECK_FALSE(value.intersects(make_rect(10.5f, 0.0f, 20.0f, 5.0f)));
        CHECK_FALSE(value.intersects(make_rect(0.0f, 6.0f, 10.0f, 7.0f)));
        CHECK_FALSE(value.intersects(make_rect(5.0f, 5.0f, 1.0f, 1.0f)));   // invalid other
    }

    TEST_CASE("rect - intersection is the overlap and invalid when disjoint") {
        const rect a = make_rect(0.0f, 0.0f, 10.0f, 10.0f);
        const rect b = make_rect(5.0f, -5.0f, 15.0f, 5.0f);
        CHECK(a.intersection(b) == make_rect(5.0f, 0.0f, 10.0f, 5.0f));
        CHECK(b.intersection(a) == make_rect(5.0f, 0.0f, 10.0f, 5.0f));
        CHECK(a.intersection(a) == a);
        CHECK_FALSE(a.intersection(make_rect(20.0f, 20.0f, 30.0f, 30.0f)).is_valid());
    }

    TEST_CASE("rect - expand grows to include points and rects") {
        rect value = {};
        value.expand(vec2(-1.0f, 2.0f));
        CHECK(value == make_rect(-1.0f, 0.0f, 0.0f, 2.0f));
        value.expand(make_rect(5.0f, 5.0f, 6.0f, 6.0f));
        CHECK(value == make_rect(-1.0f, 0.0f, 6.0f, 6.0f));

        const rect before = value;
        value.expand(make_rect(9.0f, 9.0f, 1.0f, 1.0f));   // invalid rects are ignored
        CHECK(value == before);
    }

    TEST_CASE("rect - translated, inflated and closest_point") {
        const rect value = make_rect(0.0f, 0.0f, 2.0f, 4.0f);
        CHECK(value.translated(vec2(1.0f, -1.0f)) == make_rect(1.0f, -1.0f, 3.0f, 3.0f));
        CHECK(value.inflated(1.0f) == make_rect(-1.0f, -1.0f, 3.0f, 5.0f));
        CHECK(value.inflated(-1.0f) == make_rect(1.0f, 1.0f, 1.0f, 3.0f));
        CHECK_FALSE(value.inflated(-3.0f).is_valid());

        CHECK(value.closest_point(vec2(1.0f, 1.0f)) == vec2(1.0f, 1.0f));
        CHECK(value.closest_point(vec2(5.0f, -3.0f)) == vec2(2.0f, 0.0f));
        CHECK(value.closest_point(vec2(-5.0f, 10.0f)) == vec2(0.0f, 4.0f));
    }

    TEST_CASE("regression - sweep-2026-11-12 F220: closest_point of an invalid rect returns the point unchanged") {
        const rect inverted = make_rect(2.0f, 2.0f, 0.0f, 0.0f);
        REQUIRE_FALSE(inverted.is_valid());
        CHECK(inverted.closest_point(vec2(5.0f, -1.0f)) == vec2(5.0f, -1.0f));
        CHECK(inverted.closest_point(vec2(1.0f, 1.0f)) == vec2(1.0f, 1.0f));

        // Invalid on one axis only is still invalid.
        const rect flipped = make_rect(0.0f, 3.0f, 4.0f, 1.0f);
        CHECK(flipped.closest_point(vec2(9.0f, 9.0f)) == vec2(9.0f, 9.0f));
    }

    TEST_CASE("rect - factories build ordered rects") {
        CHECK(rect::from_position_size(vec2(1.0f, 2.0f), vec2(3.0f, 4.0f)) == make_rect(1.0f, 2.0f, 4.0f, 6.0f));
        CHECK(rect::from_position_size(vec2(1.0f, 2.0f), vec2(-3.0f, 4.0f)) == make_rect(-2.0f, 2.0f, 1.0f, 6.0f));
        const rect leftward = rect::from_position_size(vec2(10.0f, 10.0f), vec2(-4.0f, 2.0f));
        CHECK(leftward == make_rect(6.0f, 10.0f, 10.0f, 12.0f));
        CHECK(rect::from_center_extents(vec2(0.0f), vec2(1.0f, 2.0f)) == make_rect(-1.0f, -2.0f, 1.0f, 2.0f));
        CHECK(rect::from_center_extents(vec2(0.0f), vec2(-1.0f, 2.0f)) == make_rect(-1.0f, -2.0f, 1.0f, 2.0f));
        CHECK(rect::from_points(vec2(4.0f, 1.0f), vec2(1.0f, 4.0f)) == make_rect(1.0f, 1.0f, 4.0f, 4.0f));
    }

    TEST_CASE("rect - rect equality, nearly_equal and formatting") {
        const rect a = make_rect(0.0f, 0.0f, 1.0f, 1.0f);
        rect b = a;
        CHECK(a == b);
        b.max.x += 1.0e-5f;
        CHECK(a != b);
        CHECK(nearly_equal(a, b, math_test::LOOSE_EPSILON));
        CHECK_FALSE(nearly_equal(a, b, 1.0e-7f));

        CHECK(std::format("{}", make_rect(0.0f, 1.0f, 2.5f, 3.0f)) == "{min=(0, 1), max=(2.5, 3)}");
        CHECK(std::format("{:.1f}", a) == "{min=(0.0, 0.0), max=(1.0, 1.0)}");
    }

    TEST_CASE("rect - urect edges are exclusive on the right and bottom") {
        const urect viewport = make_urect(10, 20, 100, 50);
        CHECK(viewport.right() == 110);
        CHECK(viewport.bottom() == 70);
        CHECK(viewport.area() == 5000);
        CHECK_FALSE(viewport.is_empty());

        CHECK(viewport.contains(10, 20));
        CHECK(viewport.contains(109, 69));
        CHECK_FALSE(viewport.contains(110, 69));
        CHECK_FALSE(viewport.contains(109, 70));
        CHECK_FALSE(viewport.contains(9, 20));
        CHECK_FALSE(viewport.contains(10, 19));

        const urect empty = {};
        CHECK(empty.is_empty());
        CHECK(empty.area() == 0);
        CHECK_FALSE(empty.contains(0, 0));
        CHECK(make_urect(0, 0, 5, 0).is_empty());
    }

    TEST_CASE("rect - urect area does not overflow 32 bits") {
        const urect huge = make_urect(0, 0, 0xFFFFFFFFu, 0xFFFFFFFFu);
        CHECK(huge.area() == u64{0xFFFFFFFFu} * u64{0xFFFFFFFFu});
        CHECK(huge.right() == 0xFFFFFFFFu);
        CHECK(huge.bottom() == 0xFFFFFFFFu);
    }

    TEST_CASE("rect - urect touching the top of the u32 range keeps its edges") {
        // x + width == 0xFFFFFFFF is the largest representable edge and must not assert.
        const urect top = make_urect(0xFFFFFFF0u, 0xFFFFFFFEu, 15, 1);
        CHECK(top.right() == 0xFFFFFFFFu);
        CHECK(top.bottom() == 0xFFFFFFFFu);
        CHECK(top.contains(0xFFFFFFFEu, 0xFFFFFFFEu));
        CHECK_FALSE(top.contains(0xFFFFFFFFu, 0xFFFFFFFEu));
        CHECK(top.intersects(make_urect(0xFFFFFFFEu, 0xFFFFFFFEu, 1, 1)));
        const urect column = make_urect(0xFFFFFFF8u, 0, 7, 0xFFFFFFFFu);   // reaches the top edge too
        CHECK(top.intersection(column) == make_urect(0xFFFFFFF8u, 0xFFFFFFFEu, 7, 1));
        CHECK(top.to_rect().max == vec2(4294967295.0f));
    }

    TEST_CASE("rect - urect edges beyond the u32 range assert instead of wrapping") {
        testing::expect_assert asserts = {};
        const urect overflowing = make_urect(0xFFFFFFFEu, 0, 4, 1);
        const u32 wrapped = overflowing.right();
        CHECK(asserts.count() == 1);
        CHECK(wrapped == 2);   // the arithmetic still wraps; the assert is the diagnostic
        CHECK(overflowing.bottom() == 1);
        CHECK(asserts.count() == 1);

        const urect tall = make_urect(0, 0xFFFFFFFFu, 1, 1);
        CHECK(tall.bottom() == 0);
        CHECK(asserts.count() == 2);
    }

    TEST_CASE("rect - urect containment and intersection") {
        const urect a = make_urect(0, 0, 10, 10);
        CHECK(a.contains(a));
        CHECK(a.contains(make_urect(2, 2, 8, 8)));
        CHECK_FALSE(a.contains(make_urect(2, 2, 9, 8)));
        CHECK(a.contains(make_urect(10, 10, 0, 0)));   // empty rect on the edge

        CHECK(a.intersects(a));
        CHECK(a.intersects(make_urect(9, 9, 5, 5)));
        CHECK_FALSE(a.intersects(make_urect(10, 0, 5, 5)));   // starts at the exclusive edge
        CHECK_FALSE(a.intersects(make_urect(0, 10, 5, 5)));
        CHECK_FALSE(a.intersects(make_urect(5, 5, 0, 5)));    // empty never intersects
        CHECK_FALSE(urect{}.intersects(a));

        CHECK(a.intersection(make_urect(5, 5, 10, 10)) == make_urect(5, 5, 5, 5));
        CHECK(a.intersection(make_urect(5, 5, 10, 10)) == make_urect(5, 5, 10, 10).intersection(a));
        CHECK(a.intersection(a) == a);

        const urect disjoint = a.intersection(make_urect(20, 20, 5, 5));
        CHECK(disjoint.is_empty());
        CHECK(disjoint.x == 20);
        CHECK(disjoint.y == 20);
    }

    TEST_CASE("rect - urect converts to a float rect") {
        const rect converted = make_urect(10, 20, 30, 40).to_rect();
        CHECK(converted == make_rect(10.0f, 20.0f, 40.0f, 60.0f));
        CHECK(urect{}.to_rect() == rect{});
    }

    TEST_CASE("rect - urect equality and formatting") {
        CHECK(make_urect(1, 2, 3, 4) == make_urect(1, 2, 3, 4));
        CHECK(make_urect(1, 2, 3, 4) != make_urect(1, 2, 3, 5));
        CHECK(std::format("{}", make_urect(1, 2, 3, 4)) == "{x=1, y=2, width=3, height=4}");
        const std::string padded = std::format("{:>40}", urect{});
        CHECK(padded.size() == 40);
        CHECK(padded.ends_with("{x=0, y=0, width=0, height=0}"));
    }
}
