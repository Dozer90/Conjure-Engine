#include "conjure/testing/doctest.h"

#include "math_test.h"

#include "conjure/core/math/transform.h"

#include <array>
#include <bit>
#include <format>
#include <string>
#include <type_traits>

using namespace conjure;
using namespace conjure::math;

namespace {
    mat4 glm_trs(const transform &value) {
        return glm::translate(mat4(1.0f), value.position) * glm::mat4_cast(value.rotation)
          * glm::scale(mat4(1.0f), value.scale);
    }

    bool is_orthonormal(const quat &rotation) {
        const vec3 x = rotation * vec3(1.0f, 0.0f, 0.0f);
        const vec3 y = rotation * vec3(0.0f, 1.0f, 0.0f);
        const vec3 z = rotation * vec3(0.0f, 0.0f, 1.0f);
        return nearly_equal(glm::length(rotation), 1.0f, math_test::TIGHT_EPSILON)
          && nearly_zero(glm::dot(x, y), math_test::TIGHT_EPSILON)
          && nearly_zero(glm::dot(y, z), math_test::TIGHT_EPSILON)
          && nearly_zero(glm::dot(x, z), math_test::TIGHT_EPSILON)
          && nearly_equal(glm::cross(x, y), z, math_test::TIGHT_EPSILON);
    }
}

TEST_SUITE("core/transform") {

    TEST_CASE("transform - layout is ten floats and trivially copyable") {
        static_assert(sizeof(transform) == 40);
        static_assert(std::is_trivially_copyable_v<transform>);
        CHECK(alignof(transform) == 4);

        // Memory order is position xyz, rotation xyzw, scale xyz: what the C# mirror assumes.
        transform value = {};
        value.position = vec3(1.0f, 2.0f, 3.0f);
        value.rotation = quat(7.0f, 4.0f, 5.0f, 6.0f);
        value.scale = vec3(8.0f, 9.0f, 10.0f);
        const std::array<f32, 10> memory = std::bit_cast<std::array<f32, 10>>(value);
        CHECK(memory == std::array<f32, 10>{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f});
    }

    TEST_CASE("transform - default transform is the identity") {
        const transform identity = {};
        CHECK(identity.position == vec3(0.0f));
        CHECK(identity.rotation == quat(1.0f, 0.0f, 0.0f, 0.0f));
        CHECK(identity.scale == vec3(1.0f));
        CHECK(identity.to_matrix() == mat4(1.0f));
        CHECK(identity.transform_point(vec3(1.0f, 2.0f, 3.0f)) == vec3(1.0f, 2.0f, 3.0f));
        CHECK(identity.inverse() == identity);
        CHECK(identity == transform{});
    }

    TEST_CASE("transform - axes follow the right-handed Y-up -Z-forward convention") {
        const transform identity = {};
        CHECK(identity.forward() == vec3(0.0f, 0.0f, -1.0f));
        CHECK(identity.right() == vec3(1.0f, 0.0f, 0.0f));
        CHECK(identity.up() == vec3(0.0f, 1.0f, 0.0f));
        CHECK_NEARLY(glm::cross(identity.right(), identity.up()), -identity.forward(), math_test::TIGHT_EPSILON);

        // Yaw 90 degrees to the left (about +Y) turns forward from -Z to -X.
        const transform yawed = transform::from_rotation(math_test::rotation_about(vec3(0.0f, 1.0f, 0.0f), HALF_PI));
        CHECK_NEARLY(yawed.forward(), vec3(-1.0f, 0.0f, 0.0f), math_test::TIGHT_EPSILON);
        CHECK_NEARLY(yawed.right(), vec3(0.0f, 0.0f, -1.0f), math_test::TIGHT_EPSILON);
        CHECK_NEARLY(yawed.up(), vec3(0.0f, 1.0f, 0.0f), math_test::TIGHT_EPSILON);
    }

    TEST_CASE("transform - to_matrix equals translate * rotate * scale") {
        const transform sample = math_test::sample_transform();
        CHECK_NEARLY(sample.to_matrix(), glm_trs(sample), math_test::TIGHT_EPSILON);

        const transform uniform = math_test::uniform_transform();
        CHECK_NEARLY(uniform.to_matrix(), glm_trs(uniform), math_test::TIGHT_EPSILON);

        const transform mirrored = transform::from_scale(vec3(-1.0f, 2.0f, 3.0f));
        CHECK_NEARLY(mirrored.to_matrix(), glm_trs(mirrored), math_test::TIGHT_EPSILON);
    }

    TEST_CASE("transform - transform_point and transform_direction match the matrix") {
        const transform sample = math_test::sample_transform();
        const mat4 matrix = sample.to_matrix();
        const vec3 point = vec3(0.7f, -1.2f, 4.0f);
        const vec3 direction = vec3(0.0f, 0.0f, -1.0f);

        CHECK_NEARLY(sample.transform_point(point), vec3(matrix * vec4(point, 1.0f)), math_test::LOOSE_EPSILON);
        CHECK_NEARLY(sample.transform_direction(direction), sample.rotation * direction, math_test::TIGHT_EPSILON);
        CHECK_NEARLY(sample.transform_direction(direction), sample.forward(), math_test::TIGHT_EPSILON);
        CHECK(nearly_equal(glm::length(sample.transform_direction(direction)), 1.0f, math_test::TIGHT_EPSILON));

        // Directions ignore translation and scale.
        CHECK_NEARLY(
          transform::from_position(vec3(5.0f)).transform_direction(direction),
          direction,
          math_test::TIGHT_EPSILON
        );
        const transform scaled = transform::from_scale(vec3(7.0f));
        CHECK_NEARLY(scaled.transform_direction(direction), direction, math_test::TIGHT_EPSILON);
    }

    TEST_CASE("transform - inverse_transform_point undoes transform_point exactly even with non-uniform scale") {
        const transform sample = math_test::sample_transform();
        const vec3 point = vec3(-3.0f, 2.0f, 0.25f);
        CHECK_NEARLY(sample.inverse_transform_point(sample.transform_point(point)), point, math_test::LOOSE_EPSILON);
        CHECK_NEARLY(sample.transform_point(sample.inverse_transform_point(point)), point, math_test::LOOSE_EPSILON);

        const vec3 direction = glm::normalize(vec3(1.0f, 1.0f, 1.0f));
        CHECK_NEARLY(
          sample.inverse_transform_direction(sample.transform_direction(direction)),
          direction,
          math_test::TIGHT_EPSILON
        );

        const transform flat = transform::from_scale(vec3(1.0f, 0.0f, 1.0f));
        CHECK(flat.inverse_transform_point(vec3(2.0f, 5.0f, 3.0f)) == vec3(2.0f, 0.0f, 3.0f));
    }

    TEST_CASE("transform - from_matrix round trips a TRS matrix") {
        const transform sample = math_test::sample_transform();
        const transform decomposed = transform::from_matrix(sample.to_matrix());

        CHECK_NEARLY(decomposed.position, sample.position, math_test::LOOSE_EPSILON);
        CHECK_NEARLY(decomposed.scale, sample.scale, math_test::LOOSE_EPSILON);
        CHECK(same_rotation(decomposed.rotation, sample.rotation, math_test::LOOSE_EPSILON));
        CHECK_NEARLY(decomposed.to_matrix(), sample.to_matrix(), math_test::LOOSE_EPSILON);

        const transform identity = transform::from_matrix(mat4(1.0f));
        CHECK(identity == transform{});

        const transform translated = transform::from_matrix(glm::translate(mat4(1.0f), vec3(1.0f, 2.0f, 3.0f)));
        CHECK(translated == transform::from_position(vec3(1.0f, 2.0f, 3.0f)));
    }

    TEST_CASE("transform - from_matrix expresses mirroring as a negative x scale") {
        const transform mirrored = transform::from_matrix(glm::scale(mat4(1.0f), vec3(1.0f, -2.0f, 3.0f)));
        CHECK(mirrored.scale.x < 0.0f);
        CHECK(nearly_equal(glm::abs(mirrored.scale), vec3(1.0f, 2.0f, 3.0f), math_test::LOOSE_EPSILON));
        CHECK(is_orthonormal(mirrored.rotation));
        CHECK_NEARLY(mirrored.to_matrix(), glm::scale(mat4(1.0f), vec3(1.0f, -2.0f, 3.0f)), math_test::LOOSE_EPSILON);

        transform rotatedMirror = math_test::sample_transform();
        rotatedMirror.scale.x = -rotatedMirror.scale.x;
        const transform decomposed = transform::from_matrix(rotatedMirror.to_matrix());
        CHECK_NEARLY(decomposed.to_matrix(), rotatedMirror.to_matrix(), math_test::LOOSE_EPSILON);
        CHECK(decomposed.scale.x < 0.0f);
    }

    TEST_CASE("transform - from_matrix handles a collapsed axis without NaN") {
        const mat4 flat = glm::scale(mat4(1.0f), vec3(2.0f, 0.0f, 2.0f));
        const transform decomposed = transform::from_matrix(flat);
        CHECK(decomposed.scale == vec3(2.0f, 0.0f, 2.0f));
        CHECK(is_orthonormal(decomposed.rotation));
        CHECK(same_rotation(decomposed.rotation, quat(1.0f, 0.0f, 0.0f, 0.0f), math_test::TIGHT_EPSILON));
        CHECK_NEARLY(decomposed.to_matrix(), flat, math_test::TIGHT_EPSILON);

        const transform zero = transform::from_matrix(mat4(0.0f));
        CHECK(zero.scale == vec3(0.0f));
        CHECK(zero.position == vec3(0.0f));
        CHECK(is_orthonormal(zero.rotation));
    }

    TEST_CASE("transform - from_matrix keeps the mirror when an axis is collapsed") {
        // The determinant of a matrix with a zero column is exactly zero, so the mirror used to be
        // lost: scale(-2, 0, 3) decomposed to scale(2, 0, 3).
        const mat4 mirroredFlat = glm::scale(mat4(1.0f), vec3(-2.0f, 0.0f, 3.0f));
        const transform decomposed = transform::from_matrix(mirroredFlat);
        CHECK(decomposed.scale.x < 0.0f);
        CHECK(decomposed.scale == vec3(-2.0f, 0.0f, 3.0f));
        CHECK(is_orthonormal(decomposed.rotation));
        CHECK_NEARLY(decomposed.to_matrix(), mirroredFlat, math_test::TIGHT_EPSILON);

        // The mirror on another axis, combined with a collapsed one, also survives the round trip.
        const mat4 mirroredY = glm::scale(mat4(1.0f), vec3(2.0f, -1.0f, 0.0f));
        const transform decomposedY = transform::from_matrix(mirroredY);
        CHECK(decomposedY.scale.x < 0.0f);
        CHECK(decomposedY.scale.z == 0.0f);
        CHECK(is_orthonormal(decomposedY.rotation));
        CHECK_NEARLY(decomposedY.to_matrix(), mirroredY, math_test::TIGHT_EPSILON);
    }

    TEST_CASE("transform - composition with a uniform parent scale matches the matrix product") {
        const transform parent = math_test::uniform_transform();
        const transform child = math_test::sample_transform();
        const transform composed = parent * child;

        CHECK_NEARLY(composed.to_matrix(), parent.to_matrix() * child.to_matrix(), math_test::LOOSE_EPSILON);

        const vec3 point = vec3(1.0f, 1.0f, 1.0f);
        CHECK_NEARLY(
          composed.transform_point(point),
          parent.transform_point(child.transform_point(point)),
          math_test::LOOSE_EPSILON
        );
        CHECK_NEARLY(composed.scale, parent.scale * child.scale, math_test::TIGHT_EPSILON);
        CHECK(same_rotation(composed.rotation, parent.rotation * child.rotation, math_test::TIGHT_EPSILON));
    }

    TEST_CASE(
      "transform - composition with a non-uniform parent scale matches the matrix product for unrotated children"
    ) {
        const transform parent = math_test::sample_transform();
        transform child = {};
        child.position = vec3(2.0f, -1.0f, 0.5f);
        child.scale = vec3(0.5f, 2.0f, 4.0f);

        const transform composed = parent * child;
        CHECK_NEARLY(composed.to_matrix(), parent.to_matrix() * child.to_matrix(), math_test::LOOSE_EPSILON);
    }

    TEST_CASE(
      "transform - composition with a non-uniform parent and a rotated child is the documented TRS approximation"
    ) {
        // A TRS cannot hold the shear that a non-uniform parent scale puts on a rotated child, so the
        // composition keeps the component-wise rule (scale product, rotation product, transformed
        // position) and deliberately differs from the matrix product. Pinning this protects the
        // contract in both directions.
        const transform parent = transform::from_scale(vec3(1.0f, 2.0f, 1.0f));
        const transform child = transform::from_rotation(math_test::rotation_about(vec3(0.0f, 0.0f, 1.0f), HALF_PI));
        const transform composed = parent * child;

        CHECK_NEARLY(composed.scale, parent.scale * child.scale, math_test::TIGHT_EPSILON);
        CHECK(same_rotation(composed.rotation, parent.rotation * child.rotation, math_test::TIGHT_EPSILON));
        CHECK_NEARLY(composed.position, parent.transform_point(child.position), math_test::TIGHT_EPSILON);

        // The matrix product maps (1, 0, 0) to (0, 2, 0) (rotate then stretch y) while the TRS maps
        // it to (0, 1, 0) (stretch x by 1 then rotate).
        const mat4 product = parent.to_matrix() * child.to_matrix();
        CHECK_FALSE(nearly_equal(composed.to_matrix(), product, math_test::LOOSE_EPSILON));
        const vec3 unitX = vec3(1.0f, 0.0f, 0.0f);
        CHECK_NEARLY(vec3(product * vec4(unitX, 1.0f)), vec3(0.0f, 2.0f, 0.0f), math_test::TIGHT_EPSILON);
        CHECK_NEARLY(composed.transform_point(unitX), vec3(0.0f, 1.0f, 0.0f), math_test::TIGHT_EPSILON);
    }

    TEST_CASE("transform - the identity is neutral for composition") {
        const transform sample = math_test::sample_transform();
        const transform identity = {};
        CHECK(nearly_equal(identity * sample, sample, math_test::TIGHT_EPSILON));
        CHECK(nearly_equal(sample * identity, sample, math_test::TIGHT_EPSILON));
    }

    TEST_CASE("transform - composition is associative for rigid transforms") {
        transform a = math_test::uniform_transform();
        a.scale = vec3(1.0f);
        transform b = math_test::sample_transform();
        b.scale = vec3(1.0f);
        const transform c = transform::look_at(vec3(1.0f, 2.0f, 3.0f), vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));

        const transform leftFirst = (a * b) * c;
        const transform rightFirst = a * (b * c);
        CHECK_NEARLY(leftFirst.position, rightFirst.position, math_test::LOOSE_EPSILON);
        CHECK(same_rotation(leftFirst.rotation, rightFirst.rotation, math_test::LOOSE_EPSILON));
    }

    TEST_CASE("transform - inverse composed on the left yields the identity for every transform") {
        const transform sample = math_test::sample_transform();
        const transform roundTrip = sample.inverse() * sample;
        CHECK_NEARLY(roundTrip.position, vec3(0.0f), math_test::LOOSE_EPSILON);
        CHECK_NEARLY(roundTrip.scale, vec3(1.0f), math_test::LOOSE_EPSILON);
        CHECK(same_rotation(roundTrip.rotation, quat(1.0f, 0.0f, 0.0f, 0.0f), math_test::LOOSE_EPSILON));
    }

    TEST_CASE("transform - inverse composed on the right is not the identity for non-uniform scale") {
        // Documented asymmetry: t * inverse() needs the shear a TRS cannot hold, so only the left
        // composition inverse() * t is exact. The position is where the difference shows up.
        const transform sample = math_test::sample_transform();
        const transform roundTrip = sample * sample.inverse();
        CHECK_NEARLY(roundTrip.scale, vec3(1.0f), math_test::LOOSE_EPSILON);
        CHECK(same_rotation(roundTrip.rotation, quat(1.0f, 0.0f, 0.0f, 0.0f), math_test::LOOSE_EPSILON));
        CHECK_FALSE(nearly_equal(roundTrip.position, vec3(0.0f), math_test::LOOSE_EPSILON));
        CHECK_FALSE(nearly_equal(roundTrip, transform{}, math_test::LOOSE_EPSILON));
    }

    TEST_CASE("transform - inverse matches the matrix inverse for uniform scale") {
        const transform uniform = math_test::uniform_transform();
        const transform inverse = uniform.inverse();
        CHECK_NEARLY(inverse.to_matrix(), glm::inverse(uniform.to_matrix()), math_test::LOOSE_EPSILON);

        const transform roundTrip = uniform * inverse;
        CHECK_NEARLY(roundTrip.position, vec3(0.0f), math_test::LOOSE_EPSILON);
        CHECK_NEARLY(roundTrip.scale, vec3(1.0f), math_test::LOOSE_EPSILON);
        CHECK(same_rotation(roundTrip.rotation, quat(1.0f, 0.0f, 0.0f, 0.0f), math_test::LOOSE_EPSILON));

        const vec3 point = vec3(3.0f, -1.0f, 2.0f);
        CHECK_NEARLY(inverse.transform_point(uniform.transform_point(point)), point, math_test::LOOSE_EPSILON);
        CHECK(nearly_equal(inverse.inverse(), uniform, math_test::LOOSE_EPSILON));
    }

    TEST_CASE("transform - inverse of a zero scale component is zero rather than infinite") {
        const transform flat = transform::from_scale(vec3(2.0f, 0.0f, 4.0f));
        const transform inverse = flat.inverse();
        CHECK(inverse.scale == vec3(0.5f, 0.0f, 0.25f));
        CHECK(inverse.position == vec3(0.0f));
    }

    TEST_CASE("transform - look_at faces the target with the requested up") {
        const vec3 eye = vec3(1.0f, 2.0f, 3.0f);
        const vec3 target = vec3(-4.0f, 0.5f, -2.0f);
        const transform camera = transform::look_at(eye, target, vec3(0.0f, 1.0f, 0.0f));

        CHECK(camera.position == eye);
        CHECK(camera.scale == vec3(1.0f));
        CHECK(is_orthonormal(camera.rotation));
        CHECK_NEARLY(camera.forward(), glm::normalize(target - eye), math_test::TIGHT_EPSILON);
        CHECK(camera.up().y > 0.0f);
        CHECK(nearly_zero(glm::dot(camera.right(), vec3(0.0f, 1.0f, 0.0f)), math_test::TIGHT_EPSILON));

        // Looking straight down -Z from the origin is the identity rotation.
        const transform forward = transform::look_at(vec3(0.0f), vec3(0.0f, 0.0f, -10.0f), vec3(0.0f, 1.0f, 0.0f));
        CHECK(same_rotation(forward.rotation, quat(1.0f, 0.0f, 0.0f, 0.0f), math_test::TIGHT_EPSILON));

        // Looking along +X yaws by -90 degrees about Y.
        const transform sideways = transform::look_at(vec3(0.0f), vec3(5.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
        CHECK_NEARLY(sideways.forward(), vec3(1.0f, 0.0f, 0.0f), math_test::TIGHT_EPSILON);
        CHECK_NEARLY(sideways.up(), vec3(0.0f, 1.0f, 0.0f), math_test::TIGHT_EPSILON);
        CHECK_NEARLY(sideways.right(), vec3(0.0f, 0.0f, 1.0f), math_test::TIGHT_EPSILON);
    }

    TEST_CASE("transform - look_at matches glm::quatLookAt for regular inputs") {
        const vec3 eye = vec3(2.0f, 1.0f, -3.0f);
        const vec3 target = vec3(0.0f, 0.0f, 0.0f);
        const vec3 up = vec3(0.0f, 1.0f, 0.0f);
        const transform camera = transform::look_at(eye, target, up);
        const quat expected = glm::quatLookAt(glm::normalize(target - eye), up);
        CHECK(same_rotation(camera.rotation, expected, math_test::TIGHT_EPSILON));
    }

    TEST_CASE("transform - look_at handles degenerate inputs") {
        // Target equal to the eye: identity rotation, nothing to look at.
        const transform still = transform::look_at(vec3(1.0f), vec3(1.0f), vec3(0.0f, 1.0f, 0.0f));
        CHECK(still.rotation == quat(1.0f, 0.0f, 0.0f, 0.0f));
        CHECK(still.position == vec3(1.0f));

        // Up parallel to the view direction: still a valid rotation facing the target.
        const transform downward = transform::look_at(vec3(0.0f, 10.0f, 0.0f), vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
        CHECK(is_orthonormal(downward.rotation));
        CHECK_NEARLY(downward.forward(), vec3(0.0f, -1.0f, 0.0f), math_test::TIGHT_EPSILON);

        const transform upward = transform::look_at(vec3(0.0f), vec3(0.0f, 5.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f));
        CHECK(is_orthonormal(upward.rotation));
        CHECK_NEARLY(upward.forward(), vec3(0.0f, 1.0f, 0.0f), math_test::TIGHT_EPSILON);

        // Zero up hint: the replacement hint is the world axis least aligned with the direction, so
        // the resulting right vector is perpendicular to that axis. Here Y is least aligned.
        const transform noHint = transform::look_at(vec3(0.0f), vec3(1.0f, 0.0f, 1.0f), vec3(0.0f));
        CHECK(is_orthonormal(noHint.rotation));
        CHECK_NEARLY(noHint.forward(), glm::normalize(vec3(1.0f, 0.0f, 1.0f)), math_test::TIGHT_EPSILON);
        CHECK(nearly_zero(glm::dot(noHint.right(), vec3(0.0f, 1.0f, 0.0f)), math_test::TIGHT_EPSILON));

        // A direction that is mostly X, then Y, with Z the smallest component: Z replaces the hint
        // (the fallback used to pick Y whenever Y was not the dominant component).
        const vec3 skewed = glm::normalize(vec3(0.8f, 0.5f, 0.33f));
        const transform skewedHint = transform::look_at(vec3(0.0f), skewed, vec3(0.0f));
        CHECK(is_orthonormal(skewedHint.rotation));
        CHECK_NEARLY(skewedHint.forward(), skewed, math_test::TIGHT_EPSILON);
        CHECK(nearly_zero(glm::dot(skewedHint.right(), vec3(0.0f, 0.0f, 1.0f)), math_test::TIGHT_EPSILON));
        CHECK_FALSE(nearly_zero(glm::dot(skewedHint.right(), vec3(0.0f, 1.0f, 0.0f)), math_test::TIGHT_EPSILON));

        // Looking straight up or down: Z replaces the parallel hint (X and Z tie, Z wins).
        CHECK(nearly_zero(glm::dot(downward.right(), vec3(0.0f, 0.0f, 1.0f)), math_test::TIGHT_EPSILON));
    }

    TEST_CASE("transform - lerp interpolates position and scale linearly and rotation by slerp") {
        const transform a = transform::from_position(vec3(0.0f));
        transform b = {};
        b.position = vec3(10.0f, 0.0f, -4.0f);
        b.rotation = math_test::rotation_about(vec3(0.0f, 1.0f, 0.0f), HALF_PI);
        b.scale = vec3(3.0f);

        CHECK(nearly_equal(transform::lerp(a, b, 0.0f), a, math_test::TIGHT_EPSILON));
        CHECK(nearly_equal(transform::lerp(a, b, 1.0f), b, math_test::TIGHT_EPSILON));

        const transform middle = transform::lerp(a, b, 0.5f);
        CHECK_NEARLY(middle.position, vec3(5.0f, 0.0f, -2.0f), math_test::TIGHT_EPSILON);
        CHECK_NEARLY(middle.scale, vec3(2.0f), math_test::TIGHT_EPSILON);
        const quat halfTurn = math_test::rotation_about(vec3(0.0f, 1.0f, 0.0f), QUARTER_PI);
        CHECK(same_rotation(middle.rotation, halfTurn, math_test::TIGHT_EPSILON));
        CHECK(nearly_equal(glm::length(middle.rotation), 1.0f, math_test::TIGHT_EPSILON));
    }

    TEST_CASE("transform - lerp extrapolates outside [0, 1] and takes the shortest rotation path") {
        const transform a = {};
        transform b = transform::from_position(vec3(1.0f, 0.0f, 0.0f));
        b.rotation = math_test::rotation_about(vec3(0.0f, 1.0f, 0.0f), radians(170.0f));
        b.scale = vec3(2.0f);

        // Like math::lerp, t is not clamped: t = 2 continues past b and t = -1 rewinds before a.
        const transform ahead = transform::lerp(a, b, 2.0f);
        CHECK_NEARLY(ahead.position, vec3(2.0f, 0.0f, 0.0f), math_test::TIGHT_EPSILON);
        CHECK_NEARLY(ahead.scale, vec3(3.0f), math_test::TIGHT_EPSILON);
        const quat doubled = math_test::rotation_about(vec3(0.0f, 1.0f, 0.0f), radians(340.0f));
        CHECK(same_rotation(ahead.rotation, doubled, math_test::LOOSE_EPSILON));
        CHECK(nearly_equal(glm::length(ahead.rotation), 1.0f, math_test::TIGHT_EPSILON));

        const transform behind = transform::lerp(a, b, -1.0f);
        CHECK_NEARLY(behind.position, vec3(-1.0f, 0.0f, 0.0f), math_test::TIGHT_EPSILON);
        CHECK_NEARLY(behind.scale, vec3(0.0f), math_test::TIGHT_EPSILON);
        const quat reversed = math_test::rotation_about(vec3(0.0f, 1.0f, 0.0f), radians(-170.0f));
        CHECK(same_rotation(behind.rotation, reversed, math_test::LOOSE_EPSILON));

        // The negated quaternion is the same rotation; slerp must not go the long way round.
        transform bNegated = b;
        bNegated.rotation = -b.rotation;
        const transform halfway = transform::lerp(a, bNegated, 0.5f);
        const quat expected = math_test::rotation_about(vec3(0.0f, 1.0f, 0.0f), radians(85.0f));
        CHECK(same_rotation(halfway.rotation, expected, math_test::LOOSE_EPSILON));
    }

    TEST_CASE("transform - factories, equality and nearly_equal") {
        const transform positioned = transform::from_position(vec3(1.0f, 2.0f, 3.0f));
        CHECK(positioned.position == vec3(1.0f, 2.0f, 3.0f));
        CHECK(positioned.rotation == quat(1.0f, 0.0f, 0.0f, 0.0f));
        CHECK(positioned.scale == vec3(1.0f));

        const quat rotation = math_test::rotation_about(vec3(1.0f, 0.0f, 0.0f), 0.5f);
        CHECK(transform::from_rotation(rotation).rotation == rotation);
        CHECK(transform::from_scale(vec3(2.0f)).scale == vec3(2.0f));

        transform slightlyOff = positioned;
        slightlyOff.position.x += 1.0e-5f;
        CHECK(slightlyOff != positioned);
        CHECK(nearly_equal(slightlyOff, positioned, math_test::LOOSE_EPSILON));
        CHECK_FALSE(nearly_equal(slightlyOff, positioned, 1.0e-7f));

        transform differentScale = positioned;
        differentScale.scale.z = 2.0f;
        CHECK_FALSE(nearly_equal(differentScale, positioned, math_test::LOOSE_EPSILON));
    }

    TEST_CASE("transform - formatter prints position, rotation (w first) and scale") {
        transform value = {};
        value.position = vec3(1.0f, 2.0f, 3.0f);
        value.scale = vec3(0.5f);
        CHECK(std::format("{}", value) == "{position=(1, 2, 3), rotation=(1, 0, 0, 0), scale=(0.5, 0.5, 0.5)}");
        const std::string identityText = std::format("{:.1f}", transform{});
        CHECK(identityText == "{position=(0.0, 0.0, 0.0), rotation=(1.0, 0.0, 0.0, 0.0), scale=(1.0, 1.0, 1.0)}");
    }
}
