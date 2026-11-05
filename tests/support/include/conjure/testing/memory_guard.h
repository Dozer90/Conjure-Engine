#pragma once

#include "conjure/core/memory/allocation_counters.h"
#include "conjure/core/types.h"

// Leak detection for fixtures (docs/design/08_testing.md 4.3, D6).
//
//   memory_guard guard = {};
//   { slot_map<int> map = {}; map.insert(1); }
//   guard.check();                   // or let the destructor do it
//
// The counters are process-wide and cumulative (allocation_counters.h); the guard compares the live
// allocation count (allocations minus frees) with its baseline. Test executables route global
// operator new/delete through the counters (tracked_global_new.cpp), so std containers count too.
//
// Because the counter is process-wide it also sees doctest's own bookkeeping, and doctest keeps
// some of it alive past the point where a guard checks:
//
//   - INFO / CAPTURE / CHECK_MESSAGE (and therefore CONJURE_REQUIRE_OK, CONJURE_CHECK_OK,
//     CONJURE_CHECK_ERROR, CONJURE_CHECK_APPROX, CONJURE_CHECK_NO_ALLOCATIONS, CONJURE_CHECK_ASSERTS)
//     grow a per-thread context stack whose storage is retained. test_main pre-grows that stack on
//     the test thread before any case runs, so these macros are safe inside a guard on the test
//     thread (self-tested); on job threads they are not.
//   - A SUBCASE boundary records the traversed path in per-case containers that live until the
//     case ends. A guard that encloses a SUBCASE therefore reports a leak (deterministically under
//     CTest, where every case is its own process).
//
// Rules: create the guard after the SUBCASE selection, or call reset_baseline() once the SUBCASEs
// have chosen the variant; never enclose a SUBCASE boundary or a worker-thread message macro.
// Fixtures that own a guard (08_testing D6) construct it inside the chosen variant for that reason.

namespace conjure::testing {

    /// <summary>
    /// Snapshots the allocation counters on construction and fails the case when the number of live
    /// allocations differs from the baseline at check() time.
    /// </summary>
    class memory_guard {
        private:
            core::memory::memory_snapshot mBaseline = {};
            bool mChecked = false;

        public:
            memory_guard();
            ~memory_guard();   // calls check() when it has not been called

            memory_guard(const memory_guard &) = delete;
            memory_guard &operator=(const memory_guard &) = delete;

        public:
            /// <summary>
            /// CHECKs that allocations minus frees since the baseline is zero, naming the delta.
            /// </summary>
            void check();

            /// <summary>
            /// Makes the current counters the new baseline (after warm-up allocations that are kept
            /// alive on purpose, or after the SUBCASE selection the guard had to precede) and arms
            /// the destructor check again.
            /// </summary>
            void reset_baseline() noexcept;

            /// <summary>
            /// Live allocations now minus live allocations at the baseline; positive means a leak.
            /// </summary>
            [[nodiscard]] i64 live_delta() const noexcept;

            /// <summary>
            /// The snapshot taken at construction or the last reset_baseline().
            /// </summary>
            [[nodiscard]] const core::memory::memory_snapshot &baseline() const noexcept {
                return mBaseline;
            }
    };
}
