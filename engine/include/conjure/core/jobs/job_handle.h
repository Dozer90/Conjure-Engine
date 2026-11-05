#pragma once

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
    class job_handle {
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
}
