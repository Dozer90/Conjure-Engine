#pragma once

#include "physics/jolt_include.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/types.h"

#include <Jolt/Core/FixedSizeFreeList.h>
#include <Jolt/Core/JobSystemWithBarrier.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <vector>

// JPH::JobSystem over core::jobs (docs/design/14_physics.md P2). Jolt builds its job graph inside
// PhysicsSystem::Update and waits on a barrier that executes jobs on the calling thread; this adapter
// hands the other jobs to the engine's workers. core::jobs::job_system::schedule() allocates a shared
// counter per call, which would break the allocation-free step (P15), so the adapter uses the
// allocation-free parallel_for instead: run_parallel() dispatches worker_count() helper ranges that pop
// Jolt jobs from a fixed-size queue for the duration of the step while range 0 (always the caller) runs
// the step itself. With 0 workers every job executes inline inside QueueJob, like
// JPH::JobSystemSingleThreaded.

namespace conjure::physics {

    class jolt_job_system final : public JPH::JobSystemWithBarrier {
        private:
            using job_pool = JPH::FixedSizeFreeList<Job>;

            core::jobs::job_system *mJobs = nullptr;
            job_pool mPool = {};
            std::vector<Job *> mQueue = {};              // ring of mCapacity slots
            u32 mCapacity = 0;
            u32 mHead = 0;                               // next slot to pop (guarded by mQueueMutex)
            u32 mTail = 0;                               // next slot to push (guarded by mQueueMutex)
            std::mutex mQueueMutex = {};
            std::atomic<u32> mGeneration = 0;            // bumped on every push and at step end; helpers wait on it
            std::atomic<bool> mParallelActive = false;   // true while run_parallel owns the helpers
            std::atomic<u32> mQueuedCount = 0;
            std::atomic<u64> mExecutedByHelpers = 0;

        public:
            jolt_job_system();
            ~jolt_job_system() override;

        public:
            /// <summary>
            /// Sizes the job pool and the queue (maxJobs) and the barrier array (maxBarriers) and binds the
            /// engine job system.
            /// </summary>
            void init(u32 maxJobs, u32 maxBarriers, core::jobs::job_system &jobs);

            /// <summary>
            /// Runs 'body' on the calling thread while worker_count() helpers execute the Jolt jobs it
            /// queues; returns once the body and every helper finished. With 0 workers the body simply runs.
            /// </summary>
            template <typename Body>
            void run_parallel(Body &&body) {
                const u32 helpers = helper_count();
                if (helpers == 0) {
                    body();
                    return;
                }
                begin_parallel();
                const std::function<void(usize, usize)> ranges = [this, &body](usize begin, usize end) {
                    CONJURE_UNUSED(end);
                    if (begin == 0) {
                        body();
                        end_parallel();
                        return;
                    }
                    help_until_done();
                };
                mJobs->parallel_for(static_cast<usize>(helpers) + 1, 1, ranges);
                drain_queue();
            }

            /// <summary>
            /// Workers that take part in run_parallel (0 when the engine job system runs inline).
            /// </summary>
            [[nodiscard]] u32 helper_count() const noexcept;

            /// <summary>
            /// Jobs executed by helper ranges since init (tests).
            /// </summary>
            [[nodiscard]] u64 executed_by_helpers() const noexcept {
                return mExecutedByHelpers.load(std::memory_order_relaxed);
            }

            /// <summary>
            /// Jobs waiting in the queue right now.
            /// </summary>
            [[nodiscard]] u32 queued_count() const noexcept {
                return mQueuedCount.load(std::memory_order_relaxed);
            }

            // JPH::JobSystem
            [[nodiscard]] int GetMaxConcurrency() const override;
            [[nodiscard]] JobHandle CreateJob(
              const char *name,
              JPH::ColorArg color,
              const JobFunction &function,
              JPH::uint32 dependencyCount = 0
            ) override;

        protected:
            void QueueJob(Job *job) override;
            void QueueJobs(Job **jobs, JPH::uint count) override;
            void FreeJob(Job *job) override;

        private:
            void begin_parallel() noexcept;
            void end_parallel() noexcept;
            void help_until_done();
            void drain_queue();
            [[nodiscard]] bool push(Job *job) noexcept;
            [[nodiscard]] Job *pop() noexcept;
    };
}
