#include "conjure/core/clock.h"

#include "conjure/core/assert.h"

#include <algorithm>
#include <chrono>
#include <thread>

// Definitions stay inside the namespace instead of using a using-directive: the C library
// declares ::clock(), which would make the unqualified name ambiguous at global scope.
namespace conjure::core {

    /*=======================================================================*/
    /*=======================================================================*/
    // Steady time source

    namespace {
        using source_clock = std::chrono::steady_clock;

        constexpr f64 SECONDS_PER_TICK =
          static_cast<f64>(source_clock::period::num) / static_cast<f64>(source_clock::period::den);

        // Longest single sleep_for call; comfortably inside every standard library's tick range.
        constexpr f64 MAX_SLEEP_STEP_SECONDS = 1.0;

        u64 raw_ticks() noexcept {
            // steady_clock never goes backwards, so the count is non-negative and fits a u64.
            return static_cast<u64>(source_clock::now().time_since_epoch().count());
        }

        u64 process_epoch_ticks() noexcept {
            // Captured once, on the first use of any steady_clock in the process, so readings
            // start near zero. Function-local static initialisation is thread-safe.
            static const u64 sEpoch = raw_ticks();
            return sEpoch;
        }
    }

    u64 ticks_now() noexcept {
        return raw_ticks();
    }

    f64 ticks_to_seconds(u64 ticks) noexcept {
        return static_cast<f64>(ticks) * SECONDS_PER_TICK;
    }

    u64 ticks_per_second() noexcept {
        return static_cast<u64>(source_clock::period::den) / static_cast<u64>(source_clock::period::num);
    }

    clock &default_clock() noexcept {
        static steady_clock sClock = {};
        return sClock;
    }


    /*=======================================================================*/
    /*=======================================================================*/
    // clock

    clock::clock() {}

    clock::~clock() {}


    /*=======================================================================*/
    /*=======================================================================*/
    // steady_clock

    steady_clock::steady_clock() :
        clock() {
        // Pin the epoch now so the first reading of this instance is already relative to it.
        process_epoch_ticks();
    }

    steady_clock::~steady_clock() {}

    f64 steady_clock::now_seconds() const noexcept {
        return ticks_to_seconds(raw_ticks() - process_epoch_ticks());
    }

    void steady_clock::sleep_until(f64 seconds) {
        // Sleep for the bulk of the wait and spin for the last SPIN_WINDOW_SECONDS: OS sleeps
        // routinely overshoot by a millisecond or more, which is too coarse for frame pacing.
        // Each sleep is capped so a huge or infinite deadline never overflows the integral tick
        // count the standard library converts the duration to; the loop re-reads the clock and
        // keeps sleeping until the deadline is near.
        f64 remaining = seconds - now_seconds();
        while (remaining > SPIN_WINDOW_SECONDS) {
            const f64 sleepSeconds = std::min(remaining - SPIN_WINDOW_SECONDS, MAX_SLEEP_STEP_SECONDS);
            std::this_thread::sleep_for(std::chrono::duration<f64>(sleepSeconds));
            remaining = seconds - now_seconds();
        }

        while (now_seconds() < seconds) {
            // Busy wait: the remaining time is below the scheduler's resolution.
        }
    }


    /*=======================================================================*/
    /*=======================================================================*/
    // manual_clock

    manual_clock::manual_clock() :
        clock() {}

    manual_clock::manual_clock(f64 startSeconds) :
        clock(),
        mNowSeconds(startSeconds) {}

    manual_clock::~manual_clock() {}

    f64 manual_clock::now_seconds() const noexcept {
        return mNowSeconds;
    }

    void manual_clock::sleep_until(f64 seconds) {
        if (seconds <= mNowSeconds) {
            return;
        }
        mNowSeconds = seconds;
    }

    void manual_clock::advance(f64 seconds) noexcept {
        // One negated predicate for the check and the early-out so NaN, which fails every
        // comparison, is rejected like a negative amount instead of poisoning the clock.
        if (!(seconds >= 0.0)) {
            CONJURE_ASSERT(false, "manual_clock::advance requires a non-negative amount, got {}", seconds);
            return;
        }
        mNowSeconds += seconds;
    }

    void manual_clock::set(f64 seconds) noexcept {
        mNowSeconds = seconds;
    }


    /*=======================================================================*/
    /*=======================================================================*/
    // stopwatch

    stopwatch::stopwatch() noexcept :
        mClock(&default_clock()),
        mStartSeconds(mClock->now_seconds()) {}

    stopwatch::stopwatch(const clock &source) noexcept :
        mClock(&source),
        mStartSeconds(source.now_seconds()) {}

    stopwatch::~stopwatch() {}

    void stopwatch::start() noexcept {
        mStartSeconds = mClock->now_seconds();
    }

    f64 stopwatch::elapsed_seconds() const noexcept {
        return mClock->now_seconds() - mStartSeconds;
    }

    f64 stopwatch::restart() noexcept {
        const f64 now = mClock->now_seconds();
        const f64 elapsed = now - mStartSeconds;
        mStartSeconds = now;
        return elapsed;
    }
}
