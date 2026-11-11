#pragma once

#include "conjure/core/api.h"
#include "conjure/core/types.h"

#include <mutex>
#include <string>
#include <string_view>
#include <vector>

// Self-verifying samples (docs/design/08_testing.md 2.5, D8): scripts report named checks through the
// Conjure.Diagnostics.Check binding and the test harness asserts all_passed() after the run. Thread-safe:
// report() may be called from any thread.

namespace conjure::engine::diagnostics {

    struct check_record {
        std::string name = {};
        bool passed = false;
        std::string detail = {};
        u64 frame = 0;

        [[nodiscard]] bool operator==(const check_record &) const = default;
    };

    class CONJURE_API check_registry {
        private:
            mutable std::mutex mMutex = {};
            std::vector<check_record> mRecords = {};
            u64 mFailedCount = 0;

        public:
            check_registry();
            ~check_registry();

            check_registry(const check_registry &) = delete;
            check_registry &operator=(const check_registry &) = delete;

        public:
            /// <summary>
            /// Appends a record. Thread-safe.
            /// </summary>
            void report(std::string_view name, bool passed, std::string_view detail, u64 frame);

            /// <summary>
            /// A copy of every record in report order.
            /// </summary>
            [[nodiscard]] std::vector<check_record> records() const;

            /// <summary>
            /// True when no record failed (also for an empty registry).
            /// </summary>
            [[nodiscard]] bool all_passed() const noexcept;

            /// <summary>
            /// Records with passed == false.
            /// </summary>
            [[nodiscard]] u64 failed_count() const noexcept;

            /// <summary>
            /// Records reported so far.
            /// </summary>
            [[nodiscard]] usize record_count() const noexcept;

            /// <summary>
            /// Forgets every record.
            /// </summary>
            void clear();
    };
}
