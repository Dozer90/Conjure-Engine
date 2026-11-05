#pragma once

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/types.h"

#include <algorithm>
#include <functional>
#include <span>

// Convenience wrappers around job_system::parallel_for that accept any callable instead of a
// std::function. The wrapper std::function is built once per call and holds only references, so
// it stays within the small-object buffer and the dispatch remains allocation-free.

namespace conjure::core::jobs {

    /// <summary>
    /// Grain used by the per-index parallel_for: max(1, count / (workerCount * 4)), treating zero
    /// workers as one.
    /// </summary>
    [[nodiscard]] constexpr usize default_grain(usize count, u32 workerCount) noexcept {
        const usize effectiveWorkers = workerCount == 0 ? 1 : static_cast<usize>(workerCount);
        return std::max<usize>(1, count / (effectiveWorkers * 4));
    }

    /// <summary>
    /// Number of ranges [0, count) splits into with the given grain (a grain of 0 counts as 1).
    /// </summary>
    [[nodiscard]] constexpr usize range_count(usize count, usize grain) noexcept {
        const usize effectiveGrain = std::max<usize>(1, grain);
        return count == 0 ? 0 : 1 + (count - 1) / effectiveGrain;
    }

    /// <summary>
    /// Runs body(begin, end) over [0, count) in ranges of at most 'grain' elements.
    /// </summary>
    template <typename Body>
    void parallel_for_range(job_system &system, usize count, usize grain, Body &&body) {
        const std::function<void(usize, usize)> ranges = [&body](usize begin, usize end) {
            body(begin, end);
        };
        system.parallel_for(count, grain, ranges);
    }

    /// <summary>
    /// Runs body(item) for every element of the span, 'grain' elements per job.
    /// </summary>
    template <typename ElementType, typename Body>
    void parallel_for_each(job_system &system, std::span<ElementType> items, usize grain, Body &&body) {
        const std::function<void(usize, usize)> ranges = [&items, &body](usize begin, usize end) {
            for (usize index = begin; index < end; ++index) {
                body(items[index]);
            }
        };
        system.parallel_for(items.size(), grain, ranges);
    }

    /// <summary>
    /// Runs body(item) for every element of the span with the default grain.
    /// </summary>
    template <typename ElementType, typename Body>
    void parallel_for_each(job_system &system, std::span<ElementType> items, Body &&body) {
        parallel_for_each(system, items, default_grain(items.size(), system.worker_count()), body);
    }
}
