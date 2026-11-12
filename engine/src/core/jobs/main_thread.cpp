#include "conjure/core/jobs/main_thread.h"

#include "conjure/core/assert.h"

#include <atomic>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;

/*===========================================================================*/
/*===========================================================================*/
// Process-wide state

namespace {
    struct main_thread_state {
        std::atomic<std::thread::id> mainThread = std::thread::id();
        std::mutex queueMutex = {};
        std::vector<std::function<void()>> queue = {};   // guarded by queueMutex
    };

    // Function-local so the state is ready whatever the static initialisation order of the
    // translation units that post during start-up.
    main_thread_state &state() noexcept {
        static main_thread_state instance = {};
        return instance;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Main thread identity

bool conjure::core::jobs::is_main_thread() noexcept {
    const std::thread::id self = std::this_thread::get_id();
    std::thread::id expected = std::thread::id();

    // Nobody has claimed the main thread yet: the first thread to ask becomes it.
    if (state().mainThread.compare_exchange_strong(expected, self, std::memory_order_acq_rel)) {
        return true;
    }
    return expected == self;
}

void conjure::core::jobs::set_main_thread() noexcept {
    state().mainThread.store(std::this_thread::get_id(), std::memory_order_release);
}

void conjure::core::jobs::reset_main_thread() noexcept {
    state().mainThread.store(std::thread::id(), std::memory_order_release);
}

std::thread::id conjure::core::jobs::main_thread_id() noexcept {
    return state().mainThread.load(std::memory_order_acquire);
}

/*===========================================================================*/
/*===========================================================================*/
// Task queue

void conjure::core::jobs::post_to_main_thread(std::function<void()> task) {
    CONJURE_ASSERT(task != nullptr, "post_to_main_thread: empty task");
    if (!task) {
        return;
    }

    main_thread_state &shared = state();
    const std::scoped_lock lock(shared.queueMutex);
    shared.queue.push_back(std::move(task));
}

usize conjure::core::jobs::drain_main_thread_queue() {
    // Compare against the designation instead of calling is_main_thread(): the drain must never
    // claim the main thread for a caller, and off the main thread the tasks have to stay queued
    // (they exist precisely because they may only run there).
    const bool onMainThread = main_thread_id() == std::this_thread::get_id();
    CONJURE_ASSERT(onMainThread, "drain_main_thread_queue called off the main thread");
    if (!onMainThread) {
        return 0;
    }

    // Swap the queue out so tasks posted while draining (including from the tasks themselves)
    // are left for the next drain and the lock is never held while user code runs.
    main_thread_state &shared = state();
    std::vector<std::function<void()>> tasks = {};
    {
        const std::scoped_lock lock(shared.queueMutex);
        tasks.swap(shared.queue);
    }

    for (std::function<void()> &task : tasks) {
        task();
    }
    const usize ran = tasks.size();

    // The callables are destroyed outside the lock. The emptied buffer goes back when nothing was posted
    // meanwhile, so a steady stream of posts reuses one allocation instead of growing a fresh queue
    // after every drain.
    tasks.clear();
    {
        const std::scoped_lock lock(shared.queueMutex);
        if (shared.queue.empty()) {
            shared.queue.swap(tasks);
        }
    }
    return ran;
}

usize conjure::core::jobs::main_thread_queue_size() {
    main_thread_state &shared = state();
    const std::scoped_lock lock(shared.queueMutex);
    return shared.queue.size();
}
