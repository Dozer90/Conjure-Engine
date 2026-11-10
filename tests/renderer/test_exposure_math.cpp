#include "conjure/testing/doctest.h"

#include "renderer/exposure_math.h"

#include "conjure/core/types.h"

#include <cmath>

// Exposure formulas (docs/design/02_renderer.md R11, 3.8, 10 "test_exposure_math"): EV100 from
// luminance with the 12.5 calibration constant, the 1 / (1.2 * 2^EV) exposure and the asymmetric
// adaptation step the exposure_adapt shader implements.

using namespace conjure;
using namespace conjure::renderer;

TEST_SUITE("renderer/exposure_math") {

    TEST_CASE("exposure - ev100_from_luminance and luminance_from_ev100 invert each other") {
        // L = 12.5 / 100 cd/m^2 is EV 0 by definition.
        CHECK(exposure::ev100_from_luminance(0.125f) == doctest::Approx(0.0f).epsilon(1.0e-5));
        CHECK(exposure::luminance_from_ev100(0.0f) == doctest::Approx(0.125f));
        CHECK(exposure::ev100_from_luminance(0.25f) == doctest::Approx(1.0f));
        CHECK(exposure::ev100_from_luminance(1280.0f) == doctest::Approx(std::log2(1280.0f * 8.0f)));
        for (const f32 ev : {-10.0f, -3.5f, 0.0f, 4.25f, 12.0f, 16.0f}) {
            CAPTURE(ev);
            CHECK(
              exposure::ev100_from_luminance(exposure::luminance_from_ev100(ev)) == doctest::Approx(ev).epsilon(1.0e-4)
            );
        }
    }

    TEST_CASE("exposure - a non-positive luminance clamps instead of producing -inf") {
        const f32 zero = exposure::ev100_from_luminance(0.0f);
        CHECK(std::isfinite(zero));
        const f32 negative = exposure::ev100_from_luminance(-5.0f);
        CHECK(std::isfinite(negative));
        CHECK(negative == zero);
    }

    TEST_CASE("exposure - exposure_from_ev100 is 1 / (1.2 * 2^EV)") {
        CHECK(exposure::exposure_from_ev100(0.0f) == doctest::Approx(1.0f / 1.2f));
        CHECK(exposure::exposure_from_ev100(1.0f) == doctest::Approx(1.0f / 2.4f));
        CHECK(exposure::exposure_from_ev100(-1.0f) == doctest::Approx(1.0f / 0.6f));
        CHECK(exposure::exposure_from_ev100(12.0f) == doctest::Approx(1.0f / (1.2f * 4096.0f)));
        // Doubling the EV halves the exposure.
        for (f32 ev = -8.0f; ev < 16.0f; ev += 1.0f) {
            CHECK(
              exposure::exposure_from_ev100(ev + 1.0f) == doctest::Approx(exposure::exposure_from_ev100(ev) * 0.5f)
            );
        }
    }

    TEST_CASE("exposure - adapt_ev100 approaches the target with the up or down speed") {
        // Brightening uses the up speed: after one second at speed 3 the step is 1 - e^-3.
        const f32 up = exposure::adapt_ev100(0.0f, 10.0f, 1.0f, 3.0f, 1.0f);
        CHECK(up == doctest::Approx(10.0f * (1.0f - std::exp(-3.0f))));
        // Darkening uses the down speed: after one second at speed 1 the step is 1 - e^-1.
        const f32 down = exposure::adapt_ev100(10.0f, 0.0f, 1.0f, 3.0f, 1.0f);
        CHECK(down == doctest::Approx(10.0f * std::exp(-1.0f)));
        // No time passes: no change. A long time: the target.
        CHECK(exposure::adapt_ev100(2.0f, 7.0f, 0.0f, 3.0f, 1.0f) == doctest::Approx(2.0f));
        CHECK(exposure::adapt_ev100(2.0f, 7.0f, 1000.0f, 3.0f, 1.0f) == doctest::Approx(7.0f));
        CHECK(exposure::adapt_ev100(7.0f, 2.0f, 1000.0f, 3.0f, 1.0f) == doctest::Approx(2.0f));
        // Already there: stays.
        CHECK(exposure::adapt_ev100(4.0f, 4.0f, 0.5f, 3.0f, 1.0f) == doctest::Approx(4.0f));
        // The step never overshoots.
        f32 value = 0.0f;
        for (u32 step = 0; step < 100; ++step) {
            value = exposure::adapt_ev100(value, 5.0f, 0.1f, 3.0f, 1.0f);
            CHECK(value <= 5.0f + 1.0e-5f);
        }
        CHECK(value == doctest::Approx(5.0f).epsilon(1.0e-3));
    }
}
