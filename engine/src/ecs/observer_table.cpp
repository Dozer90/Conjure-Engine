#include "ecs/observer_table.h"

#include "conjure/core/assert.h"

#include <utility>

using namespace conjure;
using namespace conjure::ecs;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

observer_table::observer_table() {}

observer_table::~observer_table() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

observer_id observer_table::add_component_observer(component_id id, component_event event, component_observer_fn fn) {
    CONJURE_ASSERT(static_cast<bool>(fn), "observe requires a callable");
    if (!fn) {
        return observer_id{};
    }
    const u32 index = acquire_slot();
    slot &entry = mSlots[index];
    entry.alive = true;
    entry.observesEntities = false;
    entry.id = id;
    entry.event = event;
    entry.componentFn = std::move(fn);
    ++mComponentObserverCount;
    ++mObserverCounts[interest_index(id, event)];
    mObserved[static_cast<usize>(event)].set(id);
    return observer_id{index, entry.generation};
}

observer_id observer_table::add_entity_observer(entity_observer_fn fn) {
    CONJURE_ASSERT(static_cast<bool>(fn), "observe_destroyed requires a callable");
    if (!fn) {
        return observer_id{};
    }
    const u32 index = acquire_slot();
    slot &entry = mSlots[index];
    entry.alive = true;
    entry.observesEntities = true;
    entry.entityFn = std::move(fn);
    ++mEntityObserverCount;
    return observer_id{index, entry.generation};
}

bool observer_table::remove(observer_id id) {
    if (!id.is_valid() || id.index >= mSlots.size()) {
        return false;
    }
    slot &entry = mSlots[id.index];
    if (!entry.alive || entry.generation != id.generation) {
        return false;
    }

    entry.alive = false;
    if (entry.observesEntities) {
        --mEntityObserverCount;
    } else {
        --mComponentObserverCount;
        u32 &interested = mObserverCounts[interest_index(entry.id, entry.event)];
        --interested;
        if (interested == 0) {
            mObserved[static_cast<usize>(entry.event)].clear(entry.id);
        }
    }

    // The handler may be the one running right now: destroy it once every fire has returned.
    if (mFiringDepth > 0) {
        mPendingRelease.push_back(id.index);
    } else {
        release_slot(id.index);
    }
    return true;
}

void observer_table::fire_component(
  world &owner,
  entity target,
  component_id id,
  component_event event,
  command_buffer &deferred
) {
    if (mComponentObserverCount == 0) {
        return;
    }
    // Checked per call, so an observer registered by a handler of an earlier bit of the same mask is
    // still invoked for a later bit, exactly as the full scan does.
    if (!mObserved[static_cast<usize>(event)].test(id)) {
        return;
    }
    ++mFiringDepth;
    // Observers registered during a fire land past 'count' and are first invoked by the next fire; the
    // deque keeps 'entry' valid while its handler registers them.
    const usize count = mSlots.size();
    for (usize index = 0; index < count; ++index) {
        const slot &entry = mSlots[index];
        if (!entry.alive || entry.observesEntities || entry.id != id || entry.event != event) {
            continue;
        }
        entry.componentFn(owner, target, id, deferred);
    }
    --mFiringDepth;
    finish_fire();
}

void observer_table::fire_destroyed(world &owner, entity target, command_buffer &deferred) {
    if (mEntityObserverCount == 0) {
        return;
    }
    ++mFiringDepth;
    const usize count = mSlots.size();
    for (usize index = 0; index < count; ++index) {
        const slot &entry = mSlots[index];
        if (!entry.alive || !entry.observesEntities) {
            continue;
        }
        entry.entityFn(owner, target, deferred);
    }
    --mFiringDepth;
    finish_fire();
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

u32 observer_table::acquire_slot() {
    // During a fire a freed slot below the fire's 'count' would be visited as the new observer; a fresh
    // slot past 'count' keeps "registered during a fire, invoked from the next fire on".
    u32 index = 0;
    if (!mFree.empty() && mFiringDepth == 0) {
        index = mFree.back();
        mFree.pop_back();
    } else {
        index = static_cast<u32>(mSlots.size());
        mSlots.push_back(slot{});
    }
    slot &entry = mSlots[index];
    entry.generation = mNextGeneration;
    ++mNextGeneration;
    if (mNextGeneration == 0) {
        mNextGeneration = 1;
    }
    return index;
}

void observer_table::release_slot(u32 index) {
    slot &entry = mSlots[index];
    entry.componentFn = {};
    entry.entityFn = {};
    mFree.push_back(index);
}

void observer_table::finish_fire() {
    if (mFiringDepth > 0 || mPendingRelease.empty()) {
        return;
    }
    // Take the list first: destroying a handler may unobserve other observers.
    std::vector<u32> pending = std::move(mPendingRelease);
    mPendingRelease.clear();
    for (const u32 index : pending) {
        release_slot(index);
    }
}

usize observer_table::interest_index(component_id id, component_event event) noexcept {
    return static_cast<usize>(event) * MAX_COMPONENTS + id.value;
}
