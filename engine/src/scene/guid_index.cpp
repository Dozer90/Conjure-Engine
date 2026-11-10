#include "conjure/scene/guid_index.h"

using namespace conjure;
using namespace conjure::scene;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

guid_index::guid_index() {
    mEntries.reserve(INITIAL_CAPACITY);
}

guid_index::~guid_index() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

ecs::entity guid_index::find(const core::uuid &id) const noexcept {
    const auto found = mEntries.find(id);
    if (found == mEntries.end()) {
        return ecs::NULL_ENTITY;
    }
    return found->second;
}

bool guid_index::contains(const core::uuid &id) const noexcept {
    return mEntries.contains(id);
}

u32 guid_index::size() const noexcept {
    return static_cast<u32>(mEntries.size());
}

void guid_index::reserve(u32 count) {
    mEntries.reserve(count);
}

bool guid_index::insert(const core::uuid &id, ecs::entity target) {
    if (id.is_null()) {
        return false;
    }
    return mEntries.try_emplace(id, target).second;
}

void guid_index::erase(const core::uuid &id, ecs::entity expected) noexcept {
    const auto found = mEntries.find(id);
    if (found == mEntries.end() || !(found->second == expected)) {
        return;
    }
    mEntries.erase(found);
}

void guid_index::rebind(ecs::entity target, const core::uuid &oldId, const core::uuid &newId) {
    erase(oldId, target);
    const bool inserted = insert(newId, target);
    CONJURE_UNUSED(inserted);
}

void guid_index::clear() noexcept {
    mEntries.clear();
}

usize guid_index::capacity() const noexcept {
    return mEntries.capacity();
}
