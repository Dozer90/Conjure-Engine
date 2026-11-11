#pragma once

#include "conjure/core/types.h"

#include <functional>
#include <mutex>
#include <vector>

// The application's main-thread task queue (docs/design/06_engine_loop.md D5, 3.1 step 4): tasks posted
// from any thread run on the main thread before the next FRAME_BEGIN. The drain swaps the vector out under
// the lock and runs the tasks unlocked, so a task posting another task never deadlocks and lands in the
// next frame (no re-entrancy, section 6).

namespace conjure::engine {

    class main_thread_queue {
        private:
            std::mutex mMutex = {};
            std::vector<std::function<void()>> mTasks = {};     // guarded by mMutex
            std::vector<std::function<void()>> mDraining = {};  // main thread only, reused across drains
            bool mIsDraining = false;
            u64 mTotalPosted = 0;                               // guarded by mMutex
            u64 mTotalRun = 0;                                  // main thread only

        public:
            main_thread_queue();
            ~main_thread_queue();

            main_thread_queue(const main_thread_queue &) = delete;
            main_thread_queue &operator=(const main_thread_queue &) = delete;

        public:
            /// <summary>
            /// Thread-safe. An empty task asserts and is dropped.
            /// </summary>
            void post(std::function<void()> task);

            /// <summary>
            /// Runs every task posted before the call in posting order and returns how many ran. Tasks posted
            /// while draining wait for the next drain. Main thread only.
            /// </summary>
            usize drain();

            /// <summary>
            /// Drops every pending task (shutdown).
            /// </summary>
            void clear();

            /// <summary>
            /// Tasks waiting for the next drain. Thread-safe.
            /// </summary>
            [[nodiscard]] usize pending_count();

            /// <summary>
            /// True while drain() runs (main thread).
            /// </summary>
            [[nodiscard]] bool is_draining() const noexcept {
                return mIsDraining;
            }

            [[nodiscard]] u64 total_run() const noexcept {
                return mTotalRun;
            }
    };
}
