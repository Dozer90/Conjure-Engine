#pragma once

#include "conjure/testing/doctest.h"

#include "conjure/core/memory/allocation_counters.h"
#include "conjure/core/types.h"

// Allocation gates (docs/design/08_testing.md 4.3, D7).
//
//   CONJURE_CHECK_NO_ALLOCATIONS(map.find(key));
//
//   allocation_scope scope = {};
//   system.parallel_for(count, body);
//   CHECK(scope.allocation_count() == 0);

namespace conjure::testing {

    /// <summary>
    /// Counts heap allocations made (on any thread) since construction or the last reset().
    /// </summary>
    class allocation_scope {
        private:
            core::memory::memory_snapshot mStart = {};

        public:
            allocation_scope();
            ~allocation_scope();

            allocation_scope(const allocation_scope &) = delete;
            allocation_scope &operator=(const allocation_scope &) = delete;

        public:
            /// <summary>
            /// Allocations since the scope started.
            /// </summary>
            [[nodiscard]] u64 allocation_count() const noexcept;

            /// <summary>
            /// Frees since the scope started.
            /// </summary>
            [[nodiscard]] u64 free_count() const noexcept;

            /// <summary>
            /// Bytes requested since the scope started.
            /// </summary>
            [[nodiscard]] u64 allocated_bytes() const noexcept;

            /// <summary>
            /// Restarts counting from the current counters.
            /// </summary>
            void reset() noexcept;
    };
}

// The count is read into a local before the CHECK because doctest's message machinery allocates.
#define CONJURE_CHECK_NO_ALLOCATIONS(statement)                                                             \
    do {                                                                                                    \
        ::conjure::testing::allocation_scope CONJURE_UNIQUE_NAME(conjureAllocationScope_) = {};             \
        statement;                                                                                          \
        const ::conjure::u64 CONJURE_UNIQUE_NAME(conjureAllocationCount_) =                                 \
          CONJURE_UNIQUE_NAME(conjureAllocationScope_).allocation_count();                                  \
        CHECK_MESSAGE(                                                                                      \
          CONJURE_UNIQUE_NAME(conjureAllocationCount_) == 0,                                                \
          "expected no allocations from: " #statement                                                       \
        );                                                                                                  \
    } while (false)
