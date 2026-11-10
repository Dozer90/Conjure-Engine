#include "conjure/testing/doctest.h"

#include "hlsl_shim.h"

#include "conjure/renderer/render_settings.h"

#include "conjure/core/platform_detect.h"
#include "conjure/core/types.h"

#include <cmath>

// The tonemap operators compiled natively (docs/design/02_renderer.md R11, 3.8, 10
// "test_tonemap_reference"): engine/shaders/shared/tonemap.hlsli is included through the HLSL shim
// so the very curves the GPU runs are pinned here: black maps to black, the operators are
// monotonic, bounded to [0, 1], saturate at high input and the dispatch picks the right one.

#ifndef CONJURE_TEST_TONEMAP_HLSLI
    #error "CONJURE_TEST_TONEMAP_HLSLI must name engine/shaders/shared/tonemap.hlsli"
#endif

// The shader is written with unsuffixed literals, which are doubles in C++: the narrowing to float
// is the point of the shim, so the conversion warnings are silenced for the include only.
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4244 4305)
#elif defined(__clang__)
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wconversion"
    #pragma clang diagnostic ignored "-Wdouble-promotion"
    #pragma clang diagnostic ignored "-Wimplicit-float-conversion"
    #pragma clang diagnostic ignored "-Wmissing-prototypes"
#elif defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wconversion"
    #pragma GCC diagnostic ignored "-Wdouble-promotion"
    #pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

namespace conjure::testing::hlsl {
#include CONJURE_TEST_TONEMAP_HLSLI
}

#if defined(_MSC_VER)
    #pragma warning(pop)
#elif defined(__clang__)
    #pragma clang diagnostic pop
#elif defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif

using namespace conjure;
using namespace conjure::testing;
using namespace conjure::testing::hlsl;

namespace {

    using tonemap_function = float3 (*)(float3);

    [[nodiscard]] float3 aces(float3 colour) {
        return tonemap_aces(colour);
    }

    [[nodiscard]] float3 reinhard(float3 colour) {
        return tonemap_reinhard_extended(colour, 4.0f);
    }

    [[nodiscard]] float3 agx(float3 colour) {
        return tonemap_agx(colour, 0.0f);
    }

    [[nodiscard]] float3 agx_golden(float3 colour) {
        return tonemap_agx(colour, 1.0f);
    }

    [[nodiscard]] float3 agx_punchy(float3 colour) {
        return tonemap_agx(colour, 2.0f);
    }

    [[nodiscard]] bool bounded(const float3 &value) {
        return value.x >= 0.0f && value.x <= 1.0f && value.y >= 0.0f && value.y <= 1.0f && value.z >= 0.0f
          && value.z <= 1.0f;
    }

    void check_operator(tonemap_function function, const char *name, float blackTolerance, bool greyPreserving = true) {
        CAPTURE(name);
        const float3 black = function(float3(0.0f, 0.0f, 0.0f));
        CHECK(black.x <= blackTolerance);
        CHECK(black.y <= blackTolerance);
        CHECK(black.z <= blackTolerance);
        // Monotonic in luminance and bounded at every step of a 20 stop ramp.
        float previous = -1.0f;
        for (float stop = -10.0f; stop <= 10.0f; stop += 0.25f) {
            const float input = std::exp2(stop);
            const float3 output = function(float3(input, input, input));
            CHECK(bounded(output));
            CHECK(output.x >= previous - 1.0e-5f);
            if (greyPreserving) {
                CHECK(output.x == doctest::Approx(output.y).epsilon(1.0e-3));
                CHECK(output.y == doctest::Approx(output.z).epsilon(1.0e-3));
            }
            previous = output.x;
        }
        // Very bright input saturates to (nearly) white.
        const float3 bright = function(float3(1.0e4f, 1.0e4f, 1.0e4f));
        CHECK(bright.x >= 0.97f);
        if (greyPreserving) {
            CHECK(bright.y >= 0.97f);
            CHECK(bright.z >= 0.97f);
        }
        // Mid grey lands in the middle of the display range.
        const float3 grey = function(float3(0.18f, 0.18f, 0.18f));
        CHECK(grey.x > 0.08f);
        CHECK(grey.x < 0.6f);
        // Hue is preserved in order: a red-dominant input stays red-dominant.
        const float3 red = function(float3(2.0f, 0.5f, 0.25f));
        CHECK(red.x > red.y);
        CHECK(red.y > red.z);
    }
}

TEST_SUITE("renderer/tonemap_reference") {

    TEST_CASE("tonemap - ACES fitted is black-preserving, monotonic, bounded and saturating") {
        check_operator(&aces, "aces", 1.0e-3f);
        // Known samples of the Hill fit for grey input.
        const float3 one = tonemap_aces(float3(1.0f, 1.0f, 1.0f));
        CHECK(one.x == doctest::Approx(0.619f).epsilon(0.02));
        const float3 grey = tonemap_aces(float3(0.18f, 0.18f, 0.18f));
        CHECK(grey.x == doctest::Approx(0.1056f).epsilon(0.02));
    }

    TEST_CASE("tonemap - extended Reinhard reaches exactly one at the white point") {
        check_operator(&reinhard, "reinhard", 0.0f);
        const float3 white = tonemap_reinhard_extended(float3(4.0f, 4.0f, 4.0f), 4.0f);
        CHECK(white.x == doctest::Approx(1.0f));
        const float3 half = tonemap_reinhard_extended(float3(1.0f, 1.0f, 1.0f), 4.0f);
        CHECK(half.x == doctest::Approx(0.53125f));   // 1 * (1 + 1/16) / 2
        const float3 beyond = tonemap_reinhard_extended(float3(8.0f, 8.0f, 8.0f), 4.0f);
        CHECK(beyond.x == doctest::Approx(1.0f));
        // A white point below one is clamped to one (plain Reinhard).
        const float3 clamped = tonemap_reinhard_extended(float3(1.0f, 1.0f, 1.0f), 0.1f);
        CHECK(clamped.x == doctest::Approx(1.0f));
        const float3 exact = tonemap_reinhard_extended(float3(0.0f, 0.0f, 0.0f), 4.0f);
        CHECK(exact.x == 0.0f);
    }

    TEST_CASE("tonemap - AgX and its looks are monotonic, bounded and nearly black at zero") {
        check_operator(&agx, "agx", 0.02f);
        check_operator(&agx_golden, "agx golden", 0.02f, false);   // the golden look tints greys on purpose
        check_operator(&agx_punchy, "agx punchy", 0.02f);
        // The golden look warms grey (more red than blue); the punchy look raises contrast.
        const float3 neutral = tonemap_agx(float3(0.5f, 0.5f, 0.5f), 0.0f);
        const float3 golden = tonemap_agx(float3(0.5f, 0.5f, 0.5f), 1.0f);
        CHECK(golden.x > golden.z);
        CHECK(neutral.x == doctest::Approx(neutral.z).epsilon(1.0e-3));
        // The punchy look raises saturation: a reddish input ends up redder relative to its green.
        const float3 reddishPunchy = tonemap_agx(float3(0.5f, 0.2f, 0.2f), 2.0f);
        const float3 reddishNeutral = tonemap_agx(float3(0.5f, 0.2f, 0.2f), 0.0f);
        CHECK(reddishPunchy.x - reddishPunchy.y > reddishNeutral.x - reddishNeutral.y);
    }

    TEST_CASE("tonemap - apply_tonemap dispatches on the operator index the view constants carry") {
        const float3 input(1.5f, 0.3f, 0.1f);
        const float3 viaAces = apply_tonemap(
          input,
          static_cast<float>(static_cast<u8>(renderer::tonemap_operator::ACES_FITTED)),
          4.0f,
          0.0f
        );
        const float3 viaReinhard = apply_tonemap(
          input,
          static_cast<float>(static_cast<u8>(renderer::tonemap_operator::REINHARD_EXTENDED)),
          4.0f,
          0.0f
        );
        const float3 viaAgx = apply_tonemap(
          input,
          static_cast<float>(static_cast<u8>(renderer::tonemap_operator::AGX)),
          4.0f,
          1.0f
        );
        const float3 viaNone = apply_tonemap(
          input,
          static_cast<float>(static_cast<u8>(renderer::tonemap_operator::NONE)),
          4.0f,
          0.0f
        );
        const float3 expectedAces = tonemap_aces(input);
        const float3 expectedReinhard = tonemap_reinhard_extended(input, 4.0f);
        const float3 expectedAgx = tonemap_agx(input, 1.0f);
        CHECK(viaAces.x == expectedAces.x);
        CHECK(viaAces.z == expectedAces.z);
        CHECK(viaReinhard.y == expectedReinhard.y);
        CHECK(viaAgx.x == expectedAgx.x);
        CHECK(viaNone.x == 1.0f);   // NONE saturates only
        CHECK(viaNone.y == doctest::Approx(0.3f));
        CHECK(viaNone.z == doctest::Approx(0.1f));
        // The operators differ from each other on the same input.
        CHECK(viaAces.x != viaReinhard.x);
        CHECK(viaReinhard.x != viaAgx.x);
        // Out-of-range indices fall through to the saturate path.
        const float3 fallthrough = apply_tonemap(input, 7.0f, 4.0f, 0.0f);
        CHECK(fallthrough.x == 1.0f);
    }
}
