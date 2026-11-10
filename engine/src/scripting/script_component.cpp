#include "conjure/scripting/script_component.h"

#include "conjure/scripting/log_category.h"
#include "conjure/scripting/script_runtime.h"

#include "conjure/core/assert.h"
#include "conjure/core/json/value.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/log.h"
#include "conjure/core/reflect/type_info.h"
#include "conjure/ecs/component_type_desc.h"

#include <cstring>
#include <utility>

using namespace conjure;
using namespace conjure::scripting;

/*===========================================================================*/
/*===========================================================================*/
// script_store

script_store::script_store() {}

script_store::~script_store() {}

u32 script_store::create_set(ecs::entity owner) {
    u32 index = 0;
    if (!mFreeList.empty()) {
        index = mFreeList.back();
        mFreeList.pop_back();
    } else {
        if (mSlots.size() >= MAX_SETS) {
            CONJURE_ASSERT(false, "script_store: {} sets are live", MAX_SETS);
            return INVALID_SET_HANDLE;
        }
        index = static_cast<u32>(mSlots.size());
        mSlots.emplace_back();
    }

    set_slot &slot = mSlots[index];
    slot.set = {};
    slot.set.owner = owner;
    slot.live = true;
    ++mLiveCount;
    return make_handle(index, slot.generation);
}

u32 script_store::clone_set(u32 handle, ecs::entity owner) {
    const set_slot *source = live_slot(handle);
    if (source == nullptr) {
        return INVALID_SET_HANDLE;
    }

    // The copy is built before the new slot is taken: create_set may reallocate mSlots.
    script_set copy = {};
    copy.owner = owner;
    for (const script_entry &entryValue : source->set.entries) {
        script_entry cloned = {};
        cloned.classHash = entryValue.classHash;
        cloned.managedSlot = INVALID_SLOT;
        cloned.fields = entryValue.fields;
        cloned.flags = entryValue.flags & script_entry_flags::CLASS_MISSING;
        copy.entries.push_back(std::move(cloned));
    }

    const u32 created = create_set(owner);
    if (created == INVALID_SET_HANDLE) {
        return INVALID_SET_HANDLE;
    }
    mSlots[handle_index(created)].set = std::move(copy);
    return created;
}

bool script_store::free_set(u32 handle) {
    set_slot *slot = live_slot(handle);
    if (slot == nullptr) {
        return false;
    }
    for (const script_entry &entryValue : slot->set.entries) {
        if (entryValue.is_live()) {
            unmap_slot(entryValue.managedSlot);
        }
    }
    slot->set = {};
    slot->live = false;
    slot->generation = (slot->generation + 1) & GENERATION_MASK;
    if (slot->generation == 0) {
        slot->generation = 1;
    }
    mFreeList.push_back(handle_index(handle));
    --mLiveCount;
    return true;
}

script_set *script_store::find(u32 handle) noexcept {
    set_slot *slot = live_slot(handle);
    return slot == nullptr ? nullptr : &slot->set;
}

const script_set *script_store::find(u32 handle) const noexcept {
    const set_slot *slot = live_slot(handle);
    return slot == nullptr ? nullptr : &slot->set;
}

script_entry_id script_store::add_entry(u32 handle, script_entry entry) {
    script_set *set = find(handle);
    if (set == nullptr) {
        return {};
    }
    if (set->entries.size() >= MAX_SCRIPTS_PER_ENTITY) {
        CONJURE_LOG_WARN(
          LOG_SCRIPTING,
          "entity {} already carries {} scripts; script {:#018x} not added",
          set->owner,
          MAX_SCRIPTS_PER_ENTITY,
          entry.classHash
        );
        return {};
    }
    script_entry_id id = {};
    id.setHandle = handle;
    id.entryIndex = static_cast<u8>(set->entries.size());
    set->entries.push_back(std::move(entry));
    return id;
}

bool script_store::remove_entry(script_entry_id id) {
    script_set *set = find(id.setHandle);
    if (set == nullptr || id.entryIndex >= set->entries.size()) {
        return false;
    }
    const script_entry &removed = set->entries[id.entryIndex];
    if (removed.is_live()) {
        unmap_slot(removed.managedSlot);
    }
    set->entries.erase(set->entries.begin() + id.entryIndex);

    // Entries after the removed one shifted down: their reverse-map records follow.
    for (usize index = id.entryIndex; index < set->entries.size(); ++index) {
        const script_entry &moved = set->entries[index];
        if (moved.is_live()) {
            script_entry_id movedId = {};
            movedId.setHandle = id.setHandle;
            movedId.entryIndex = static_cast<u8>(index);
            mSlotToEntry[static_cast<usize>(moved.managedSlot)] = movedId;
        }
    }
    return true;
}

script_entry *script_store::find_entry(script_entry_id id) noexcept {
    script_set *set = find(id.setHandle);
    if (set == nullptr || id.entryIndex >= set->entries.size()) {
        return nullptr;
    }
    return &set->entries[id.entryIndex];
}

const script_entry *script_store::find_entry(script_entry_id id) const noexcept {
    const script_set *set = find(id.setHandle);
    if (set == nullptr || id.entryIndex >= set->entries.size()) {
        return nullptr;
    }
    return &set->entries[id.entryIndex];
}

void script_store::map_slot(i32 slot, script_entry_id id) {
    CONJURE_ASSERT(slot >= 0, "script_store::map_slot with slot {}", slot);
    if (slot < 0) {
        return;
    }
    script_entry *entryValue = find_entry(id);
    if (entryValue == nullptr) {
        return;
    }
    const usize index = static_cast<usize>(slot);
    if (index >= mSlotToEntry.size()) {
        mSlotToEntry.resize(index + 1);
    }
    mSlotToEntry[index] = id;
    entryValue->managedSlot = slot;
}

void script_store::unmap_slot(i32 slot) {
    if (slot < 0 || static_cast<usize>(slot) >= mSlotToEntry.size()) {
        return;
    }
    const script_entry_id id = mSlotToEntry[static_cast<usize>(slot)];
    mSlotToEntry[static_cast<usize>(slot)] = {};
    script_entry *entryValue = find_entry(id);
    if (entryValue != nullptr && entryValue->managedSlot == slot) {
        entryValue->managedSlot = INVALID_SLOT;
    }
}

std::optional<script_entry_id> script_store::resolve_slot(i32 slot) const noexcept {
    if (slot < 0 || static_cast<usize>(slot) >= mSlotToEntry.size()) {
        return std::nullopt;
    }
    const script_entry_id id = mSlotToEntry[static_cast<usize>(slot)];
    if (!id.is_valid()) {
        return std::nullopt;
    }
    return id;
}

usize script_store::mapped_slot_count() const noexcept {
    usize count = 0;
    for (const script_entry_id &id : mSlotToEntry) {
        if (id.is_valid()) {
            ++count;
        }
    }
    return count;
}

void script_store::for_each_set(const std::function<void(u32, script_set &)> &callable) {
    for (u32 index = 0; index < mSlots.size(); ++index) {
        set_slot &slot = mSlots[index];
        if (slot.live) {
            callable(make_handle(index, slot.generation), slot.set);
        }
    }
}

void script_store::clear() {
    for (u32 index = 0; index < mSlots.size(); ++index) {
        if (mSlots[index].live) {
            free_set(make_handle(index, mSlots[index].generation));
        }
    }
    mSlotToEntry.clear();
}

script_store::set_slot *script_store::live_slot(u32 handle) noexcept {
    if (handle == INVALID_SET_HANDLE) {
        return nullptr;
    }
    const u32 index = handle_index(handle);
    if (index >= mSlots.size()) {
        return nullptr;
    }
    set_slot &slot = mSlots[index];
    if (!slot.live || slot.generation != handle_generation(handle)) {
        return nullptr;
    }
    return &slot;
}

const script_store::set_slot *script_store::live_slot(u32 handle) const noexcept {
    if (handle == INVALID_SET_HANDLE) {
        return nullptr;
    }
    const u32 index = handle_index(handle);
    if (index >= mSlots.size()) {
        return nullptr;
    }
    const set_slot &slot = mSlots[index];
    if (!slot.live || slot.generation != handle_generation(handle)) {
        return nullptr;
    }
    return &slot;
}


/*===========================================================================*/
/*===========================================================================*/
// Component hooks

namespace {

    void copy_script_components(void *destination, const void *source, u32 count) {
        auto *target = static_cast<script_component *>(destination);
        const auto *origin = static_cast<const script_component *>(source);
        script_runtime *runtime = script_runtime::active();
        for (u32 index = 0; index < count; ++index) {
            target[index].setHandle = INVALID_SET_HANDLE;
            if (origin[index].setHandle == INVALID_SET_HANDLE) {
                continue;
            }
            if (runtime == nullptr) {
                CONJURE_LOG_WARN(LOG_SCRIPTING, "script_component copied without a script_runtime; scripts dropped");
                continue;
            }
            target[index].setHandle = runtime->clone_set_for_copy(origin[index].setHandle);
        }
    }

    void destruct_script_components(void *object, u32 count) {
        auto *components = static_cast<script_component *>(object);
        script_runtime *runtime = script_runtime::active();
        for (u32 index = 0; index < count; ++index) {
            if (components[index].setHandle == INVALID_SET_HANDLE) {
                continue;
            }
            if (runtime != nullptr) {
                runtime->free_set_from_destruct(components[index].setHandle);
            }
            components[index].setHandle = INVALID_SET_HANDLE;
        }
    }

    core::result<void> write_script_component_json(const void *component, core::json::writer &out) {
        const auto *value = static_cast<const script_component *>(component);
        script_runtime *runtime = script_runtime::active();
        if (runtime == nullptr || value->setHandle == INVALID_SET_HANDLE) {
            out.begin_object();
            out.key("scripts");
            out.begin_array();
            out.end_array();
            out.end_object();
            return core::result<void>::success();
        }
        runtime->write_set_json(value->setHandle, out);
        return core::result<void>::success();
    }

    core::result<void> read_script_component_json(void *component, const core::json::value &in) {
        auto *value = static_cast<script_component *>(component);
        script_runtime *runtime = script_runtime::active();
        if (runtime == nullptr) {
            return core::error(core::error_code::INVALID_STATE, "script_component read without a live script_runtime");
        }
        CONJURE_TRY_ASSIGN(const u32 handle, runtime->read_set_json(value->setHandle, in));
        value->setHandle = handle;
        return core::result<void>::success();
    }

    void remap_script_component_entities(void *component, const ecs::entity_remap &map) {
        auto *value = static_cast<script_component *>(component);
        script_runtime *runtime = script_runtime::active();
        if (runtime == nullptr || value->setHandle == INVALID_SET_HANDLE) {
            return;
        }
        runtime->remap_set_entities(value->setHandle, map);
    }
}

ecs::component_ops conjure::scripting::script_component_ops() noexcept {
    ecs::component_ops ops = {};
    ops.copy = &copy_script_components;
    ops.destruct = &destruct_script_components;
    return ops;
}

ecs::component_serializer conjure::scripting::script_component_serializer() noexcept {
    ecs::component_serializer serializer = {};
    serializer.writeJson = &write_script_component_json;
    serializer.readJson = &read_script_component_json;
    serializer.remapEntities = &remap_script_component_entities;
    return serializer;
}

core::result<ecs::component_id> conjure::scripting::register_script_component(ecs::component_registry &registry) {
    static const std::string sName(SCRIPT_COMPONENT_NAME);
    ecs::component_type_desc desc = {};
    desc.name = sName.c_str();
    desc.size = sizeof(script_component);
    desc.alignment = alignof(script_component);
    desc.flags = ecs::component_flags::NONE;
    desc.ops = script_component_ops();
    // No reflection: the handle is never meaningful across sessions. The serializer hooks carry the
    // script set through archives, scene files, prefabs and snapshots (R29).
    desc.reflection = nullptr;
    desc.serializer = script_component_serializer();
    return registry.register_described(desc);
}
