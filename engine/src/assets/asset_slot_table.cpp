#include "assets/asset_slot_table.h"

#include "conjure/core/assert.h"

using namespace conjure;
using namespace conjure::assets;
using namespace conjure::assets::detail;

asset_slot_table::asset_slot_table() {}

asset_slot_table::~asset_slot_table() {}

asset_slot *asset_slot_table::find(const asset_id &id) noexcept {
    const auto found = mIndexById.find(id);
    if (found == mIndexById.end()) {
        return nullptr;
    }
    return &mSlots[found->second];
}

const asset_slot *asset_slot_table::find(const asset_id &id) const noexcept {
    const auto found = mIndexById.find(id);
    if (found == mIndexById.end()) {
        return nullptr;
    }
    return &mSlots[found->second];
}

asset_slot *asset_slot_table::find_or_create(const asset_id &id, bool &outCreated) {
    outCreated = false;
    if (id.is_null()) {
        return nullptr;
    }
    asset_slot *existing = find(id);
    if (existing != nullptr) {
        return existing;
    }

    const u32 index = static_cast<u32>(mSlots.size());
    asset_slot &slot = mSlots.emplace_back();
    slot.id = id;
    slot.index = index;
    mData.emplace_back();
    mIndexById.try_emplace(id, index);
    outCreated = true;
    return &slot;
}

slot_data &asset_slot_table::data(const asset_slot &slot) noexcept {
    CONJURE_ASSERT(slot.index < mData.size(), "slot index {} out of range", slot.index);
    return mData[slot.index];
}

const slot_data &asset_slot_table::data(const asset_slot &slot) const noexcept {
    CONJURE_ASSERT(slot.index < mData.size(), "slot index {} out of range", slot.index);
    return mData[slot.index];
}

asset_slot &asset_slot_table::at(u32 index) noexcept {
    return mSlots[index];
}

const asset_slot &asset_slot_table::at(u32 index) const noexcept {
    return mSlots[index];
}
