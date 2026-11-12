#pragma once

#include "conjure/core/api.h"
#include "conjure/core/assert.h"
#include "conjure/core/types.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <random>
#include <type_traits>

// Pseudo-random numbers.
//
// The engine is std::mt19937_64 everywhere so seeded runs replay identically. The helpers below
// draw from any uniform random bit generator but deliberately avoid the standard distributions,
// whose algorithms differ between standard libraries: uniform and chance produce the same values on
// every platform for the same engine state. unit_vector3 consumes the same engine draws everywhere
// (two 64-bit draws), so the engine state stays in step, but its x and y go through std::cos /
// std::sin and can differ in the last bit between C runtimes (z is exact).
//
//   random::engine &generator = random::default_engine();       // this thread's engine
//   const i32 damage = random::uniform(generator, 10, 20);       // inclusive
//   const f32 jitter = random::uniform(generator, -1.0f, 1.0f);  // half-open [min, max)
//   if (random::chance(generator, 0.25)) { ... }

namespace conjure::core::random {

    /// <summary>
    /// The engine type used throughout Conjure. Its output sequence is fixed by the C++ standard,
    /// so seeded sequences are portable.
    /// </summary>
    using engine = std::mt19937_64;

    /// <summary>
    /// Seed used by every thread's default engine until seed() changes it.
    /// </summary>
    inline constexpr u64 DEFAULT_SEED = 0x5EED;

    /// <summary>
    /// This thread's engine. Created on first use, seeded with the process seed (see seed()).
    /// Every thread starts from the same seed, so two threads doing the same work produce the same
    /// numbers.
    /// </summary>
    [[nodiscard]] CONJURE_API engine &default_engine() noexcept;

    /// <summary>
    /// Sets the seed that every thread's default engine receives on first use and reseeds the
    /// calling thread's engine immediately, so the sequence restarts on this thread.
    /// Engines already created on other threads keep running until they call seed() themselves.
    /// Thread-safe.
    /// </summary>
    CONJURE_API void seed(u64 value) noexcept;

    /// <summary>
    /// The seed new default engines are created with.
    /// </summary>
    [[nodiscard]] CONJURE_API u64 current_seed() noexcept;

    namespace detail {
        /// <summary>
        /// Draws exactly 64 uniformly distributed bits from any generator, combining several
        /// outputs when the generator yields fewer bits per call.
        /// </summary>
        template <std::uniform_random_bit_generator Generator>
        [[nodiscard]] u64 next_u64(Generator &generator) {
            constexpr std::uintmax_t RANGE = Generator::max() - Generator::min();

            if constexpr (RANGE >= std::numeric_limits<u64>::max()) {
                return static_cast<u64>(generator() - Generator::min());
            } else if constexpr (RANGE >= std::numeric_limits<u32>::max()) {
                const u64 high = static_cast<u64>(generator() - Generator::min()) & 0xFFFFFFFFull;
                const u64 low = static_cast<u64>(generator() - Generator::min()) & 0xFFFFFFFFull;
                return (high << 32) | low;
            } else {
                // Rare generators with fewer than 32 bits per call: gather bytes.
                static_assert(RANGE >= 0xFF, "generator must yield at least 8 bits per call");
                u64 value = 0;
                for (int shift = 0; shift < 64; shift += 8) {
                    const u64 raw = static_cast<u64>(generator() - Generator::min());
                    value |= (raw & 0xFFull) << shift;
                }
                return value;
            }
        }

        /// <summary>
        /// Uniform integer in [0, span] (inclusive). The span of all 64 bits is a plain draw.
        /// </summary>
        template <std::uniform_random_bit_generator Generator>
        [[nodiscard]] u64 bounded_u64(Generator &generator, u64 span) {
            if (span == std::numeric_limits<u64>::max()) {
                return next_u64(generator);
            }

            // Reject the low remainder of the 64-bit range so every value in [0, bucketCount) is
            // equally likely. The rejection probability is below 50 percent for any span.
            const u64 bucketCount = span + 1;
            const u64 threshold = (u64{0} - bucketCount) % bucketCount;
            u64 draw = next_u64(generator);
            while (draw < threshold) {
                draw = next_u64(generator);
            }
            return draw % bucketCount;
        }

        /// <summary>
        /// Uniform double in [0, 1) with 53 random bits.
        /// </summary>
        template <std::uniform_random_bit_generator Generator>
        [[nodiscard]] f64 unit_f64(Generator &generator) {
            constexpr f64 SCALE = 1.0 / 9007199254740992.0;   // 2^-53
            return static_cast<f64>(next_u64(generator) >> 11) * SCALE;
        }

        /// <summary>
        /// Uniform float in [0, 1) with 24 random bits.
        /// </summary>
        template <std::uniform_random_bit_generator Generator>
        [[nodiscard]] f32 unit_f32(Generator &generator) {
            constexpr f32 SCALE = 1.0f / 16777216.0f;   // 2^-24
            return static_cast<f32>(next_u64(generator) >> 40) * SCALE;
        }
    }

    /// <summary>
    /// Uniform integer in the inclusive range [min, max]. Asserts when min > max and then returns
    /// min. Works for every integral type including 8-bit ones (bool excluded).
    /// </summary>
    template <std::uniform_random_bit_generator Generator, std::integral T>
        requires (!std::same_as<T, bool>)
    [[nodiscard]] T uniform(Generator &generator, T min, T max) {
        CONJURE_ASSERT(min <= max, "random::uniform requires min <= max");
        if (min >= max) {
            return min;
        }

        // Work in u64 so the span never overflows, then shift back into T's range.
        using unsigned_type = std::make_unsigned_t<T>;
        const u64 span = static_cast<u64>(static_cast<unsigned_type>(max) - static_cast<unsigned_type>(min));
        const unsigned_type offset = static_cast<unsigned_type>(detail::bounded_u64(generator, span));
        const unsigned_type shifted = static_cast<unsigned_type>(static_cast<unsigned_type>(min) + offset);
        return static_cast<T>(shifted);
    }

    /// <summary>
    /// Uniform floating point value in the half-open range [min, max). Asserts when min > max and
    /// then returns min. Never returns max.
    /// </summary>
    template <std::uniform_random_bit_generator Generator, std::floating_point T>
    [[nodiscard]] T uniform(Generator &generator, T min, T max) {
        CONJURE_ASSERT(min <= max, "random::uniform requires min <= max");
        if (min >= max) {
            return min;
        }

        T unit = T(0);
        if constexpr (std::is_same_v<T, f32>) {
            unit = detail::unit_f32(generator);
        } else {
            unit = static_cast<T>(detail::unit_f64(generator));
        }

        // Interpolate the way std::lerp does. When the ends straddle zero, max - min may overflow
        // to infinity, but the two scaled ends have opposite signs and their sum is exactly
        // bounded by [min, max]. Otherwise max - min cannot overflow and the min + unit * span
        // form is exact at min.
        T value = T(0);
        if (min <= T(0) && max >= T(0)) {
            value = min * (T(1) - unit) + max * unit;
        } else {
            value = min + unit * (max - min);
        }

        // Rounding can land exactly on max for wide or tiny ranges; step back inside.
        if (value >= max) {
            return std::nextafter(max, min);
        }
        return value;
    }

    /// <summary>
    /// True with the given probability (clamped to [0, 1]). 0 is never true, 1 is always true.
    /// </summary>
    template <std::uniform_random_bit_generator Generator>
    [[nodiscard]] bool chance(Generator &generator, f64 probability) {
        if (probability <= 0.0) {
            return false;
        }
        if (probability >= 1.0) {
            return true;
        }
        return detail::unit_f64(generator) < probability;
    }

    /// <summary>
    /// A uniformly distributed point on the unit sphere, built as Vector3{x, y, z}. The default
    /// is a plain array; pass math::vec3 (or any type constructible from three floats) to get it.
    /// Consumes two 64-bit draws (two calls of random::engine). x and y come from std::cos /
    /// std::sin and are not bit-identical across platforms; do not hash or compare them bit for bit
    /// across machines.
    /// </summary>
    template <typename Vector3 = std::array<f32, 3>, std::uniform_random_bit_generator Generator>
    [[nodiscard]] Vector3 unit_vector3(Generator &generator) {
        // Archimedes: z uniform in [-1, 1] and the angle uniform around the axis is uniform on
        // the sphere's surface.
        constexpr f64 TWO_PI = 6.283185307179586476925286766559;
        const f64 z = -1.0 + 2.0 * detail::unit_f64(generator);
        const f64 angle = TWO_PI * detail::unit_f64(generator);
        const f64 radius = std::sqrt(std::max(0.0, 1.0 - z * z));

        return Vector3{
          static_cast<f32>(radius * std::cos(angle)),
          static_cast<f32>(radius * std::sin(angle)),
          static_cast<f32>(z)
        };
    }

    /// <summary>
    /// An owned, independently seeded engine with the helpers above as members. Satisfies
    /// std::uniform_random_bit_generator so it can also feed the standard algorithms.
    /// </summary>
    class CONJURE_API random_engine {
        private:
            engine mEngine = engine(DEFAULT_SEED);
            u64 mSeed = DEFAULT_SEED;

        public:
            using result_type = engine::result_type;

            random_engine();
            explicit random_engine(u64 seedValue);
            ~random_engine();

            random_engine(const random_engine &) = default;
            random_engine &operator=(const random_engine &) = default;
            random_engine(random_engine &&) noexcept = default;
            random_engine &operator=(random_engine &&) noexcept = default;

        public:
            /// <summary>
            /// Restarts the sequence from the given seed.
            /// </summary>
            void seed(u64 seedValue) noexcept;

            /// <summary>
            /// The seed the current sequence started from.
            /// </summary>
            [[nodiscard]] u64 current_seed() const noexcept {
                return mSeed;
            }

            /// <summary>
            /// The next raw 64-bit output.
            /// </summary>
            [[nodiscard]] result_type operator()() noexcept {
                return mEngine();
            }

            /// <summary>
            /// Smallest value operator() can return, as required by std::uniform_random_bit_generator.
            /// </summary>
            [[nodiscard]] static constexpr result_type min() noexcept {
                return engine::min();
            }

            /// <summary>
            /// Largest value operator() can return, as required by std::uniform_random_bit_generator.
            /// </summary>
            [[nodiscard]] static constexpr result_type max() noexcept {
                return engine::max();
            }

            /// <summary>
            /// Advances the sequence by the given number of draws without producing values.
            /// </summary>
            void discard(u64 count) noexcept {
                mEngine.discard(count);
            }

            /// <summary>
            /// The wrapped engine, for callers that need the standard interface directly.
            /// </summary>
            [[nodiscard]] engine &raw() noexcept {
                return mEngine;
            }

            /// <summary>
            /// Uniform integer in [min, max] or floating point value in [min, max).
            /// </summary>
            template <typename T>
            [[nodiscard]] T uniform(T minimum, T maximum) {
                return random::uniform(mEngine, minimum, maximum);
            }

            /// <summary>
            /// True with the given probability.
            /// </summary>
            [[nodiscard]] bool chance(f64 probability) {
                return random::chance(mEngine, probability);
            }

            /// <summary>
            /// A uniformly distributed point on the unit sphere. Two engine draws; x and y may differ
            /// in the last bit between platforms (see the free random::unit_vector3).
            /// </summary>
            template <typename Vector3 = std::array<f32, 3>>
            [[nodiscard]] Vector3 unit_vector3() {
                return random::unit_vector3<Vector3>(mEngine);
            }

            /// <summary>
            /// True when both engines are in the same state and will produce the same sequence.
            /// </summary>
            [[nodiscard]] bool operator==(const random_engine &other) const noexcept {
                return mEngine == other.mEngine;
            }
    };
}
