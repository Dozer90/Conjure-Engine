#include "audio/group_table.h"
#include "conjure/audio/audio_error.h"

#include "conjure/core/error.h"

#include <cmath>

using namespace conjure;
using namespace conjure::audio;

namespace {
    constexpr u8 MASTER_INDEX = 0;
    constexpr f32 GAIN_EPSILON = 1.0e-6f;
}

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

group_table::group_table() {
    slot &master = mSlots[MASTER_INDEX];
    master.live = true;
    master.generation = MASTER_GROUP.generation;
    master.parentIndex = MASTER_INDEX;
    mLiveCount = 1;
    rebuild_infos();
}

group_table::~group_table() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

core::result<group_id> group_table::create(const group_desc &desc) {
    if (desc.name.is_null()) {
        return core::make_error(
          audio_error::DUPLICATE_GROUP,
          "a group needs a name (the null name is the master group)"
        );
    }
    if (find(desc.name).is_valid()) {
        return core::make_error(audio_error::DUPLICATE_GROUP, "a group named '{}' already exists", desc.name);
    }

    group_id parent = desc.parent;
    if (!desc.parentName.is_null()) {
        parent = find(desc.parentName);
        if (!parent.is_valid()) {
            return core::make_error(audio_error::INVALID_GROUP, "unknown parent group '{}'", desc.parentName);
        }
    }
    const slot *parentSlot = resolve(parent);
    if (parentSlot == nullptr) {
        return core::make_error(audio_error::INVALID_GROUP, "the parent group id is stale or unknown");
    }
    if (mLiveCount >= MAX_GROUPS) {
        return core::make_error(audio_error::GROUP_LIMIT_REACHED, "{} groups already exist", MAX_GROUPS);
    }
    if (depth_of(parent.index) + 1 > MAX_GROUP_DEPTH) {
        return core::make_error(
          audio_error::GROUP_CYCLE,
          "a group under '{}' would exceed the maximum depth of {}",
          parentSlot->name,
          MAX_GROUP_DEPTH
        );
    }

    u8 index = 0;
    bool found = false;
    for (u8 candidate = 1; candidate < MAX_GROUPS; ++candidate) {
        if (!mSlots[candidate].live) {
            index = candidate;
            found = true;
            break;
        }
    }
    if (!found) {
        return core::make_error(audio_error::GROUP_LIMIT_REACHED, "{} groups already exist", MAX_GROUPS);
    }

    slot &created = mSlots[index];
    created.live = true;
    created.generation = static_cast<u8>(created.generation + 1);
    if (created.generation == 0) {
        created.generation = 1;
    }
    created.name = desc.name;
    created.parentIndex = parent.index;
    created.gain = clamp_group_gain(desc.gain);
    created.effectiveGain = created.gain * parentSlot->effectiveGain;
    created.paused = false;
    ++mLiveCount;
    rebuild_infos();
    return group_id{index, created.generation};
}

core::result<group_removal> group_table::remove(group_id id) {
    if (id == MASTER_GROUP || id.index == MASTER_INDEX) {
        return core::make_error(audio_error::INVALID_GROUP, "the master group cannot be removed");
    }
    const slot *removed = resolve(id);
    if (removed == nullptr) {
        return core::make_error(audio_error::INVALID_GROUP, "the group id is stale or unknown");
    }

    group_removal removal = {};
    removal.parentIndex = removed->parentIndex;

    for (u8 index = 1; index < MAX_GROUPS; ++index) {
        slot &child = mSlots[index];
        if (child.live && index != id.index && child.parentIndex == id.index) {
            child.parentIndex = removal.parentIndex;
            recompute_subtree(index, removal.changedGains);
        }
    }

    slot &retired = mSlots[id.index];
    retired.live = false;
    retired.name = {};
    retired.gain = 1.0f;
    retired.effectiveGain = 1.0f;
    retired.paused = false;
    retired.parentIndex = MASTER_INDEX;
    --mLiveCount;
    rebuild_infos();
    return removal;
}

bool group_table::is_valid(group_id id) const noexcept {
    return resolve(id) != nullptr;
}

group_id group_table::find(core::string_id name) const noexcept {
    if (name.is_null()) {
        return MASTER_GROUP;
    }
    for (u8 index = 1; index < MAX_GROUPS; ++index) {
        if (mSlots[index].live && mSlots[index].name == name) {
            return group_id{index, mSlots[index].generation};
        }
    }
    return group_id{};
}

group_id group_table::id_of(u8 index) const noexcept {
    if (index >= MAX_GROUPS || !mSlots[index].live) {
        return group_id{};
    }
    return group_id{index, mSlots[index].generation};
}

group_change_list group_table::set_gain(group_id id, f32 gain) {
    group_change_list changed = {};
    const slot *target = resolve(id);
    if (target == nullptr) {
        return changed;
    }
    const f32 clamped = clamp_group_gain(gain);
    slot &mutableTarget = mSlots[id.index];
    if (std::abs(mutableTarget.gain - clamped) <= GAIN_EPSILON) {
        return changed;
    }
    mutableTarget.gain = clamped;
    recompute_subtree(id.index, changed);
    rebuild_infos();
    return changed;
}

bool group_table::set_paused(group_id id, bool paused) noexcept {
    if (resolve(id) == nullptr) {
        return false;
    }
    slot &target = mSlots[id.index];
    if (target.paused == paused) {
        return false;
    }
    target.paused = paused;
    rebuild_infos();
    return true;
}

f32 group_table::effective_gain(u8 index) const noexcept {
    if (index >= MAX_GROUPS || !mSlots[index].live) {
        return 1.0f;
    }
    return mSlots[index].effectiveGain;
}

bool group_table::paused(u8 index) const noexcept {
    if (index >= MAX_GROUPS || !mSlots[index].live) {
        return false;
    }
    return mSlots[index].paused;
}

std::optional<group_info> group_table::info(group_id id) const noexcept {
    const slot *target = resolve(id);
    if (target == nullptr) {
        return std::nullopt;
    }
    group_info info = {};
    info.id = group_id{id.index, target->generation};
    info.name = target->name;
    info.parent = id.index == MASTER_INDEX ? group_id{} : id_of(target->parentIndex);
    info.gain = target->gain;
    info.effectiveGain = target->effectiveGain;
    info.paused = target->paused;
    return info;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

const group_table::slot *group_table::resolve(group_id id) const noexcept {
    if (id.index >= MAX_GROUPS || id.generation == 0) {
        return nullptr;
    }
    const slot &candidate = mSlots[id.index];
    if (!candidate.live || candidate.generation != id.generation) {
        return nullptr;
    }
    return &candidate;
}

u32 group_table::depth_of(u8 index) const noexcept {
    u32 depth = 0;
    u8 current = index;
    while (current != MASTER_INDEX && depth < MAX_GROUPS) {
        current = mSlots[current].parentIndex;
        ++depth;
    }
    return depth;
}

void group_table::recompute_subtree(u8 rootIndex, group_change_list &changed) {
    slot &root = mSlots[rootIndex];
    const f32 parentEffective = rootIndex == MASTER_INDEX ? 1.0f : mSlots[root.parentIndex].effectiveGain;
    const f32 effective = root.gain * parentEffective;
    if (std::abs(effective - root.effectiveGain) > GAIN_EPSILON) {
        changed.push_back(rootIndex);
    }
    root.effectiveGain = effective;

    for (u8 index = 1; index < MAX_GROUPS; ++index) {
        if (mSlots[index].live && index != rootIndex && mSlots[index].parentIndex == rootIndex) {
            recompute_subtree(index, changed);
        }
    }
}

void group_table::rebuild_infos() {
    mInfos.clear();
    for (u8 index = 0; index < MAX_GROUPS; ++index) {
        if (!mSlots[index].live) {
            continue;
        }
        const std::optional<group_info> entry = info(group_id{index, mSlots[index].generation});
        if (entry.has_value()) {
            mInfos.push_back(*entry);
        }
    }
}
