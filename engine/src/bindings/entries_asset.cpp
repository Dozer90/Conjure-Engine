#include "bindings/entry_support.h"

#include "conjure/bindings/bindings_subsystem.h"

#include "conjure/assets/asset_database.h"
#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_slot.h"
#include "conjure/assets/asset_type_registry.h"
#include "conjure/assets/asset_types.h"

#include <atomic>
#include <optional>
#include <string>

// asset_* (docs/design/15_bindings.md 2.3, 3.7, B9): identity and state queries that never load and never
// bind (a polled uuid costs no permanent slot), the residency scripts request (asset_load /
// asset_load_blocking hold a handle in the active script_residency, asset_release drops it) and the path
// lookups of the editor-side database. Only asset_ref_bind and the loads create slots.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::bindings::detail;

namespace {

    [[nodiscard]] assets::asset_manager *manager() noexcept {
        return active_context().assets;
    }

    /// <summary>
    /// What resolve() does for a uuid nothing has bound yet: LOOKUP answers from the sources and creates no
    /// slot (the read-only entries, so a uuid a script merely polls never costs a permanent slot, 05), BIND
    /// creates it (asset_ref_bind, the one entry the design reserves bind_slot for).
    /// </summary>
    enum class slot_policy : u8 {
        LOOKUP,
        BIND
    };

    struct resolved_slot {
        assets::asset_id id = {};
        const assets::asset_slot *slot = nullptr;                          // nullptr: unbound under LOOKUP
        assets::asset_type_id type = assets::asset_type_id::INVALID;       // the slot's, else the sources'
    };

    /// <summary>
    /// The slot and type of a uuid (never loaded); records INVALID_ARGUMENT for a null pointer or uuid,
    /// NOT_SUPPORTED without a manager and ASSET_TYPE_MISMATCH when a non-zero typeId disagrees with the
    /// known type. An unknown uuid resolves with type INVALID (and no slot under LOOKUP): the entry decides.
    /// </summary>
    [[nodiscard]] bool resolve(
      native_function_id entry,
      const conjure_uuid *id,
      uint64_t typeId,
      slot_policy policy,
      resolved_slot &out,
      int32_t &status
    ) noexcept {
        if (id == nullptr) {
            status = fail_entry(entry, bindings_error::INVALID_ARGUMENT, "id");
            return false;
        }
        if (manager() == nullptr) {
            status = unavailable(entry, "assets");
            return false;
        }
        out.id = marshal::to_uuid(*id);
        if (out.id.is_null()) {
            status = fail_entry(entry, bindings_error::INVALID_ARGUMENT, "null asset uuid");
            return false;
        }
        out.slot = manager()->find_slot(out.id);
        if (out.slot != nullptr) {
            out.type = out.slot->type.load(std::memory_order_acquire);
        }
        if (out.type == assets::asset_type_id::INVALID) {
            out.type = manager()->type_of(out.id).value_or(assets::asset_type_id::INVALID);
        }
        if (policy == slot_policy::BIND) {
            out.slot = manager()->bind_slot(out.id);
            if (out.slot == nullptr) {
                status = fail_entry(entry, bindings_error::INVALID_ARGUMENT, "null asset uuid");
                return false;
            }
        }
        const bool mismatch = typeId != 0 && out.type != assets::asset_type_id::INVALID
          && out.type != static_cast<assets::asset_type_id>(typeId);
        if (mismatch) {
            status = fail_entry(entry, bindings_error::ASSET_TYPE_MISMATCH, "the slot holds another asset type");
            return false;
        }
        return true;
    }

    [[nodiscard]] int32_t unknown_asset(native_function_id entry) noexcept {
        return fail_entry(entry, bindings_error::NOT_FOUND, "the asset is unknown to every source");
    }

    [[nodiscard]] bool hold(
      native_function_id entry,
      const assets::asset_id &id,
      assets::asset_handle handle,
      int32_t &status
    ) noexcept {
        script_residency *residency = scripting_bindings_subsystem::active_residency();
        if (residency == nullptr) {
            status = unavailable(entry, "script residency");
            return false;
        }
        if (!handle.is_bound()) {
            status = fail_entry(entry, bindings_error::NOT_FOUND, "the asset is unknown to every source");
            return false;
        }
        static_cast<void>(residency->hold(id, std::move(handle)));
        return true;
    }
}

int32_t conjure::bindings::entries::asset_ref_get(const conjure_uuid *id, uint64_t typeId, conjure_asset_info *out) {
    constexpr native_function_id ID = native_function_id::asset_ref_get;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (out == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "out");
    }
    int32_t status = 0;
    resolved_slot resolved = {};
    if (!resolve(ID, id, typeId, slot_policy::LOOKUP, resolved, status)) {
        return status;
    }
    if (resolved.slot == nullptr && resolved.type == assets::asset_type_id::INVALID) {
        return unknown_asset(ID);
    }
    // A known uuid nothing bound yet reads as an UNLOADED slot of the sources' type: no slot is created.
    *out = conjure_asset_info{};
    out->state = static_cast<uint8_t>(assets::asset_state::UNLOADED);
    if (resolved.slot != nullptr) {
        out->state = static_cast<uint8_t>(resolved.slot->state.load(std::memory_order_acquire));
        out->version = resolved.slot->version.load(std::memory_order_acquire);
    }
    out->type_id = static_cast<uint64_t>(resolved.type);
    return 0;
}

int32_t conjure::bindings::entries::asset_ref_bind(const conjure_uuid *id, uint64_t typeId, const void **outSlot) {
    constexpr native_function_id ID = native_function_id::asset_ref_bind;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (outSlot == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "outSlot");
    }
    int32_t status = 0;
    resolved_slot resolved = {};
    if (!resolve(ID, id, typeId, slot_policy::BIND, resolved, status)) {
        return status;
    }
    *outSlot = resolved.slot;
    return 0;
}

int32_t conjure::bindings::entries::asset_find(const char *utf8Path, int32_t length, conjure_uuid *outId) {
    constexpr native_function_id ID = native_function_id::asset_find;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (outId == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "outId");
    }
    const core::result<std::string_view> path = marshal::read_utf8(utf8Path, length);
    if (!path.has_value()) {
        return fail_entry(ID, path.get_error());
    }
    if (manager() == nullptr) {
        return unavailable(ID, "assets");
    }
    const std::optional<assets::asset_id> found = manager()->find(*path);
    if (!found.has_value()) {
        return fail_entry(ID, bindings_error::NOT_FOUND, *path);
    }
    *outId = marshal::from_uuid(*found);
    return 0;
}

int32_t conjure::bindings::entries::asset_path_of(const conjure_uuid *id, char *utf8, int32_t capacity) {
    constexpr native_function_id ID = native_function_id::asset_path_of;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (id == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "id");
    }
    if (capacity < 0) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "capacity");
    }
    if (active_context().assetDatabase == nullptr) {
        // Cooked projects have no path table (B9 / section 6).
        return unavailable(ID, "asset database");
    }
    const std::optional<std::string> path = active_context().assetDatabase->asset_path_of(marshal::to_uuid(*id));
    if (!path.has_value()) {
        return fail_entry(ID, bindings_error::NOT_FOUND, "unknown asset uuid");
    }
    return marshal::write_utf8(*path, utf8, capacity);
}

int32_t conjure::bindings::entries::asset_load(const conjure_uuid *id, uint64_t typeId, uint8_t priority) {
    constexpr native_function_id ID = native_function_id::asset_load;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (priority >= static_cast<uint8_t>(assets::ASSET_LOAD_PRIORITY_COUNT)) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "priority");
    }
    int32_t status = 0;
    resolved_slot resolved = {};
    if (!resolve(ID, id, typeId, slot_policy::LOOKUP, resolved, status)) {
        return status;
    }
    if (resolved.type == assets::asset_type_id::INVALID) {
        return unknown_asset(ID);
    }
    script_residency *residency = scripting_bindings_subsystem::active_residency();
    if (residency != nullptr && residency->holds(resolved.id)) {
        return 0;
    }
    // load() binds the slot itself: only a uuid some source knows gets one.
    assets::asset_handle handle = manager()->load(resolved.id, static_cast<assets::asset_load_priority>(priority));
    if (!hold(ID, resolved.id, std::move(handle), status)) {
        return status;
    }
    return 0;
}

int32_t conjure::bindings::entries::asset_load_blocking(const conjure_uuid *id, uint64_t typeId) {
    constexpr native_function_id ID = native_function_id::asset_load_blocking;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    int32_t status = 0;
    resolved_slot resolved = {};
    if (!resolve(ID, id, typeId, slot_policy::LOOKUP, resolved, status)) {
        return status;
    }
    if (resolved.type == assets::asset_type_id::INVALID) {
        return unknown_asset(ID);
    }
    if (scripting_bindings_subsystem::active_residency() == nullptr) {
        return unavailable(ID, "script residency");
    }
    core::result<assets::asset_handle> loaded = manager()->load_blocking(resolved.id);
    if (!loaded.has_value()) {
        return fail_entry(ID, loaded.get_error());
    }
    if (!hold(ID, resolved.id, std::move(loaded).value(), status)) {
        return status;
    }
    return 0;
}

int32_t conjure::bindings::entries::asset_release(const conjure_uuid *id) {
    constexpr native_function_id ID = native_function_id::asset_release;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (id == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "id");
    }
    script_residency *residency = scripting_bindings_subsystem::active_residency();
    if (residency == nullptr) {
        return unavailable(ID, "script residency");
    }
    return residency->release(marshal::to_uuid(*id)) ? 1 : 0;
}

int32_t conjure::bindings::entries::asset_state_of(const conjure_uuid *id) {
    constexpr native_function_id ID = native_function_id::asset_state_of;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (id == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "id");
    }
    if (manager() == nullptr) {
        return unavailable(ID, "assets");
    }
    return static_cast<int32_t>(manager()->state_of(marshal::to_uuid(*id)));
}

int32_t conjure::bindings::entries::asset_type_of(const conjure_uuid *id, uint64_t *outTypeId) {
    constexpr native_function_id ID = native_function_id::asset_type_of;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (outTypeId == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "outTypeId");
    }
    int32_t status = 0;
    resolved_slot resolved = {};
    if (!resolve(ID, id, 0, slot_policy::LOOKUP, resolved, status)) {
        return status;
    }
    if (resolved.type == assets::asset_type_id::INVALID) {
        return unknown_asset(ID);
    }
    *outTypeId = static_cast<uint64_t>(resolved.type);
    return 0;
}

uint64_t conjure::bindings::entries::asset_type_id(const char *utf8TypeName, int32_t length) {
    constexpr native_function_id ID = native_function_id::asset_type_id;
    CONJURE_BINDINGS_GUARD_ENTITY(ID);
    const core::result<std::string_view> name = marshal::read_utf8(utf8TypeName, length);
    if (!name.has_value()) {
        static_cast<void>(fail_entry(ID, name.get_error()));
        return 0;
    }
    if (manager() == nullptr) {
        static_cast<void>(unavailable(ID, "assets"));
        return 0;
    }
    // Only registered types have a loader: an unregistered name is "unknown" (0) rather than a hash
    // nothing can load.
    if (manager()->types().find(*name) == nullptr) {
        static_cast<void>(fail_entry(ID, bindings_error::NOT_FOUND, *name));
        return 0;
    }
    return static_cast<uint64_t>(assets::make_asset_type_id(*name));
}
