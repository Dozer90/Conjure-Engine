#pragma once

#include "conjure/testing/doctest.h"

#include "conjure/core/assert.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/types.h"

#include <array>
#include <mutex>
#include <span>
#include <string>
#include <string_view>

// Expected assertions (docs/design/08_testing.md 4.2).
//
//   expect_assert guard = {};
//   map.erase(staleKey);               // asserts and early-outs
//   CHECK(guard.count() == 1);
//
// or, for one statement: CONJURE_CHECK_ASSERTS(map.erase(staleKey));
//
// test_main installs detail::testing_assert_handler for the whole run. Inside an expect_assert scope
// the handler records the failure and returns CONTINUE, so the code under test must early-out after
// its assert. Outside a scope the assert fails the current case, prints the captured log and aborts
// the process (08_testing 3.1), in a direct run as much as under CTest: the code under test would
// otherwise proceed past a violated precondition. A developer who wants to see every failure of a
// direct run can opt in to continuing with --conjure-continue-on-assert (or
// CONJURE_TEST_CONTINUE_ON_ASSERT=1); the opt-in never applies to threads other than the test
// thread, where no case could continue anyway.
// Recording never allocates, so allocation-counting tests can expect asserts too.

namespace conjure::testing {

    /// <summary>
    /// One recorded assertion failure. The message is a truncated copy in fixed storage.
    /// </summary>
    struct asserted_failure {
        static constexpr usize MAX_MESSAGE_LENGTH = 255;

        const char *expression = "";
        const char *file = "";
        u32 line = 0;
        std::array<char, MAX_MESSAGE_LENGTH + 1> message = {};   // NUL-terminated

        /// <summary>
        /// The recorded message as a view.
        /// </summary>
        [[nodiscard]] std::string_view message_view() const noexcept {
            return std::string_view(message.data());
        }
    };

    /// <summary>
    /// Turns a fatal CONJURE_ASSERT into a recorded failure for its lifetime; never unwinds. Scopes
    /// nest: the innermost live scope of the calling thread records. Asserts raised on threads that
    /// have no scope of their own are recorded by the most recently created live scope of the whole
    /// process, so helper threads that deliberately violate a precondition can be tested. Scopes may
    /// be created and destroyed on any thread in any order; the process-wide chain is kept
    /// consistent under a lock.
    /// </summary>
    class expect_assert {
        public:
            /// <summary>
            /// How many failures are kept; count() keeps counting beyond it.
            /// </summary>
            static constexpr u32 CAPACITY = 8;

        private:
            mutable std::mutex mMutex = {};
            std::array<asserted_failure, CAPACITY> mFailures = {};
            u32 mCount = 0;
            core::assert_handler mPreviousHandler = nullptr;
            expect_assert *mPreviousThreadScope = nullptr;
            expect_assert *mPreviousGlobalScope = nullptr;   // next older live scope in the process-wide chain

        public:
            expect_assert();
            ~expect_assert();   // restores the previous handler and the enclosing scope

            expect_assert(const expect_assert &) = delete;
            expect_assert &operator=(const expect_assert &) = delete;

        public:
            /// <summary>
            /// Number of assertion failures recorded since construction or the last reset().
            /// </summary>
            [[nodiscard]] u32 count() const noexcept;

            /// <summary>
            /// The recorded failures, oldest first; at most CAPACITY. When more than CAPACITY were
            /// recorded the oldest are overwritten and the span is in storage order.
            /// </summary>
            [[nodiscard]] std::span<const asserted_failure> failures() const noexcept;

            /// <summary>
            /// The message of the most recent failure (the expression when it had no message), or
            /// an empty string when none was recorded.
            /// </summary>
            [[nodiscard]] std::string last_message() const;

            /// <summary>
            /// Forgets every recorded failure.
            /// </summary>
            void reset() noexcept;

            /// <summary>
            /// Records a failure. Called by the handler; thread-safe.
            /// </summary>
            void record(const core::assert_info &info) noexcept;

        private:
            void unlink_global_scope() noexcept;
    };

    namespace detail {
        /// <summary>
        /// The handler test_main installs: records inside an active expect_assert scope, otherwise
        /// fails the current case, prints the captured log and aborts (continues instead on the
        /// test thread when environment().continueOnAssert is set).
        /// </summary>
        core::assert_action testing_assert_handler(const core::assert_info &info);

        /// <summary>
        /// The scope that would record an assert raised on the calling thread right now, or nullptr.
        /// </summary>
        [[nodiscard]] expect_assert *active_assert_scope() noexcept;
    }
}

// Checks that the statement asserts exactly once. Asserts are compiled out of Distribution, where the
// statement would run with its precondition violated, so there the macro only compiles the statement
// (inside a lambda that is never called) and checks nothing.
#if CONJURE_ASSERTS_ENABLED
    #define CONJURE_CHECK_ASSERTS(statement)                                                                \
        do {                                                                                                \
            ::conjure::testing::expect_assert CONJURE_UNIQUE_NAME(conjureAssertGuard_) = {};                \
            statement;                                                                                      \
            CHECK_MESSAGE(                                                                                  \
              CONJURE_UNIQUE_NAME(conjureAssertGuard_).count() == 1,                                        \
              "expected exactly one assertion from: " #statement                                            \
            );                                                                                              \
        } while (false)
#else
    #define CONJURE_CHECK_ASSERTS(statement)                                                                \
        do {                                                                                                \
            [[maybe_unused]] const auto CONJURE_UNIQUE_NAME(conjureUnchecked_) = [&]() {                    \
                statement;                                                                                  \
            };                                                                                              \
        } while (false)
#endif
