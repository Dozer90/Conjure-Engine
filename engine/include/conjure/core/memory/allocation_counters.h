#pragma once

#include "conjure/core/types.h"

#include <atomic>

// Process-wide allocation counters.
//
// Every heap acquisition made through core::memory (tracked_allocate, and therefore every block an
// arena, pool or frame allocator obtains) bumps these counters. Test executables additionally route
// global operator new/delete through them, which gives exact allocation counts for leak gates.
// Counting is cumulative: nothing is ever decremented, so "live allocations" is
// allocationCount - freeCount and allocatedBytes is the total number of bytes ever requested.

namespace conjure::core::memory {

    /// <summary>
    /// The three cumulative counters. Relaxed atomics: readers only need eventual, monotonic values.
    /// </summary>
    struct allocation_counters {
        std::atomic<u64> allocationCount = 0;
        std::atomic<u64> freeCount = 0;
        std::atomic<u64> allocatedBytes = 0;
    };

    /// <summary>
    /// A plain copy of the counters taken at one point in time.
    /// </summary>
    struct memory_snapshot {
        u64 allocationCount = 0;
        u64 freeCount = 0;
        u64 allocatedBytes = 0;

        /// <summary>
        /// Allocations that have not been freed yet (allocationCount - freeCount).
        /// </summary>
        [[nodiscard]] constexpr u64 live_count() const noexcept {
            return allocationCount - freeCount;
        }

        [[nodiscard]] constexpr bool operator==(const memory_snapshot &) const noexcept = default;
    };

    /// <summary>
    /// The process-wide counters. Safe to call from any thread, including during static initialisation.
    /// </summary>
    [[nodiscard]] allocation_counters &counters() noexcept;

    /// <summary>
    /// Copies the current counter values. The three loads are not one atomic transaction; callers
    /// comparing snapshots quiesce other threads first (test fixtures) or accept small skews.
    /// </summary>
    [[nodiscard]] memory_snapshot snapshot() noexcept;

    /// <summary>
    /// Records one allocation of 'bytes' bytes. Called by tracked_allocate and by code that acquires
    /// memory through another route (global operator new in tests, physics allocation hooks).
    /// </summary>
    void record_allocation(usize bytes) noexcept;

    /// <summary>
    /// Records one free. Counterpart of record_allocation.
    /// </summary>
    void record_free() noexcept;
}
