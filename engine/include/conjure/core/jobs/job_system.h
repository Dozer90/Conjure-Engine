#pragma once

#include "conjure/core/jobs/job_handle.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <functional>
#include <memory>
#include <span>

namespace conjure::core::jobs {

    /// <summary>
    /// Scheduling lane. Workers always take the highest priority job available.
    /// </summary>
    enum class job_priority : u8 {
        HIGH = 0,
        NORMAL,
        LOW
    };

    /// <summary>
    /// Number of priority lanes.
    /// </summary>
    inline constexpr usize JOB_PRIORITY_COUNT = 3;

    /// <summary>
    /// Upper bound accepted for job_system_desc::workerCount.
    /// </summary>
    inline constexpr u32 MAX_WORKER_COUNT = 256;

    /// <summary>
    /// Upper bound accepted for job_system_desc::maxQueuedJobs. start() reserves one pointer per
    /// slot in each priority lane (24 bytes per unit), so the bound keeps a stray value from
    /// exhausting memory.
    /// </summary>
    inline constexpr usize MAX_QUEUED_JOBS = usize{1} << 20;

    /// <summary>
    /// Configuration passed to job_system::start().
    /// </summary>
    struct job_system_desc {
        u32 workerCount = 0;                               // 0 = every job runs inline on the caller
        usize maxQueuedJobs = 16384;                       // <= MAX_QUEUED_JOBS; when full the caller runs jobs itself
        const char *threadNamePrefix = "conjure-worker";   // workers are named "<prefix>-<index>"
    };

    /// <summary>
    /// Counters accumulated since the last start(). Reads are approximate while jobs are running.
    /// stolen + helped + inlineExecutions == executed holds only while no worker executes a job it
    /// queued itself: such a job is neither stolen nor helped and is counted in executed alone.
    /// </summary>
    struct job_system_stats {
        u64 scheduled = 0;            // jobs and parallel_for ranges handed to the system
        u64 executed = 0;             // jobs and ranges that finished executing
        u64 stolen = 0;               // executed by a worker other than the thread that queued the job
        u64 helped = 0;               // executed by a thread inside wait() / wait_idle()
        u64 inlineExecutions = 0;     // executed synchronously by the scheduling thread
        u64 queueFullFallbacks = 0;   // jobs run inline because the queue held maxQueuedJobs
        usize maxQueueDepth = 0;      // most jobs ever queued at once
        usize queuedNow = 0;          // jobs queued at the time of the snapshot
        u32 poolGrowths = 0;          // times the job block pool had to allocate after start()
        usize poolCapacity = 0;       // job blocks owned by the pool
    };

    namespace detail {
        /// <summary>
        /// Executes one parallel_for range against an opaque body. Lets both parallel_for overloads
        /// share one allocation-free dispatch path.
        /// </summary>
        using range_invoke_fn = void (*)(const void *context, usize begin, usize end);
    }

    /// <summary>
    /// Thread pool with a shared, priority-laned job queue. Jobs are std::function objects; a
    /// system started with zero workers (or one that is not running) executes everything inline on
    /// the calling thread, which keeps every caller deterministic and headless-testable. wait()
    /// executes queued jobs on the waiting thread, so waiting from inside a job never deadlocks.
    /// Not copyable or movable: workers hold the address of the system.
    /// </summary>
    class job_system {
        private:
            class state;

            std::unique_ptr<state> mState = nullptr;   // worker threads, queue, block pool and statistics

        public:
            job_system();
            ~job_system();

            job_system(const job_system &) = delete;
            job_system &operator=(const job_system &) = delete;
            job_system(job_system &&) = delete;
            job_system &operator=(job_system &&) = delete;

        public:
            /// <summary>
            /// Creates desc.workerCount named worker threads. Fails with INVALID_STATE when already
            /// running and with INVALID_ARGUMENT for a queue capacity of zero or above
            /// MAX_QUEUED_JOBS, a worker count above MAX_WORKER_COUNT or a null/empty thread name
            /// prefix. Statistics are reset.
            /// </summary>
            [[nodiscard]] result<void> start(const job_system_desc &desc);

            /// <summary>
            /// Stops accepting jobs from non-worker threads, lets the workers finish everything that
            /// is queued, then joins them. Safe to call twice and from the destructor. Must not be
            /// called from one of the system's own workers.
            /// </summary>
            void stop();

            /// <summary>
            /// True between a successful start() and stop().
            /// </summary>
            [[nodiscard]] bool is_running() const noexcept;

            /// <summary>
            /// Number of worker threads, 0 when everything runs inline (not started or started
            /// with workerCount 0).
            /// </summary>
            [[nodiscard]] u32 worker_count() const noexcept;

            /// <summary>
            /// Queues one job and returns its handle. An empty job asserts and yields an invalid
            /// handle. When the system is not running, has no workers or the queue is full, the
            /// job executes on the caller before this returns.
            /// </summary>
            [[nodiscard]] job_handle schedule(std::function<void()> job, job_priority priority = job_priority::NORMAL);

            /// <summary>
            /// Queues a batch and returns one handle that completes when every job in it has run.
            /// Empty entries are skipped (asserting); an empty batch yields an invalid handle.
            /// </summary>
            [[nodiscard]] job_handle schedule(
              std::span<const std::function<void()>> jobs,
              job_priority priority = job_priority::NORMAL
            );

            /// <summary>
            /// Blocks until the handle is complete. While waiting the caller executes queued jobs,
            /// so this is safe from inside a job and from a worker. Returns immediately for an
            /// invalid or complete handle.
            /// </summary>
            void wait(const job_handle &handle);

            /// <summary>
            /// Blocks until no job is queued or running, helping with queued jobs meanwhile. From
            /// inside a job it cannot wait for the jobs the calling thread is itself executing, nor
            /// for jobs that are themselves blocked in wait_idle() (two such jobs would wait for each
            /// other forever); it returns once every other job has finished. Only a call made outside
            /// every job therefore guarantees that the system is completely idle when it returns.
            /// </summary>
            void wait_idle();

            /// <summary>
            /// Splits [0, count) into ranges of at most 'grain' elements (a grain of 0 counts as
            /// 1), runs them across the workers and the caller, and returns when every range has
            /// finished. Runs the whole range inline when there are no workers or count <= grain.
            /// The body is referenced, not copied, and the dispatch does not allocate once the
            /// block pool has warmed up. Nesting from inside a job or another parallel_for is fine.
            /// </summary>
            void parallel_for(usize count, usize grain, const std::function<void(usize begin, usize end)> &body);

            /// <summary>
            /// Per-index variant with grain = max(1, count / (workerCount * 4)).
            /// </summary>
            void parallel_for(usize count, const std::function<void(usize index)> &body);

            /// <summary>
            /// 0 on the main thread and on any thread that is not a job system worker; 1..N on
            /// the workers of the system that owns them.
            /// </summary>
            [[nodiscard]] static u32 thread_index() noexcept;

            /// <summary>
            /// The system whose worker the calling thread is, or nullptr on every other thread.
            /// </summary>
            [[nodiscard]] static job_system *current() noexcept;

            /// <summary>
            /// Snapshot of the counters accumulated since start().
            /// </summary>
            [[nodiscard]] job_system_stats stats() const noexcept;

        private:
            void parallel_for_impl(usize count, usize grain, detail::range_invoke_fn invoke, const void *context);
    };
}
