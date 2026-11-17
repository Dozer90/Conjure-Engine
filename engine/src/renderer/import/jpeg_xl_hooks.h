#pragma once

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/types.h"

#include <jxl/memory_manager.h>
#include <jxl/parallel_runner.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>

// The engine services libjxl runs on (docs/design/02_renderer.md 3.13): its allocations go through
// core::memory (CONVENTIONS.md 3: no malloc outside core::memory) and its parallel work onto core::jobs, so the
// engine never starts a thread of its own for it (libjxl's jxl_threads runner is not linked). Only the JPEG XL
// decoder and encoder translation units include this header.

namespace conjure::renderer::detail {

    /// <summary>
    /// The bytes libjxl may hold at once during one decode. Every block the memory manager of that decode hands
    /// out counts against 'limitBytes' until it is freed; a request that would take the live total above the
    /// limit is refused (libjxl then fails the decode cleanly) and sets 'exceeded'. libjxl allocates from the job
    /// system's workers, so the fields are atomic. The budget must outlive the decoder that uses it.
    /// </summary>
    struct jpeg_xl_memory_budget {
        std::atomic<u64> liveBytes = 0;
        std::atomic<u64> limitBytes = std::numeric_limits<u64>::max();
        std::atomic<bool> exceeded = false;
    };

    /// <summary>
    /// A JxlMemoryManager whose blocks come from core::memory::tracked_allocate, aligned like malloc's, and count
    /// against 'budget' when one is given (none: only the system limits apply, as for the encoder).
    /// </summary>
    [[nodiscard]] JxlMemoryManager jpeg_xl_memory_manager(jpeg_xl_memory_budget *budget = nullptr) noexcept;

    /// <summary>
    /// True when 'jobs' can run libjxl's work on more than one thread: it exists, is running and has workers.
    /// Without that libjxl gets no runner and works on the calling thread.
    /// </summary>
    [[nodiscard]] bool can_run_jpeg_xl_in_parallel(const core::jobs::job_system *jobs) noexcept;

    /// <summary>
    /// The JxlParallelRunner over a core::jobs::job_system passed as 'runnerOpaque'. [startRange, endRange) is
    /// split into one contiguous task per thread that can take part (the workers and the caller), never more
    /// tasks than values or than JPEG_XL_MAX_THREADS (jpeg_xl_codec.h), and task k calls 'function' for each of
    /// its values with thread id k. A task runs on one thread at a time, so the ids libjxl indexes its per-thread
    /// storage with never collide, and the split does not depend on which threads pick the tasks up. Returns what
    /// 'init' returned when that failed.
    /// </summary>
    JxlParallelRetCode run_jpeg_xl_on_jobs(
      void *runnerOpaque,
      void *jpegxlOpaque,
      JxlParallelRunInit init,
      JxlParallelRunFunction function,
      uint32_t startRange,
      uint32_t endRange
    );
}
