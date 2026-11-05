#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"

#include "conjure/core/math.h"
#include "conjure/core/uuid.h"

#include <format>
#include <string>

using namespace conjure;
using namespace conjure::math;
using namespace conjure::testing;

TEST_SUITE("testing/approx") {

    TEST_CASE("approx_equal - scalars inside and outside the tolerance") {
        CHECK(approx_equal(1.0f, 1.0f + 0.5f * testing::EPSILON));
        CHECK_FALSE(approx_equal(1.0f, 1.0f + 10.0f * testing::EPSILON));
        CHECK(approx_equal(1.0f, 1.5f, 0.5f));
        CHECK_FALSE(approx_equal(1.0f, 1.6f, 0.5f));
    }

    TEST_CASE("approx - vectors inside and outside the tolerance") {
        const vec3 base(1.0f, 2.0f, 3.0f);

        CHECK(approx_equal(base, base + vec3(0.5f * testing::EPSILON)));
        CHECK_FALSE(approx_equal(base, base + vec3(0.0f, 0.0f, 10.0f * testing::EPSILON)));
        CHECK(approx_equal(vec2(1.0f, 2.0f), vec2(1.0f, 2.0f)));
        CHECK_FALSE(approx_equal(vec2(1.0f, 2.0f), vec2(1.0f, 2.1f)));
        CHECK(approx_equal(vec4(1.0f, 2.0f, 3.0f, 4.0f), vec4(1.0f, 2.0f, 3.0f, 4.0f)));
        CHECK_FALSE(approx_equal(vec4(1.0f, 2.0f, 3.0f, 4.0f), vec4(1.0f, 2.0f, 3.0f, 4.1f)));
        CHECK(approx_equal(base, base + vec3(0.05f), 0.1f));
    }

    TEST_CASE("approx - quaternion sign equivalence") {
        const quat rotation = glm::angleAxis(radians(30.0f), vec3(0.0f, 1.0f, 0.0f));
        const quat negated(-rotation.w, -rotation.x, -rotation.y, -rotation.z);

        CHECK(approx_equal(rotation, rotation));
        CHECK(approx_equal(rotation, negated));
        CHECK_FALSE(approx_equal(rotation, glm::angleAxis(radians(31.0f), vec3(0.0f, 1.0f, 0.0f))));
        CHECK_FALSE(nearly_equal(rotation, negated));
    }

    TEST_CASE("approx - matrices") {
        const mat4 identity(1.0f);
        mat4 nudged = identity;
        nudged[3][1] = 10.0f * testing::EPSILON;

        CHECK(approx_equal(identity, mat4(1.0f)));
        CHECK_FALSE(approx_equal(identity, nudged));
        CHECK(approx_equal(identity, nudged, 1.0e-3f));

        const mat3 small(1.0f);
        mat3 smallNudged = small;
        smallNudged[2][2] = 2.0f;
        CHECK(approx_equal(small, mat3(1.0f)));
        CHECK_FALSE(approx_equal(small, smallNudged));
    }

    TEST_CASE("approx - bounding boxes and transforms") {
        const aabb box = aabb::from_center_extents(vec3(0.0f), vec3(1.0f));
        aabb bigger = box;
        bigger.max += vec3(0.1f);
        CHECK(approx_equal(box, box));
        CHECK_FALSE(approx_equal(box, bigger));

        transform left = {};
        left.position = vec3(1.0f, 2.0f, 3.0f);
        transform right = left;
        right.rotation = quat(-1.0f, 0.0f, 0.0f, 0.0f);   // -identity: same rotation
        CHECK(approx_equal(left, right));
        right.scale = vec3(2.0f);
        CHECK_FALSE(approx_equal(left, right));
    }

    TEST_CASE("CONJURE_CHECK_APPROX - accepts values within tolerance") {
        CONJURE_CHECK_APPROX(vec3(1.0f, 0.0f, 0.0f), vec3(1.0f + 0.1f * testing::EPSILON, 0.0f, 0.0f));
        CONJURE_CHECK_APPROX(2.0f, 2.0f);
    }

    TEST_CASE("StringMaker - prints vec3 and uuid") {
        const vec3 value(1.5f, -2.0f, 0.25f);
        const std::string vectorText = doctest::toString(value).c_str();
        CHECK(vectorText == std::format("{}", value));
        CHECK(vectorText.find("1.5") != std::string::npos);

        const core::uuid id(0x0123456789ABCDEFull, 0xFEDCBA9876543210ull);
        const std::string idText = doctest::toString(id).c_str();
        CHECK(idText == id.to_string());
        CHECK(idText == "01234567-89ab-cdef-fedc-ba9876543210");

        const quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
        CHECK(std::string(doctest::toString(rotation).c_str()) == std::format("{}", rotation));
    }
}
