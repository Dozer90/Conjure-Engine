#include "conjure/testing/doctest.h"

#include "assets/durations.h"

#include <array>
#include <chrono>
#include <limits>

using namespace conjure;
using namespace conjure::assets;

TEST_SUITE("assets/durations") {

    TEST_CASE("to_steady_duration - converts seconds to the clock, NaN and negative as 0, at most 1e9 s") {
        using steady_duration = std::chrono::steady_clock::duration;
        CHECK(detail::MAX_DURATION_SECONDS == 1.0e9);

        // The clock's tick is finer than a millisecond everywhere, so these convert exactly.
        constexpr steady_duration LONGEST = std::chrono::seconds(1'000'000'000);
        struct duration_case {
            f64 given = 0.0;
            f64 seconds = 0.0;
            steady_duration onClock = {};
        };
        constexpr std::array<duration_case, 11> CASES = {{
            {0.002, 0.002, std::chrono::milliseconds(2)},
            {0.5, 0.5, std::chrono::milliseconds(500)},
            {30.0, 30.0, std::chrono::seconds(30)},
            {0.0, 0.0, steady_duration::zero()},
            {-0.0, 0.0, steady_duration::zero()},
            {-1.0, 0.0, steady_duration::zero()},
            {-std::numeric_limits<f64>::infinity(), 0.0, steady_duration::zero()},
            {std::numeric_limits<f64>::quiet_NaN(), 0.0, steady_duration::zero()},
            {1.0e9, 1.0e9, LONGEST},
            {1.0e15, 1.0e9, LONGEST},
            {std::numeric_limits<f64>::infinity(), 1.0e9, LONGEST},
        }};
        for (const duration_case &entry : CASES) {
            CAPTURE(entry.given);
            CHECK(detail::clamp_duration_seconds(entry.given) == entry.seconds);
            CHECK(detail::to_steady_duration(entry.given) == entry.onClock);
        }
    }
}
