#include "ecs/entity_allocator.h"

#include "conjure/ecs/ecs_error.h"
#include "conjure/ecs/log_category.h"

#include "conjure/core/assert.h"
#include "conjure/core/log.h"

#include <mutex>

using namespace conjure;
using namespace conjure::ecs;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

entity_allocator::entity_allocator() {}

entity_allocator::~entity_allocator() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<void> entity_allocator::reserve(std::span<entity> out) {
    const std::scoped_lock lock(mMutex);

    // Check the whole request first so a failure takes nothing.
    const usize fromFreeList = out.size() < mFreeList.size() ? out.size() : mFreeList.size();
    const usize fresh = out.size() - fromFreeList;
    if (fresh > 0 && (fresh > static_cast<usize>(MAX_ENTITY_INDEX) - mNextIndex + 1)) {
        return core::make_error(ecs_error::ENTITY_LIMIT_REACHED, "the entity index space is exhausted");
    }

    usize written = 0;
    while (written < fromFreeList) {
        const free_slot slot = mFreeList.back();
        mFreeList.pop_back();
        out[written] = entity{slot.index, slot.generation};
        ++written;
    }
    while (written < out.size()) {
        out[written] = entity{mNextIndex, 1};
        ++mNextIndex;
        ++written;
    }
    return core::result<void>::success();
}

void entity_allocator::release(entity target) {
    const std::scoped_lock lock(mMutex);
    const u32 next = target.generation + 1;
    if (next >= ARCHIVE_LOCAL_GENERATION || next == 0) {
        // The generation space of this index is used up: never hand the index out again so a stale
        // handle can never alias a future entity.
        ++mRetiredCount;
        CONJURE_LOG_DEBUG(LOG_ECS, "entity index {} retired after {} generations", target.index, target.generation);
        return;
    }
    mFreeList.push_back(free_slot{target.index, next});
}

void entity_allocator::return_unused(std::span<const entity> unused) {
    const std::scoped_lock lock(mMutex);
    for (const entity value : unused) {
        mFreeList.push_back(free_slot{value.index, value.generation});
    }
}

u32 entity_allocator::peak_index() const noexcept {
    const std::scoped_lock lock(mMutex);
    return mNextIndex;
}

std::vector<entity> entity_allocator::free_slots() const {
    const std::scoped_lock lock(mMutex);
    std::vector<entity> slots = {};
    slots.reserve(mFreeList.size());
    for (const free_slot &slot : mFreeList) {
        slots.push_back(entity{slot.index, slot.generation});
    }
    return slots;
}
