#include "conjure/testing/doctest.h"

#include "math_test.h"

#include "conjure/core/math/glm_types.h"

#include <array>
#include <bit>
#include <format>
#include <string>
#include <type_traits>

using namespace conjure;
using namespace conjure::math;

TEST_SUITE("core/glm_types") {

    TEST_CASE("glm_types - aliases are the expected glm types with unpadded layouts") {
        static_assert(std::is_same_v<vec3, glm::vec3>);
        static_assert(std::is_same_v<quat, glm::quat>);
        static_assert(std::is_same_v<mat4, glm::mat4>);
        static_assert(std::is_same_v<uvec2, glm::uvec2>);
        static_assert(std::is_same_v<ivec3::value_type, i32>);
        static_assert(std::is_same_v<uvec3::value_type, u32>);
        static_assert(std::is_same_v<vec4::value_type, f32>);
        static_assert(std::is_trivially_copyable_v<vec3> && std::is_trivially_copyable_v<quat>);
        static_assert(std::is_trivially_copyable_v<mat4>);

        CHECK(sizeof(vec2) == 8);
        CHECK(sizeof(vec3) == 12);
        CHECK(sizeof(vec4) == 16);
        CHECK(sizeof(ivec2) == 8);
        CHECK(sizeof(uvec3) == 12);
        CHECK(sizeof(quat) == 16);
        CHECK(sizeof(mat3) == 36);
        CHECK(sizeof(mat4) == 64);
    }

    TEST_CASE("glm_types - quaternions are constructed as (w, x, y, z) and stored as x, y, z, w") {
        const quat rotation = quat(1.0f, 2.0f, 3.0f, 4.0f);
        CHECK(rotation.w == 1.0f);
        CHECK(rotation.x == 2.0f);
        CHECK(rotation.y == 3.0f);
        CHECK(rotation.z == 4.0f);

        const std::array<f32, 4> memory = std::bit_cast<std::array<f32, 4>>(rotation);
        CHECK(memory == std::array<f32, 4>{2.0f, 3.0f, 4.0f, 1.0f});

        const quat identity = quat(1.0f, 0.0f, 0.0f, 0.0f);
        CHECK(identity * vec3(1.0f, 2.0f, 3.0f) == vec3(1.0f, 2.0f, 3.0f));
    }

    TEST_CASE("glm_types - glm is configured for radians, 0..1 depth and explicit constructors") {
        // Radians: 90 degrees as pi/2 rotates +X onto +Y about Z.
        const quat quarterTurn = glm::angleAxis(HALF_PI, vec3(0.0f, 0.0f, 1.0f));
        CHECK_NEARLY(quarterTurn * vec3(1.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f), math_test::TIGHT_EPSILON);

        // Zero-to-one depth: the near plane projects to depth 0.
        const mat4 projection = glm::perspective(radians(60.0f), 1.0f, 0.5f, 100.0f);
        const vec4 nearClip = projection * vec4(0.0f, 0.0f, -0.5f, 1.0f);
        CHECK(nearly_equal(nearClip.z / nearClip.w, 0.0f, math_test::TIGHT_EPSILON));
        const vec4 farClip = projection * vec4(0.0f, 0.0f, -100.0f, 1.0f);
        CHECK(nearly_equal(farClip.z / farClip.w, 1.0f, math_test::TIGHT_EPSILON));

        // Explicit constructors: a scalar does not implicitly convert to a vector.
        static_assert(!std::is_convertible_v<f32, vec3>);
        static_assert(!std::is_convertible_v<vec4, vec3>);
        static_assert(std::is_constructible_v<vec3, f32>);
    }

    TEST_CASE("glm_types - nearly_equal for vectors compares every component") {
        CHECK(nearly_equal(vec2(1.0f, 2.0f), vec2(1.0f, 2.0f)));
        CHECK_FALSE(nearly_equal(vec2(1.0f, 2.0f), vec2(1.0f, 2.1f)));
        CHECK(nearly_equal(vec2(1.0f, 2.0f), vec2(1.0f, 2.1f), 0.2f));

        CHECK(nearly_equal(vec3(1.0f, 2.0f, 3.0f), vec3(1.0f, 2.0f, 3.0f + 0.5f * EPSILON)));
        CHECK_FALSE(nearly_equal(vec3(1.0f, 2.0f, 3.0f), vec3(1.0f, 2.0f, 3.01f)));
        CHECK_FALSE(nearly_equal(vec3(1.0f, 2.0f, 3.0f), vec3(1.01f, 2.0f, 3.0f)));
        CHECK_FALSE(nearly_equal(vec3(1.0f, 2.0f, 3.0f), vec3(1.0f, 2.01f, 3.0f)));

        CHECK(nearly_equal(vec4(1.0f), vec4(1.0f)));
        CHECK_FALSE(nearly_equal(vec4(1.0f, 1.0f, 1.0f, 1.0f), vec4(1.0f, 1.0f, 1.0f, 1.5f)));
        CHECK(nearly_equal(vec4(1.0f, 1.0f, 1.0f, 1.0f), vec4(1.0f, 1.0f, 1.0f, 1.5f), 0.6f));
    }

    TEST_CASE("glm_types - nearly_equal for quaternions is component-wise while same_rotation ignores sign") {
        const quat rotation = glm::angleAxis(radians(40.0f), glm::normalize(vec3(1.0f, 1.0f, 0.0f)));
        const quat negated = -rotation;

        CHECK(nearly_equal(rotation, rotation));
        CHECK_FALSE(nearly_equal(rotation, negated));
        CHECK(same_rotation(rotation, negated));
        CHECK(same_rotation(rotation, rotation));

        const quat other = glm::angleAxis(radians(41.0f), glm::normalize(vec3(1.0f, 1.0f, 0.0f)));
        CHECK_FALSE(nearly_equal(rotation, other));
        CHECK_FALSE(same_rotation(rotation, other));
        CHECK(same_rotation(rotation, other, 0.01f));
    }

    TEST_CASE("glm_types - nearly_equal for matrices compares every element") {
        mat4 matrix = mat4(1.0f);
        CHECK(nearly_equal(matrix, mat4(1.0f)));
        matrix[2][1] = 0.001f;
        CHECK_FALSE(nearly_equal(matrix, mat4(1.0f)));
        CHECK(nearly_equal(matrix, mat4(1.0f), 0.01f));

        mat3 small = mat3(1.0f);
        CHECK(nearly_equal(small, mat3(1.0f)));
        small[0][2] = -0.5f;
        CHECK_FALSE(nearly_equal(small, mat3(1.0f)));
    }

    TEST_CASE("glm_types - nearly_zero for vectors uses the length") {
        CHECK(nearly_zero(vec3(0.0f)));
        CHECK(nearly_zero(vec3(0.5f * EPSILON, 0.0f, 0.0f)));
        CHECK_FALSE(nearly_zero(vec3(0.0f, 2.0f * EPSILON, 0.0f)));
        CHECK(nearly_zero(vec3(0.05f, 0.05f, 0.05f), 0.1f));
        CHECK_FALSE(nearly_zero(vec3(0.1f, 0.1f, 0.1f), 0.1f));
        CHECK(nearly_zero(vec2(0.0f)));
        CHECK_FALSE(nearly_zero(vec2(0.0f, 1.0f)));
    }

    TEST_CASE("glm_types - safe_normalize returns unit vectors and a fallback for zero input") {
        const vec3 normalized = safe_normalize(vec3(0.0f, 3.0f, 4.0f));
        CHECK_NEARLY(normalized, vec3(0.0f, 0.6f, 0.8f), math_test::TIGHT_EPSILON);
        CHECK(nearly_equal(glm::length(safe_normalize(vec3(-7.0f, 2.0f, 0.1f))), 1.0f, math_test::TIGHT_EPSILON));

        CHECK(safe_normalize(vec3(0.0f)) == vec3(0.0f, 0.0f, -1.0f));
        CHECK(safe_normalize(vec3(0.0f), vec3(0.0f, 1.0f, 0.0f)) == vec3(0.0f, 1.0f, 0.0f));
        CHECK(safe_normalize(vec3(1.0e-9f, 0.0f, 0.0f)) == vec3(0.0f, 0.0f, -1.0f));

        CHECK_NEARLY(safe_normalize(vec2(0.0f, -2.0f)), vec2(0.0f, -1.0f), math_test::TIGHT_EPSILON);
        CHECK(safe_normalize(vec2(0.0f)) == vec2(1.0f, 0.0f));
        CHECK(safe_normalize(vec2(0.0f), vec2(0.0f, 1.0f)) == vec2(0.0f, 1.0f));
    }

    TEST_CASE("glm_types - formatters print components in parentheses") {
        CHECK(std::format("{}", vec2(1.0f, -2.5f)) == "(1, -2.5)");
        CHECK(std::format("{}", vec3(1.0f, 2.0f, 3.0f)) == "(1, 2, 3)");
        CHECK(std::format("{}", vec4(0.5f, 0.0f, -1.0f, 1.0f)) == "(0.5, 0, -1, 1)");
        CHECK(std::format("{}", ivec2(-1, 2)) == "(-1, 2)");
        CHECK(std::format("{}", ivec3(1, -2, 3)) == "(1, -2, 3)");
        CHECK(std::format("{}", ivec4(1, 2, 3, -4)) == "(1, 2, 3, -4)");
        CHECK(std::format("{}", uvec2(1920u, 1080u)) == "(1920, 1080)");
        CHECK(std::format("{}", uvec3(1u, 2u, 3u)) == "(1, 2, 3)");
        CHECK(std::format("{}", uvec4(1u, 2u, 3u, 4u)) == "(1, 2, 3, 4)");
    }

    TEST_CASE("glm_types - quaternion formatter prints w first") {
        CHECK(std::format("{}", quat(1.0f, 0.0f, 0.0f, 0.0f)) == "(1, 0, 0, 0)");
        CHECK(std::format("{}", quat(0.5f, 0.25f, -0.125f, 2.0f)) == "(0.5, 0.25, -0.125, 2)");
    }

    TEST_CASE("glm_types - matrix formatters print columns in order") {
        mat4 matrix = mat4(1.0f);
        matrix[3] = vec4(1.0f, 2.0f, 3.0f, 1.0f);   // translation column
        CHECK(std::format("{}", matrix) == "[(1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (1, 2, 3, 1)]");

        mat3 small = mat3(0.0f);
        small[1] = vec3(4.0f, 5.0f, 6.0f);
        CHECK(std::format("{}", small) == "[(0, 0, 0), (4, 5, 6), (0, 0, 0)]");
    }

    TEST_CASE("glm_types - format specs are forwarded to every component") {
        CHECK(std::format("{:.2f}", vec3(1.0f, 2.5f, -0.75f)) == "(1.00, 2.50, -0.75)");
        CHECK(std::format("{:.1f}", vec2(0.26f, 0.74f)) == "(0.3, 0.7)");
        CHECK(std::format("{:+}", vec2(1.0f, -1.0f)) == "(+1, -1)");
        CHECK(std::format("{:04}", ivec2(1, 2)) == "(0001, 0002)");
        CHECK(std::format("{:.1f}", quat(1.0f, 0.0f, 0.0f, 0.0f)) == "(1.0, 0.0, 0.0, 0.0)");
        CHECK(std::format("{:.0f}", mat3(2.0f)) == "[(2, 0, 0), (0, 2, 0), (0, 0, 2)]");
        CHECK(std::format("{}|{:.1f}", vec2(1.0f, 2.0f), vec2(3.0f, 4.0f)) == "(1, 2)|(3.0, 4.0)");
    }
}
