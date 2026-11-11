#include "engine/main_thread_queue.h"

#include "conjure/core/assert.h"

#include <utility>

using namespace conjure;
using namespace conjure::engine;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

main_thread_queue::main_thread_queue() {}

main_thread_queue::~main_thread_queue() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

void main_thread_queue::post(std::function<void()> task) {
    CONJURE_ASSERT(static_cast<bool>(task), "main_thread_queue::post requires a callable task");
    if (!task) {
        return;
    }
    const std::lock_guard<std::mutex> lock(mMutex);
    mTasks.push_back(std::move(task));
    ++mTotalPosted;
}

usize main_thread_queue::drain() {
    CONJURE_ASSERT(!mIsDraining, "main_thread_queue::drain re-entered");
    if (mIsDraining) {
        return 0;
    }
    {
        const std::lock_guard<std::mutex> lock(mMutex);
        std::swap(mTasks, mDraining);
    }
    mIsDraining = true;
    for (std::function<void()> &task : mDraining) {
        task();
    }
    const usize count = mDraining.size();
    mTotalRun += count;
    mDraining.clear();
    mIsDraining = false;
    return count;
}

void main_thread_queue::clear() {
    const std::lock_guard<std::mutex> lock(mMutex);
    mTasks.clear();
}

usize main_thread_queue::pending_count() {
    const std::lock_guard<std::mutex> lock(mMutex);
    return mTasks.size();
}
