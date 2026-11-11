#pragma once

#include "conjure/core/api.h"
#include "conjure/core/types.h"

#include <functional>
#include <thread>

// The main thread and its task queue. Worker threads and dedicated threads post work that must
// run on the main thread (window, GPU submission, editor state); the engine drains the queue once
// per frame before FRAME_BEGIN.

namespace conjure::core::jobs {

    /// <summary>
    /// True on the main thread: the thread that first called set_main_thread() or constructed the
    /// first job_system. When neither has happened, the first thread to ask becomes the main thread.
    /// </summary>
    [[nodiscard]] CONJURE_API bool is_main_thread() noexcept;

    /// <summary>
    /// Designates the calling thread as the main thread. The engine calls this at startup; tests
    /// call it before using the main-thread queue.
    /// </summary>
    CONJURE_API void set_main_thread() noexcept;

    /// <summary>
    /// Forgets the designation so the next set_main_thread() / is_main_thread() claims it again.
    /// Intended for tests and for restarting the engine inside one process.
    /// </summary>
    CONJURE_API void reset_main_thread() noexcept;

    /// <summary>
    /// Identifier of the designated main thread, or a default-constructed id when none is set.
    /// Does not claim the calling thread.
    /// </summary>
    [[nodiscard]] CONJURE_API std::thread::id main_thread_id() noexcept;

    /// <summary>
    /// Queues a task for the next drain_main_thread_queue(). Thread-safe. Empty tasks assert and
    /// are dropped.
    /// </summary>
    CONJURE_API void post_to_main_thread(std::function<void()> task);

    /// <summary>
    /// Runs every task queued so far, once, in posting order, and returns how many ran. Tasks
    /// posted while draining run on the next drain. Called off the designated main thread (or
    /// before one is designated) it asserts, runs nothing, leaves the tasks queued and returns 0;
    /// it never claims the main-thread designation for the caller.
    /// </summary>
    CONJURE_API usize drain_main_thread_queue();

    /// <summary>
    /// Number of tasks waiting for the next drain.
    /// </summary>
    [[nodiscard]] CONJURE_API usize main_thread_queue_size();
}
