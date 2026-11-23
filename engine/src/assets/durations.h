#pragma once

#include "conjure/core/types.h"

#include <chrono>
#include <cmath>

// The assets API takes its timing settings as f64 seconds with the unit in the name, like the rest of
// the engine. The watcher and the manager measure with steady_clock, so each setting is converted once, here:
// NaN and negative values become 0, and values too large for steady_clock::duration (anything above
// MAX_DURATION_SECONDS, infinity included) are held at that maximum.

namespace conjure::assets::detail {

    /// <summary>
    /// The longest duration a timing setting takes, about 31.7 years. now() + this stays far below the
    /// i64 nanosecond range of steady_clock::duration on every platform (the clock counts from boot).
    /// </summary>
    inline constexpr f64 MAX_DURATION_SECONDS = 1.0e9;

    /// <summary>
    /// The seconds a timing setting works with: NaN and negative values become 0, values above
    /// MAX_DURATION_SECONDS (infinity included) become that maximum.
    /// </summary>
    [[nodiscard]] inline f64 clamp_duration_seconds(f64 seconds) noexcept {
        if (std::isnan(seconds) || seconds <= 0.0) {
            return 0.0;
        }
        if (seconds > MAX_DURATION_SECONDS) {
            return MAX_DURATION_SECONDS;
        }
        return seconds;
    }

    /// <summary>
    /// The clamped seconds as a steady_clock duration, rounded to the clock's tick.
    /// </summary>
    [[nodiscard]] inline std::chrono::steady_clock::duration to_steady_duration(f64 seconds) noexcept {
        const std::chrono::duration<f64> clamped(clamp_duration_seconds(seconds));
        return std::chrono::round<std::chrono::steady_clock::duration>(clamped);
    }
}
