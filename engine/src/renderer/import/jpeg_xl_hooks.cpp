#include "renderer/import/jpeg_xl_hooks.h"
#include "renderer/import/jpeg_xl_codec.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/memory/tracked_allocation.h"
#include "conjure/core/types.h"

#include <algorithm>
#include <cstring>
#include <functional>

using namespace conjure;
using namespace conjure::renderer;

namespace {
    /// <summary>
    /// libjxl was written against malloc, whose blocks suit any fundamental type: 16 bytes on every 64-bit
    /// platform the engine builds for. The size header tracked_free needs back is one alignment unit, so the
    /// block handed to libjxl keeps that alignment.
    /// </summary>
    constexpr usize ALLOCATION_ALIGNMENT = std::max<usize>(core::memory::DEFAULT_ALIGNMENT, 16);
    constexpr usize ALLOCATION_HEADER = ALLOCATION_ALIGNMENT;
    static_assert(core::memory::is_power_of_two(ALLOCATION_ALIGNMENT), "tracked_allocate takes a power of two");
    static_assert(ALLOCATION_HEADER >= sizeof(usize), "the header holds the block size");

    /// <summary>
    /// Counts 'bytes' against the budget unless that would take the live total above the limit. The
    /// compare-exchange makes two allocations racing near the limit see each other, so neither is refused for
    /// the other's sake alone.
    /// </summary>
    [[nodiscard]] bool reserve(detail::jpeg_xl_memory_budget &budget, usize bytes) noexcept {
        const u64 limit = budget.limitBytes.load(std::memory_order_relaxed);
        u64 live = budget.liveBytes.load(std::memory_order_relaxed);
        do {
            if (bytes > limit || live > limit - bytes) {
                budget.exceeded.store(true, std::memory_order_relaxed);
                return false;
            }
        } while (!budget.liveBytes.compare_exchange_weak(live, live + bytes, std::memory_order_relaxed));
        return true;
    }

    void *allocate(void *opaque, size_t size) {
        if (size > core::memory::MAX_ALLOCATION_BYTES - ALLOCATION_HEADER) {
            return nullptr;
        }
        const usize total = static_cast<usize>(size) + ALLOCATION_HEADER;
        detail::jpeg_xl_memory_budget *budget = static_cast<detail::jpeg_xl_memory_budget *>(opaque);
        if (budget != nullptr && !reserve(*budget, total)) {
            return nullptr;
        }
        void *block = core::memory::tracked_allocate(total, ALLOCATION_ALIGNMENT);
        if (block == nullptr) {
            if (budget != nullptr) {
                budget->liveBytes.fetch_sub(total, std::memory_order_relaxed);
            }
            return nullptr;
        }
        std::memcpy(block, &total, sizeof(total));
        return static_cast<std::byte *>(block) + ALLOCATION_HEADER;
    }

    void release(void *opaque, void *address) {
        if (address == nullptr) {
            return;
        }
        std::byte *block = static_cast<std::byte *>(address) - ALLOCATION_HEADER;
        usize total = 0;
        std::memcpy(&total, block, sizeof(total));
        core::memory::tracked_free(block, total);
        detail::jpeg_xl_memory_budget *budget = static_cast<detail::jpeg_xl_memory_budget *>(opaque);
        if (budget != nullptr) {
            budget->liveBytes.fetch_sub(total, std::memory_order_relaxed);
        }
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Module functions

JxlMemoryManager conjure::renderer::detail::jpeg_xl_memory_manager(jpeg_xl_memory_budget *budget) noexcept {
    JxlMemoryManager manager = {};
    manager.opaque = budget;
    manager.alloc = &allocate;
    manager.free = &release;
    return manager;
}

bool conjure::renderer::detail::can_run_jpeg_xl_in_parallel(const core::jobs::job_system *jobs) noexcept {
    return jobs != nullptr && jobs->is_running() && jobs->worker_count() > 0;
}

JxlParallelRetCode conjure::renderer::detail::run_jpeg_xl_on_jobs(
  void *runnerOpaque,
  void *jpegxlOpaque,
  JxlParallelRunInit init,
  JxlParallelRunFunction function,
  uint32_t startRange,
  uint32_t endRange
) {
    core::jobs::job_system &jobs = *static_cast<core::jobs::job_system *>(runnerOpaque);
    const u64 valueCount = endRange > startRange ? static_cast<u64>(endRange - startRange) : 0;
    // The workers and the caller, at most JPEG_XL_MAX_THREADS: the decoder's memory budget allows for that many.
    const u64 threadCount = std::min<u64>(static_cast<u64>(jobs.worker_count()) + 1, detail::JPEG_XL_MAX_THREADS);
    const u64 taskCount = std::clamp<u64>(valueCount, 1, threadCount);

    const JxlParallelRetCode initialised = init(jpegxlOpaque, static_cast<size_t>(taskCount));
    if (initialised != JXL_PARALLEL_RET_SUCCESS) {
        return initialised;
    }
    if (valueCount == 0) {
        return JXL_PARALLEL_RET_SUCCESS;
    }

    // Task k owns the values [k * count / tasks, (k + 1) * count / tasks): contiguous, fixed by the counts alone.
    const std::function<void(usize, usize)> tasks = [&](usize firstTask, usize endTask) {
        for (usize task = firstTask; task < endTask; ++task) {
            const u64 first = startRange + valueCount * task / taskCount;
            const u64 last = startRange + valueCount * (task + 1) / taskCount;
            for (u64 value = first; value < last; ++value) {
                function(jpegxlOpaque, static_cast<uint32_t>(value), task);
            }
        }
    };
    jobs.parallel_for(static_cast<usize>(taskCount), 1, tasks);
    return JXL_PARALLEL_RET_SUCCESS;
}
