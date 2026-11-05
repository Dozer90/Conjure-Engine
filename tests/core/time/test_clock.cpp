#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "conjure/core/clock.h"

#include <limits>
#include <memory>

using namespace conjure;

namespace {
    // Drives a clock through its base-class interface so the virtual dispatch is exercised.
    f64 read_through_base(const core::clock &source) {
        return source.now_seconds();
    }

    void sleep_through_base(core::clock &source, f64 deadline) {
        source.sleep_until(deadline);
    }
}

TEST_SUITE("core/clock") {

    TEST_CASE("clock - manual clock starts at zero or at the given time") {
        const core::manual_clock fromZero = {};
        CHECK(fromZero.now_seconds() == 0.0);

        const core::manual_clock fromTen(10.0);
        CHECK(fromTen.now_seconds() == 10.0);
    }

    TEST_CASE("clock - manual clock advances by the given amount and only when told") {
        core::manual_clock manual = {};

        manual.advance(1.5);
        CHECK(manual.now_seconds() == 1.5);
        CHECK(manual.now_seconds() == 1.5);

        manual.advance(0.0);
        CHECK(manual.now_seconds() == 1.5);

        manual.advance(0.25);
        CHECK(manual.now_seconds() == doctest::Approx(1.75));
    }

    TEST_CASE("clock - manual clock rejects a negative advance with an assertion and keeps its time") {
        testing::expect_assert asserts = {};
        core::manual_clock manual(4.0);

        manual.advance(-1.0);

        CHECK(asserts.count() == 1);
        CHECK(manual.now_seconds() == 4.0);
    }

    TEST_CASE("clock - manual clock rejects a NaN advance with an assertion and keeps its time") {
        // Regression: NaN failed the assertion but slipped past the negative-amount early-out and
        // poisoned the clock permanently.
        testing::expect_assert asserts = {};
        core::manual_clock manual(4.0);

        manual.advance(std::numeric_limits<f64>::quiet_NaN());

        CHECK(asserts.count() == 1);
        CHECK(manual.now_seconds() == 4.0);

        manual.advance(1.0);
        CHECK(manual.now_seconds() == 5.0);
    }

    TEST_CASE("clock - manual clock set moves to an absolute time in either direction") {
        core::manual_clock manual = {};

        manual.set(100.0);
        CHECK(manual.now_seconds() == 100.0);

        manual.set(5.0);
        CHECK(manual.now_seconds() == 5.0);

        manual.set(-2.0);
        CHECK(manual.now_seconds() == -2.0);
    }

    TEST_CASE("clock - manual clock sleep_until jumps to the deadline") {
        core::manual_clock manual = {};

        manual.sleep_until(3.0);
        CHECK(manual.now_seconds() == 3.0);

        SUBCASE("a deadline in the past does nothing") {
            manual.sleep_until(1.0);
            CHECK(manual.now_seconds() == 3.0);
        }

        SUBCASE("a deadline equal to now does nothing") {
            manual.sleep_until(3.0);
            CHECK(manual.now_seconds() == 3.0);
        }

        SUBCASE("a later deadline moves the clock again") {
            manual.sleep_until(3.5);
            CHECK(manual.now_seconds() == 3.5);
        }
    }

    TEST_CASE("clock - clocks are usable through the base class") {
        core::manual_clock manual(2.0);

        CHECK(read_through_base(manual) == 2.0);
        sleep_through_base(manual, 6.0);
        CHECK(read_through_base(manual) == 6.0);

        std::unique_ptr<core::clock> owned = std::make_unique<core::manual_clock>(1.0);
        CHECK(owned->now_seconds() == 1.0);
    }

    TEST_CASE("clock - stopwatch measures elapsed time on an injected clock") {
        core::manual_clock manual(10.0);
        core::stopwatch watch(manual);

        CHECK(&watch.source() == &manual);
        CHECK(watch.elapsed_seconds() == 0.0);

        manual.advance(2.0);
        CHECK(watch.elapsed_seconds() == 2.0);

        manual.advance(0.5);
        CHECK(watch.elapsed_seconds() == 2.5);
    }

    TEST_CASE("clock - stopwatch start resets the origin to now") {
        core::manual_clock manual = {};
        core::stopwatch watch(manual);

        manual.advance(3.0);
        watch.start();
        CHECK(watch.elapsed_seconds() == 0.0);

        manual.advance(1.0);
        CHECK(watch.elapsed_seconds() == 1.0);
    }

    TEST_CASE("clock - stopwatch restart returns the interval and the intervals add up without gaps") {
        core::manual_clock manual = {};
        core::stopwatch watch(manual);

        manual.advance(1.0);
        CHECK(watch.restart() == 1.0);
        CHECK(watch.elapsed_seconds() == 0.0);

        manual.advance(2.0);
        CHECK(watch.restart() == 2.0);

        CHECK(watch.restart() == 0.0);
    }

    TEST_CASE("clock - stopwatch follows a clock that is set backwards") {
        core::manual_clock manual(10.0);
        const core::stopwatch watch(manual);

        manual.set(8.0);
        CHECK(watch.elapsed_seconds() == -2.0);
    }

    TEST_CASE("clock - stopwatch copies keep measuring the same clock from the same origin") {
        core::manual_clock manual = {};
        core::stopwatch original(manual);
        manual.advance(1.0);

        core::stopwatch copy = original;
        CHECK(copy.elapsed_seconds() == 1.0);
        CHECK(&copy.source() == &manual);

        copy.start();
        CHECK(copy.elapsed_seconds() == 0.0);
        CHECK(original.elapsed_seconds() == 1.0);
    }

    TEST_CASE("clock - default stopwatch measures the process-wide default clock") {
        const core::stopwatch watch = {};

        CHECK(&watch.source() == &core::default_clock());
        CHECK(watch.elapsed_seconds() >= 0.0);
    }

    TEST_CASE("clock - default_clock is one shared steady instance") {
        core::clock &first = core::default_clock();
        core::clock &second = core::default_clock();

        CHECK(&first == &second);
        CHECK(dynamic_cast<core::steady_clock *>(&first) != nullptr);
    }

    TEST_CASE("clock - tick conversion uses the steady clock resolution") {
        CHECK(core::ticks_to_seconds(0) == 0.0);
        CHECK(core::ticks_per_second() > 0);
        CHECK(core::ticks_to_seconds(core::ticks_per_second()) == doctest::Approx(1.0));
        CHECK(core::ticks_to_seconds(core::ticks_per_second() / 2) == doctest::Approx(0.5));
    }

    TEST_CASE("clock - steady clock is monotonic and readings agree across instances") {
        core::steady_clock first = {};
        const core::steady_clock second = {};

        // The one wall-clock dependent check in this suite: time never runs backwards.
        f64 previous = first.now_seconds();
        CHECK(previous >= 0.0);
        for (int iteration = 0; iteration < 1000; ++iteration) {
            const f64 current = first.now_seconds();
            CHECK(current >= previous);
            previous = current;
        }

        u64 previousTicks = core::ticks_now();
        for (int iteration = 0; iteration < 1000; ++iteration) {
            const u64 currentTicks = core::ticks_now();
            CHECK(currentTicks >= previousTicks);
            previousTicks = currentTicks;
        }

        const f64 fromFirst = first.now_seconds();
        const f64 fromSecond = second.now_seconds();
        CHECK(fromSecond >= fromFirst);
        CHECK(fromSecond - fromFirst < 1.0);

        SUBCASE("sleep_until with a deadline in the past returns immediately") {
            const f64 before = first.now_seconds();
            first.sleep_until(before - 10.0);
            CHECK(first.now_seconds() >= before);
        }

        SUBCASE("sleep_until does not return before the deadline") {
            const f64 deadline = first.now_seconds() + 0.001;
            first.sleep_until(deadline);
            CHECK(first.now_seconds() >= deadline);
        }

        SUBCASE("sleep_until through the sleeping path does not return before the deadline") {
            const f64 deadline = first.now_seconds() + core::steady_clock::SPIN_WINDOW_SECONDS + 0.001;
            first.sleep_until(deadline);
            CHECK(first.now_seconds() >= deadline);
        }
    }
}
