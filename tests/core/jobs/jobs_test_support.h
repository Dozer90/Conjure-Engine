#pragma once

// Helpers shared by the jobs tests: a job system fixture, a bounded spin-wait and the 0/8 worker
// SUBCASE pair every behavioural test runs through.


#include "conjure/testing/doctest.h"

#include "conjure/core/jobs/job_system.h"

#include <chrono>
#include <thread>

namespace conjure::testing {

    /// <summary>
    /// Starts a job system with the given worker count for the lifetime of the object.
    /// </summary>
    class scoped_job_system {
        private:
            core::jobs::job_system mSystem = {};

        public:
            explicit scoped_job_system(u32 workerCount, usize maxQueuedJobs = 16384);
            ~scoped_job_system();

            scoped_job_system(const scoped_job_system &) = delete;
            scoped_job_system &operator=(const scoped_job_system &) = delete;

        public:
            /// <summary>
            /// The running system.
            /// </summary>
            [[nodiscard]] core::jobs::job_system &system() noexcept {
                return mSystem;
            }

            [[nodiscard]] core::jobs::job_system *operator->() noexcept {
                return &mSystem;
            }
    };

    /// <summary>
    /// Yields until the predicate holds or the timeout expires. Returns the predicate's final value
    /// so a timed-out wait fails the test instead of hanging it.
    /// </summary>
    template <typename Predicate>
    [[nodiscard]] bool spin_until(Predicate &&ready, f64 timeoutSeconds = 10.0) {
        const auto timeout = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
          std::chrono::duration<f64>(timeoutSeconds)
        );
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!ready()) {
            if (std::chrono::steady_clock::now() >= deadline) {
                return ready();
            }
            std::this_thread::yield();
        }
        return true;
    }
}

// Declares `variable` and runs the enclosing TEST_CASE once with 0 workers (everything inline)
// and once with 8 workers.
#define CONJURE_JOBS_WORKER_SUBCASES(variable)                                                              \
    ::conjure::u32 variable = 0;                                                                            \
    SUBCASE("0 workers") {                                                                                  \
        variable = 0;                                                                                       \
    }                                                                                                       \
    SUBCASE("8 workers") {                                                                                  \
        variable = 8;                                                                                       \
    }
