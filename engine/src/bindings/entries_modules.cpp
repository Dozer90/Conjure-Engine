#include "bindings/entry_support.h"

#include "conjure/core/string_id.h"
#include "conjure/modules/module_manifest.h"
#include "conjure/modules/module_registry.h"
#include "conjure/modules/module_types.h"
#include "conjure/modules/module_version.h"

#include <span>

// modules_* (docs/design/15_bindings.md 2.3, B12, B13): a read-only, index-based view of
// modules::module_registry. Indices address registry.all() (registration order, stable across loads and
// unloads); the record carries the state so scripts can tell a loaded module from a registered one.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::bindings::detail;

namespace {

    [[nodiscard]] const modules::module_registry *registry() noexcept {
        return active_context().modules;
    }

    [[nodiscard]] bool resolve_index(
      native_function_id id,
      int32_t index,
      modules::module_id &out,
      int32_t &status
    ) noexcept {
        const std::span<const modules::module_id> all = registry()->all();
        if (index < 0 || static_cast<usize>(index) >= all.size()) {
            status = fail_entry(id, bindings_error::NOT_FOUND, "module index");
            return false;
        }
        out = all[static_cast<usize>(index)];
        return true;
    }
}

int32_t conjure::bindings::entries::modules_count(void) {
    constexpr native_function_id ID = native_function_id::modules_count;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (registry() == nullptr) {
        return unavailable(ID, "module registry");
    }
    return static_cast<int32_t>(registry()->all().size());
}

int32_t conjure::bindings::entries::modules_find(const char *utf8Name, int32_t length) {
    constexpr native_function_id ID = native_function_id::modules_find;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    const core::result<std::string_view> name = marshal::read_utf8(utf8Name, length);
    if (!name.has_value()) {
        return fail_entry(ID, name.get_error());
    }
    if (registry() == nullptr) {
        return unavailable(ID, "module registry");
    }
    // from_hash: a lookup never interns the queried string (the registry interned the real names at load).
    const modules::module_id found = registry()->find(core::string_id::from_hash(core::hash::fnv1a64(*name)));
    if (!found.is_valid()) {
        return fail_entry(ID, bindings_error::NOT_FOUND, *name);
    }
    const std::span<const modules::module_id> all = registry()->all();
    for (usize index = 0; index < all.size(); ++index) {
        if (all[index] == found) {
            return static_cast<int32_t>(index);
        }
    }
    return fail_entry(ID, bindings_error::NOT_FOUND, *name);
}

int32_t conjure::bindings::entries::modules_info(int32_t index, conjure_module_record *out) {
    constexpr native_function_id ID = native_function_id::modules_info;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (out == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "out");
    }
    if (registry() == nullptr) {
        return unavailable(ID, "module registry");
    }
    int32_t status = 0;
    modules::module_id id = {};
    if (!resolve_index(ID, index, id, status)) {
        return status;
    }
    const modules::module_manifest &manifest = registry()->manifest(id);
    *out = conjure_module_record{};
    out->version_major = manifest.version.majorVersion;
    out->version_minor = manifest.version.minorVersion;
    out->version_patch = manifest.version.patchVersion;
    out->state = static_cast<uint8_t>(registry()->state(id));
    out->kind = static_cast<uint8_t>(manifest.kind);
    out->feature_flags = static_cast<uint32_t>(manifest.requiredFeatures);
    return 0;
}

int32_t conjure::bindings::entries::modules_name(int32_t index, char *utf8, int32_t capacity) {
    constexpr native_function_id ID = native_function_id::modules_name;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (capacity < 0) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "capacity");
    }
    if (registry() == nullptr) {
        return unavailable(ID, "module registry");
    }
    int32_t status = 0;
    modules::module_id id = {};
    if (!resolve_index(ID, index, id, status)) {
        return status;
    }
    return marshal::write_utf8(registry()->manifest(id).name, utf8, capacity);
}
