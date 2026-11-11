#pragma once

#include "conjure/core/api.h"
#include "conjure/core/types.h"

// Time sources.
//
// Everything in the engine that needs to know "what time is it" asks a clock so the main loop,
// animation and tests can be driven by a manual clock with fully deterministic results, while the
// shipped game runs on the steady clock. Raw ticks (ticks_now / ticks_to_seconds) exist for
// profiling where the cost of a virtual call and a double conversion matters.

namespace conjure::core {

    /// <summary>
    /// A monotonic time source measured in seconds. The epoch is implementation defined: only
    /// differences between two readings of the same clock are meaningful.
    /// </summary>
    class CONJURE_API clock {
        public:
            clock();
            virtual ~clock();

            clock(const clock &) = delete;
            clock &operator=(const clock &) = delete;

        public:
            /// <summary>
            /// Current time in seconds. Never decreases between two calls on the same clock.
            /// </summary>
            [[nodiscard]] virtual f64 now_seconds() const noexcept = 0;

            /// <summary>
            /// Blocks (or, for simulated clocks, advances time) until now_seconds() reaches the
            /// given deadline. Returns immediately when the deadline has already passed.
            /// </summary>
            virtual void sleep_until(f64 seconds) = 0;
    };

    /// <summary>
    /// Real time from std::chrono::steady_clock. Seconds count from the first use of any
    /// steady_clock in the process, so readings start near zero and keep full double precision.
    /// Every instance reads the same underlying time.
    /// </summary>
    class CONJURE_API steady_clock final : public clock {
        public:
            /// <summary>
            /// How close to the deadline the thread stops sleeping and starts spinning.
            /// </summary>
            static constexpr f64 SPIN_WINDOW_SECONDS = 0.002;

        public:
            steady_clock();
            ~steady_clock() override;

        public:
            /// <summary>
            /// Seconds since the process-wide steady epoch.
            /// </summary>
            [[nodiscard]] f64 now_seconds() const noexcept override;

            /// <summary>
            /// Sleeps the calling thread until 2 ms before the deadline, then spins on the clock so
            /// the wake-up lands within the scheduler jitter of the deadline. Deadlines may be
            /// arbitrarily far away (even infinite): the sleep is taken in bounded steps.
            /// </summary>
            void sleep_until(f64 seconds) override;
    };

    /// <summary>
    /// A clock that only moves when told to. Starts at zero. Used by tests and by tools that
    /// step the engine deterministically.
    /// </summary>
    class CONJURE_API manual_clock final : public clock {
        private:
            f64 mNowSeconds = 0.0;

        public:
            manual_clock();
            explicit manual_clock(f64 startSeconds);
            ~manual_clock() override;

        public:
            /// <summary>
            /// The current simulated time.
            /// </summary>
            [[nodiscard]] f64 now_seconds() const noexcept override;

            /// <summary>
            /// Jumps the clock forward to the deadline. A deadline in the past leaves the clock
            /// where it is: a manual clock never runs backwards through sleep_until.
            /// </summary>
            void sleep_until(f64 seconds) override;

            /// <summary>
            /// Moves the clock forward by the given non-negative amount. Negative and NaN amounts
            /// assert and are ignored.
            /// </summary>
            void advance(f64 seconds) noexcept;

            /// <summary>
            /// Sets the clock to an absolute time; may move it backwards.
            /// </summary>
            void set(f64 seconds) noexcept;
    };

    /// <summary>
    /// The process-wide steady clock. Use it when no clock was injected.
    /// </summary>
    [[nodiscard]] CONJURE_API clock &default_clock() noexcept;

    /// <summary>
    /// Raw steady-clock ticks for profiling. Convert differences with ticks_to_seconds.
    /// </summary>
    [[nodiscard]] CONJURE_API u64 ticks_now() noexcept;

    /// <summary>
    /// Converts a tick count (or a difference of two) from ticks_now into seconds.
    /// </summary>
    [[nodiscard]] CONJURE_API f64 ticks_to_seconds(u64 ticks) noexcept;

    /// <summary>
    /// Number of ticks in one second of the steady clock.
    /// </summary>
    [[nodiscard]] CONJURE_API u64 ticks_per_second() noexcept;

    /// <summary>
    /// Measures elapsed time on a clock. Runs from construction; start() and restart() reset the
    /// origin. The clock is referenced, not owned, and must outlive the stopwatch.
    /// </summary>
    class CONJURE_API stopwatch {
        private:
            const clock *mClock = nullptr;
            f64 mStartSeconds = 0.0;

        public:
            /// <summary>
            /// Starts measuring on the process-wide steady clock.
            /// </summary>
            stopwatch() noexcept;

            /// <summary>
            /// Starts measuring on the given clock.
            /// </summary>
            explicit stopwatch(const clock &source) noexcept;

            ~stopwatch();

            stopwatch(const stopwatch &) noexcept = default;
            stopwatch &operator=(const stopwatch &) noexcept = default;

        public:
            /// <summary>
            /// Resets the origin to the current time.
            /// </summary>
            void start() noexcept;

            /// <summary>
            /// Seconds elapsed since the last start, restart or construction.
            /// </summary>
            [[nodiscard]] f64 elapsed_seconds() const noexcept;

            /// <summary>
            /// Returns the elapsed seconds and resets the origin to the current time in one
            /// reading, so consecutive intervals add up without gaps.
            /// </summary>
            f64 restart() noexcept;

            /// <summary>
            /// The clock being measured.
            /// </summary>
            [[nodiscard]] const clock &source() const noexcept {
                return *mClock;
            }
    };
}
