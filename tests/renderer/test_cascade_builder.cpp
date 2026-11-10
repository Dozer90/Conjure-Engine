#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"

#include "renderer/shadows/cascade_builder.h"

#include "conjure/renderer/camera.h"
#include "conjure/renderer/limits.h"
#include "conjure/renderer/render_settings.h"

#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <array>
#include <cmath>

// Cascaded shadow maps (docs/design/02_renderer.md R9, 3.4, 10 "test_cascade_builder"): the
// practical split scheme, the bounding spheres rounded to a centimetre, texel snapping that keeps
// the projection stable under camera motion, and reverse-Z light projections that enclose the slice.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;

namespace {

    [[nodiscard]] math::mat4 camera_at(const math::vec3 &eye, const math::vec3 &target) {
        return glm::inverse(math::look_at(eye, target, math::vec3(0.0f, 1.0f, 0.0f)));
    }

    [[nodiscard]] shadow_settings test_settings() {
        shadow_settings settings = {};
        settings.distance = 100.0f;
        settings.splitLambda = 0.7f;
        settings.casterPadding = 50.0f;
        settings.cascadeCount = 4;
        return settings;
    }
}

TEST_SUITE("renderer/cascade_builder") {

    TEST_CASE("compute_cascade_splits - lambda 0 is uniform, lambda 1 logarithmic, between is a blend") {
        const std::array<f32, MAX_SHADOW_CASCADES + 1> uniform = compute_cascade_splits(1.0f, 101.0f, 4, 0.0f);
        CHECK(uniform[0] == doctest::Approx(1.0f));
        CHECK(uniform[1] == doctest::Approx(26.0f));
        CHECK(uniform[2] == doctest::Approx(51.0f));
        CHECK(uniform[3] == doctest::Approx(76.0f));
        CHECK(uniform[4] == doctest::Approx(101.0f));
        const std::array<f32, MAX_SHADOW_CASCADES + 1> logarithmic = compute_cascade_splits(1.0f, 10000.0f, 4, 1.0f);
        CHECK(logarithmic[1] == doctest::Approx(10.0f));
        CHECK(logarithmic[2] == doctest::Approx(100.0f));
        CHECK(logarithmic[3] == doctest::Approx(1000.0f));
        CHECK(logarithmic[4] == doctest::Approx(10000.0f));
        const std::array<f32, MAX_SHADOW_CASCADES + 1> blended = compute_cascade_splits(1.0f, 10000.0f, 4, 0.5f);
        CHECK(blended[1] == doctest::Approx(0.5f * 10.0f + 0.5f * 2500.75f));
        // Monotonic for every lambda and count; unused entries sit at the far distance.
        for (f32 lambda = 0.0f; lambda <= 1.0f; lambda += 0.1f) {
            for (u32 count = 1; count <= MAX_SHADOW_CASCADES; ++count) {
                const std::array<f32, MAX_SHADOW_CASCADES + 1> splits = compute_cascade_splits(
                  0.1f,
                  100.0f,
                  count,
                  lambda
                );
                CHECK(splits[0] == doctest::Approx(0.1f));
                for (u32 index = 1; index <= MAX_SHADOW_CASCADES; ++index) {
                    CHECK(splits[index] >= splits[index - 1] - 1.0e-6f);
                    if (index <= count) {
                        CHECK(splits[index] > splits[index - 1]);
                    }
                    CHECK(splits[index] <= 100.0f + 1.0e-3f);
                }
                CHECK(splits[count] == doctest::Approx(100.0f));
                CHECK(splits[MAX_SHADOW_CASCADES] == doctest::Approx(100.0f));
            }
        }
        // Degenerate inputs are clamped: a zero count is one cascade, a distance before the near plane moves out.
        const std::array<f32, MAX_SHADOW_CASCADES + 1> clamped = compute_cascade_splits(1.0f, 0.5f, 0, 2.0f);
        CHECK(clamped[0] == doctest::Approx(1.0f));
        CHECK(clamped[1] > clamped[0]);
        const std::array<f32, MAX_SHADOW_CASCADES + 1> tooMany = compute_cascade_splits(1.0f, 100.0f, 9, 0.5f);
        CHECK(tooMany[MAX_SHADOW_CASCADES] == doctest::Approx(100.0f));
    }

    TEST_CASE("slice_corners - the eight corners sit on the slice planes with the camera's field of view") {
        camera value = {};
        value.verticalFovRadians = math::HALF_PI;   // tan(45 deg) = 1
        const std::array<math::vec3, 8> corners = slice_corners(value, math::mat4(1.0f), 2.0f, 1.0f, 10.0f);
        CONJURE_CHECK_APPROX(corners[0], math::vec3(-2.0f, -1.0f, -1.0f));
        CONJURE_CHECK_APPROX(corners[2], math::vec3(2.0f, 1.0f, -1.0f));
        CONJURE_CHECK_APPROX(corners[4], math::vec3(-20.0f, -10.0f, -10.0f));
        CONJURE_CHECK_APPROX(corners[6], math::vec3(20.0f, 10.0f, -10.0f));
        // An orthographic camera keeps the same half height on both planes.
        camera ortho = {};
        ortho.projection = projection_type::ORTHOGRAPHIC;
        ortho.orthographicHeight = 4.0f;
        const std::array<math::vec3, 8> orthoCorners = slice_corners(ortho, math::mat4(1.0f), 1.0f, 1.0f, 10.0f);
        CONJURE_CHECK_APPROX(orthoCorners[0], math::vec3(-2.0f, -2.0f, -1.0f));
        CONJURE_CHECK_APPROX(orthoCorners[6], math::vec3(2.0f, 2.0f, -10.0f));
        // The camera transform moves the corners.
        const math::mat4 worldFromView = glm::translate(math::mat4(1.0f), math::vec3(0.0f, 5.0f, 0.0f));
        const std::array<math::vec3, 8> moved = slice_corners(value, worldFromView, 1.0f, 1.0f, 2.0f);
        CONJURE_CHECK_APPROX(moved[0], math::vec3(-1.0f, 4.0f, -1.0f));
    }

    TEST_CASE("build_cascades - every slice corner projects inside its cascade with reverse-Z depth in [0, 1]") {
        camera value = {};
        value.nearPlane = 0.1f;
        value.farPlane = 1000.0f;
        const math::mat4 worldFromView = camera_at(math::vec3(3.0f, 2.0f, 8.0f), math::vec3(0.0f, 0.0f, -10.0f));
        const math::vec3 sun = glm::normalize(math::vec3(0.3f, -1.0f, 0.2f));
        const cascade_set set = build_cascades(value, worldFromView, 16.0f / 9.0f, sun, test_settings(), 2048);
        CHECK(set.count == 4);
        f32 previousFar = value.nearPlane;
        for (u32 index = 0; index < set.count; ++index) {
            CAPTURE(index);
            const cascade &slice = set.cascades[index];
            CHECK(slice.nearDistance == doctest::Approx(previousFar));
            CHECK(slice.farDistance > slice.nearDistance);
            previousFar = slice.farDistance;
            CHECK(slice.radius > 0.0f);
            CHECK(slice.texelWorldSize == doctest::Approx(2.0f * slice.radius / 2048.0f));
            // The radius is a whole number of centimetres.
            CHECK(std::fmod(slice.radius + 1.0e-4f, 0.01f) < 2.0e-4f);
            const std::array<math::vec3, 8> corners = slice_corners(
              value,
              worldFromView,
              16.0f / 9.0f,
              slice.nearDistance,
              slice.farDistance
            );
            for (const math::vec3 &corner : corners) {
                const math::vec4 clip = slice.clipFromWorld * math::vec4(corner, 1.0f);
                CHECK(clip.w == doctest::Approx(1.0f));
                CHECK(clip.x >= -1.0f - 1.0e-3f);
                CHECK(clip.x <= 1.0f + 1.0e-3f);
                CHECK(clip.y >= -1.0f - 1.0e-3f);
                CHECK(clip.y <= 1.0f + 1.0e-3f);
                CHECK(clip.z >= 0.0f);
                CHECK(clip.z <= 1.0f);
                CHECK(glm::length(corner - slice.centre) <= slice.radius + slice.texelWorldSize * 2.0f);
            }
            // Casters toward the light (outside the slice) still land in front of the far plane.
            const math::vec3 towardLight = slice.centre - sun * (slice.radius + 40.0f);
            const math::vec4 casterClip = slice.clipFromWorld * math::vec4(towardLight, 1.0f);
            CHECK(casterClip.z > 0.0f);
            CHECK(casterClip.z <= 1.0f);
            // The light view looks along the light direction.
            const math::vec3 forward = -math::vec3(glm::inverse(slice.viewFromWorld)[2]);
            CONJURE_CHECK_APPROX(glm::normalize(forward), sun);
        }
        CHECK(set.cascades[set.count - 1].farDistance == doctest::Approx(100.0f));
        // Later cascades cover more ground.
        CHECK(set.cascades[3].radius > set.cascades[0].radius);
    }

    TEST_CASE("build_cascades - a small camera translation moves the snapped centre by whole texels only") {
        camera value = {};
        const math::vec3 sun = glm::normalize(math::vec3(0.2f, -1.0f, 0.1f));
        const shadow_settings settings = test_settings();
        const cascade_set first = build_cascades(
          value,
          camera_at(math::vec3(0.0f, 1.0f, 0.0f), math::vec3(0.0f, 1.0f, -10.0f)),
          1.0f,
          sun,
          settings,
          1024
        );
        const cascade_set second = build_cascades(
          value,
          camera_at(math::vec3(0.0123f, 1.0f, 0.0071f), math::vec3(0.0123f, 1.0f, -10.0f)),
          1.0f,
          sun,
          settings,
          1024
        );
        for (u32 index = 0; index < first.count; ++index) {
            CAPTURE(index);
            CHECK(first.cascades[index].radius == doctest::Approx(second.cascades[index].radius));
            // The snapped centre difference, expressed in light space, is a multiple of the texel size.
            const math::vec4 delta = first.cascades[index].viewFromWorld * math::vec4(
              second.cascades[index].centre - first.cascades[index].centre,
              0.0f
            );
            const f32 texel = first.cascades[index].texelWorldSize;
            const f32 stepsX = delta.x / texel;
            const f32 stepsY = delta.y / texel;
            CHECK(std::fabs(stepsX - std::round(stepsX)) < 1.0e-2f);
            CHECK(std::fabs(stepsY - std::round(stepsY)) < 1.0e-2f);
        }
        // A sub-texel move leaves the first cascade's projection unchanged.
        const cascade_set tiny = build_cascades(
          value,
          camera_at(math::vec3(0.00001f, 1.0f, 0.0f), math::vec3(0.00001f, 1.0f, -10.0f)),
          1.0f,
          sun,
          settings,
          1024
        );
        CHECK(approx_equal(tiny.cascades[0].clipFromWorld, first.cascades[0].clipFromWorld, 1.0e-4f));
    }

    TEST_CASE("build_cascades - the cascade count is clamped and a vertical light direction still has an up vector") {
        camera value = {};
        shadow_settings settings = test_settings();
        settings.cascadeCount = 0;
        const cascade_set one = build_cascades(
          value,
          math::mat4(1.0f),
          1.0f,
          math::vec3(0.0f, -1.0f, 0.0f),
          settings,
          512
        );
        CHECK(one.count == 1);
        CHECK(one.cascades[0].farDistance == doctest::Approx(100.0f));
        settings.cascadeCount = 99;
        const cascade_set four = build_cascades(
          value,
          math::mat4(1.0f),
          1.0f,
          math::vec3(0.0f, -1.0f, 0.0f),
          settings,
          512
        );
        CHECK(four.count == MAX_SHADOW_CASCADES);
        // Straight down: the projection is still well formed (no NaN).
        for (u32 index = 0; index < four.count; ++index) {
            const math::vec4 clip = four.cascades[index].clipFromWorld * math::vec4(four.cascades[index].centre, 1.0f);
            CHECK(std::isfinite(clip.x));
            CHECK(std::isfinite(clip.y));
            CHECK(std::isfinite(clip.z));
            CHECK(clip.x == doctest::Approx(0.0f).epsilon(1.0e-2));
        }
        // A zero light direction falls back to straight down.
        const cascade_set zero = build_cascades(value, math::mat4(1.0f), 1.0f, math::vec3(0.0f), settings, 512);
        CHECK(approx_equal(zero.cascades[0].clipFromWorld, four.cascades[0].clipFromWorld, 1.0e-4f));
        // Zero padding keeps the depth range at the sphere's diameter.
        settings.casterPadding = 0.0f;
        const cascade_set tight = build_cascades(
          value,
          math::mat4(1.0f),
          1.0f,
          math::vec3(0.0f, -1.0f, 0.0f),
          settings,
          512
        );
        const cascade &slice = tight.cascades[0];
        const math::vec4 top = slice.clipFromWorld * math::vec4(
          slice.centre + math::vec3(0.0f, slice.radius, 0.0f),
          1.0f
        );
        const math::vec4 bottom = slice.clipFromWorld * math::vec4(
          slice.centre - math::vec3(0.0f, slice.radius, 0.0f),
          1.0f
        );
        CHECK(top.z == doctest::Approx(1.0f).epsilon(1.0e-3));
        CHECK(bottom.z == doctest::Approx(0.0f).epsilon(1.0e-3));
    }
}
