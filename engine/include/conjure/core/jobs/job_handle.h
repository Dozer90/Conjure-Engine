#pragma once

#include "conjure/core/api.h"
#include "conjure/core/types.h"

#include <atomic>
#include <memory>

namespace conjure::core::jobs {

    class job_system;

    namespace detail {
        /// <summary>
        /// Number of jobs still outstanding for one handle. Shared between the handle, the job
        /// system and every queued job block that belongs to the handle. 64 bits wide so a
        /// parallel_for over a huge range can never truncate its range count.
        /// </summary>
        struct job_counter {
            std::atomic<u64> remaining = 0;
        };
    }

    /// <summary>
    /// Reference to the completion state of one scheduled job or of a batch scheduled together.
    /// Handles are cheap to copy; every copy observes the same counter. A default-constructed
    /// handle is invalid and reports itself complete, so waiting on it is a no-op.
    /// </summary>
    class CONJURE_API job_handle {
        private:
            friend class job_system;

            std::shared_ptr<detail::job_counter> mCounter = nullptr;

        public:
            job_handle() noexcept = default;

            /// <summary>
            /// Wraps an existing counter. Used by the job system; callers obtain handles from
            /// job_system::schedule().
            /// </summary>
            explicit job_handle(std::shared_ptr<detail::job_counter> counter) noexcept;

            ~job_handle() = default;

            job_handle(const job_handle &) = default;
            job_handle &operator=(const job_handle &) = default;
            job_handle(job_handle &&) noexcept = default;
            job_handle &operator=(job_handle &&) noexcept = default;

        public:
            /// <summary>
            /// True when this handle refers to scheduled work. False for a default-constructed or
            /// reset handle.
            /// </summary>
            [[nodiscard]] bool valid() const noexcept {
                return mCounter != nullptr;
            }

            /// <summary>
            /// True when every job behind this handle has finished executing. An invalid handle is
            /// always complete.
            /// </summary>
            [[nodiscard]] bool is_complete() const noexcept {
                return mCounter == nullptr || mCounter->remaining.load(std::memory_order_acquire) == 0;
            }

            /// <summary>
            /// Number of jobs behind this handle that have not finished yet. 0 for an invalid handle.
            /// </summary>
            [[nodiscard]] u64 pending() const noexcept {
                return mCounter == nullptr ? 0 : mCounter->remaining.load(std::memory_order_acquire);
            }

            /// <summary>
            /// Drops the reference and makes the handle invalid. The jobs themselves still run.
            /// </summary>
            void reset() noexcept {
                mCounter.reset();
            }

            /// <summary>
            /// Two handles are equal when they observe the same counter (or are both invalid).
            /// </summary>
            [[nodiscard]] bool operator==(const job_handle &other) const noexcept {
                return mCounter == other.mCounter;
            }
    };

    /// <summary>
    /// Caller-owned completion state of one or more batches: the allocation-free counterpart of a
    /// job_handle for code that dispatches the same batch every frame (the ECS scheduler's stages).
    /// job_system::schedule(jobs, fence) adds a batch, job_system::wait(fence) returns once every job
    /// added so far has run. Queued jobs hold the fence's address, so the owner keeps it alive and in
    /// place until it is complete; it can then be reused. Neither copyable nor movable for that reason.
    /// </summary>
    class CONJURE_API job_fence {
        private:
            friend class job_system;

            detail::job_counter mCounter = {};

        public:
            job_fence() noexcept = default;

            /// <summary>
            /// Asserts when jobs are still pending: a batch that outlives its fence would signal freed
            /// memory.
            /// </summary>
            ~job_fence();

            job_fence(const job_fence &) = delete;
            job_fence &operator=(const job_fence &) = delete;
            job_fence(job_fence &&) = delete;
            job_fence &operator=(job_fence &&) = delete;

        public:
            /// <summary>
            /// True when every job scheduled on this fence has finished executing (a fresh fence is
            /// complete).
            /// </summary>
            [[nodiscard]] bool is_complete() const noexcept {
                return pending() == 0;
            }

            /// <summary>
            /// Number of jobs scheduled on this fence that have not finished yet.
            /// </summary>
            [[nodiscard]] u64 pending() const noexcept {
                return mCounter.remaining.load(std::memory_order_acquire);
            }
    };
}
