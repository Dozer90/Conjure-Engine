#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "support_child_runner.h"

#include "conjure/core/assert.h"
#include "conjure/core/jobs/dedicated_thread.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/process.h"

#include <atomic>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::testing;

namespace {
    // CONJURE_ASSERT compiles to nothing in Distribution: every expectation below scales with this.
    constexpr u32 RECORDS_PER_VIOLATION = CONJURE_ASSERTS_ENABLED ? 1 : 0;

    assert_action dummy_handler(const assert_info &) {
        return assert_action::CONTINUE;
    }

    void violate(int value) {
        CONJURE_ASSERT(value > 0, "value {} must be positive", value);
    }
}

TEST_SUITE("testing/assert_capture") {

    TEST_CASE("expect_assert - records exactly one assert") {
        expect_assert guard = {};

        const int expectedLine = __LINE__ + 1;
        CONJURE_ASSERT(1 == 2, "one is not {}", 2);

        CHECK(guard.count() == RECORDS_PER_VIOLATION);
        REQUIRE(guard.failures().size() == RECORDS_PER_VIOLATION);
#if CONJURE_ASSERTS_ENABLED
        const asserted_failure &failure = guard.failures()[0];
        CHECK(std::string_view(failure.expression) == "1 == 2");
        CHECK(std::string_view(failure.file) == __FILE__);
        CHECK(failure.line == static_cast<u32>(expectedLine));
        CHECK(failure.message_view() == "one is not 2");
        CHECK(guard.last_message() == "one is not 2");
#else
        CONJURE_UNUSED(expectedLine);
        CHECK(guard.last_message().empty());
#endif
    }

    TEST_CASE("assert_capture - records the expression when there is no message") {
        expect_assert guard = {};

        CONJURE_ASSERT(false);

        CHECK(guard.last_message() == (CONJURE_ASSERTS_ENABLED ? "false" : ""));
    }

    TEST_CASE("assert_capture - nothing recorded without a failure") {
        expect_assert guard = {};

        CONJURE_ASSERT(true);
        CONJURE_VERIFY(true, "fine");

        CHECK(guard.count() == 0);
        CHECK(guard.failures().empty());
        CHECK(guard.last_message().empty());
    }

    TEST_CASE("assert_capture - restores the previous handler") {
        const assert_handler original = current_assert_handler();
        set_assert_handler(&dummy_handler);
        {
            expect_assert guard = {};
            CHECK(current_assert_handler() == &testing::detail::testing_assert_handler);
            CHECK(testing::detail::active_assert_scope() == &guard);
        }
        CHECK(current_assert_handler() == &dummy_handler);
        CHECK(testing::detail::active_assert_scope() == nullptr);
        set_assert_handler(original);
    }

    TEST_CASE("assert_capture - nested scopes record in the innermost and restore the outer") {
        expect_assert outer = {};
        {
            expect_assert inner = {};
            CHECK(testing::detail::active_assert_scope() == &inner);
            violate(-1);
            CHECK(inner.count() == RECORDS_PER_VIOLATION);
            CHECK(outer.count() == 0);
        }
        CHECK(testing::detail::active_assert_scope() == &outer);
        violate(-2);
        CHECK(outer.count() == RECORDS_PER_VIOLATION);
        CHECK(outer.last_message() == (CONJURE_ASSERTS_ENABLED ? "value -2 must be positive" : ""));
    }

    TEST_CASE("assert_capture - records asserts raised on helper threads") {
        expect_assert guard = {};
        {
            jobs::dedicated_thread helper("support-assert", [](std::stop_token) {
                violate(-3);
            });
        }

        CHECK(guard.count() == RECORDS_PER_VIOLATION);
        CHECK(guard.last_message() == (CONJURE_ASSERTS_ENABLED ? "value -3 must be positive" : ""));
    }

    TEST_CASE("assert_capture - a scope destroyed on one thread while a newer one lives elsewhere") {
        // Interleaved lifetimes across threads: the outer scope (test thread) dies while the helper's
        // scope is still the newest live one. The helper's scope must then keep recording and, when
        // it dies, must not restore the dead outer scope as the process-wide fallback.
        std::atomic<bool> helperScopeReady = false;
        std::atomic<bool> outerGone = false;
        std::atomic<u32> recordedOnHelper = 0;

        jobs::dedicated_thread helper("support-interleave", [&](std::stop_token) {
            expect_assert helperScope = {};
            helperScopeReady.store(true);
            while (!outerGone.load()) {
                std::this_thread::yield();
            }
            violate(-4);
            recordedOnHelper.store(helperScope.count());
        });

        {
            expect_assert outer = {};
            while (!helperScopeReady.load()) {
                std::this_thread::yield();
            }
            CHECK(testing::detail::active_assert_scope() == &outer);   // the thread-local wins here
        }
        outerGone.store(true);
        helper.join();

        CHECK(recordedOnHelper.load() == RECORDS_PER_VIOLATION);
        CHECK(testing::detail::active_assert_scope() == nullptr);
    }

    TEST_CASE("assert_capture - count continues past the capacity and keeps the newest message") {
        expect_assert guard = {};

        for (int index = 1; index <= static_cast<int>(expect_assert::CAPACITY) + 3; ++index) {
            violate(-index);
        }

        CHECK(guard.count() == (expect_assert::CAPACITY + 3) * RECORDS_PER_VIOLATION);
        CHECK(guard.failures().size() == expect_assert::CAPACITY * RECORDS_PER_VIOLATION);
        CHECK(guard.last_message() == (CONJURE_ASSERTS_ENABLED ? "value -11 must be positive" : ""));
    }

    TEST_CASE("assert_capture - truncates long messages") {
        expect_assert guard = {};
        const std::string longText(500, 'm');

        CONJURE_ASSERT(false, "{}", longText);

        CHECK(guard.last_message().size() == asserted_failure::MAX_MESSAGE_LENGTH * RECORDS_PER_VIOLATION);
#if CONJURE_ASSERTS_ENABLED
        CHECK(guard.failures()[0].message_view().size() == asserted_failure::MAX_MESSAGE_LENGTH);
#endif
    }

    TEST_CASE("assert_capture - reset forgets recorded failures") {
        expect_assert guard = {};
        violate(0);
        CHECK(guard.count() == RECORDS_PER_VIOLATION);

        guard.reset();

        CHECK(guard.count() == 0);
        CHECK(guard.last_message().empty());
    }

    TEST_CASE("CONJURE_CHECK_ASSERTS - passes for a statement that asserts once") {
        int calls = 0;
        const auto violateOnce = [&calls] {
            ++calls;
            CONJURE_ASSERT(false, "deliberate");
        };

        CONJURE_CHECK_ASSERTS(violateOnce());

        CHECK(calls == static_cast<int>(RECORDS_PER_VIOLATION));
    }

    TEST_CASE("CONJURE_CHECK_ASSERTS - compiles to nothing in distribution") {
        int calls = 0;
        const auto violateTwice = [&calls] {
            ++calls;
            CONJURE_ASSERT(calls < 0, "first");
            CONJURE_ASSERT(calls < 0, "second");
        };

#if CONJURE_ASSERTS_ENABLED
        // Two asserts must not satisfy the "exactly one" check: count them by hand instead.
        expect_assert guard = {};
        violateTwice();
        CHECK(guard.count() == 2);
        CHECK(calls == 1);
#else
        CONJURE_CHECK_ASSERTS(violateTwice());
        CHECK(calls == 0);
#endif
    }

    /*-----------------------------------------------------------------------*/
    // Through the child executable (the macro fails the case; only visible from outside)

    TEST_CASE("CONJURE_CHECK_ASSERTS - zero or two asserts fail the case") {
        const process_result none = run_support_child(
          {"-tc=support_child - CONJURE_CHECK_ASSERTS with no assert fails", "--conjure-isolated"}
        );
        const process_result two = run_support_child(
          {"-tc=support_child - CONJURE_CHECK_ASSERTS with two asserts fails", "--conjure-isolated"}
        );

#if CONJURE_ASSERTS_ENABLED
        CHECK(none.exitCode != 0);
        CHECK(none.exitCode != SKIP_EXIT_CODE);
        CHECK(output_contains(none.standardOutput, "expected exactly one assertion from: ++calls"));

        CHECK(two.exitCode != 0);
        CHECK(two.exitCode != SKIP_EXIT_CODE);
        CHECK(output_contains(two.standardOutput, "expected exactly one assertion from: violateTwice()"));
#else
        // The macro compiles to nothing, so neither case can fail.
        CHECK(none.exitCode == 0);
        CHECK(two.exitCode == 0);
#endif
    }
}
