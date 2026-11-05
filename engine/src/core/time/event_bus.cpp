#include "conjure/core/event_bus.h"

using namespace conjure;
using namespace conjure::core;

/*===========================================================================*/
/*===========================================================================*/
// channel_base

event_bus::channel_base::channel_base() {}

event_bus::channel_base::~channel_base() {}


/*===========================================================================*/
/*===========================================================================*/
// event_bus

event_bus::event_bus() {}

event_bus::~event_bus() {}

usize event_bus::dispatch_queued() {
    // Take the whole batch under the lock, then publish outside it so handlers (and other
    // threads) can enqueue while dispatching without deadlocking or being delivered early.
    std::vector<deferred_publish> batch = {};
    {
        const std::scoped_lock lock(mQueueMutex);
        batch.swap(mQueue);
    }

    for (deferred_publish &deferred : batch) {
        deferred(*this);
    }
    return batch.size();
}

usize event_bus::queued_count() const {
    const std::scoped_lock lock(mQueueMutex);
    return mQueue.size();
}

void event_bus::clear_queue() {
    std::vector<deferred_publish> dropped = {};
    {
        const std::scoped_lock lock(mQueueMutex);
        dropped.swap(mQueue);
    }
    // 'dropped' is destroyed here, outside the lock, in case an event destructor is expensive.
}
