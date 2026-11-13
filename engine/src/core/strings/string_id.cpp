#include "conjure/core/string_id.h"

#include "conjure/core/assert.h"
#include "conjure/core/log.h"

#include <cstddef>
#include <memory>
#include <mutex>
#include <new>
#include <shared_mutex>
#include <unordered_map>

using namespace conjure::core;

/*===========================================================================*/
/*===========================================================================*/
// Name table

namespace {
    struct name_table {
        // Shared for lookups and for registrations of names that are already known, which is by far
        // the common case (the same runtime names are constructed again and again, and hierarchy
        // views and log formatting resolve them every frame); exclusive only to insert a new name.
        std::shared_mutex mutex = {};

        // Node-based so the strings never move: views handed out by find() stay valid for the
        // lifetime of the process. Entries are never erased.
        std::unordered_map<conjure::u64, std::string> names = {};
    };

    // Function-local so ids constructed by other translation units during static initialisation
    // find a live table whatever the link order. The table lives in raw storage and is never
    // destroyed: static destructors in other translation units (log sinks flushing, module
    // registries) may still format or construct ids, and a destroyed map or mutex would be
    // undefined behaviour. The leak is intentional and bounded to one table per process.
    name_table &table() {
        alignas(name_table) static std::byte storage[sizeof(name_table)] = {};
        static name_table *instance = std::construct_at(reinterpret_cast<name_table *>(storage));
        return *instance;
    }
}

void conjure::core::detail::register_string_id_name(conjure::u64 hash, std::string_view name) {
    name_table &names = table();

    // The locks cover only the map: logging and asserting below call out to user code (sinks,
    // the assert handler) that may itself resolve or construct ids, which would re-enter the
    // non-recursive mutex on this thread.
    std::string existing = {};
    bool known = false;
    {
        const std::shared_lock lock(names.mutex);
        const auto found = names.names.find(hash);
        if (found != names.names.end()) {
            if (found->second == name) {
                return;
            }
            known = true;
            existing = found->second;
        }
    }

    // Another thread may insert the same hash between the two locks, so the insertion checks again.
    if (!known) {
        const std::scoped_lock lock(names.mutex);
        const auto [slot, inserted] = names.names.try_emplace(hash, name);
        if (inserted || slot->second == name) {
            return;
        }
        existing = slot->second;
    }

    // Two different names with one FNV-1a 64 hash. The first spelling wins so existing views stay
    // valid; the caller has to rename one of them.
    CONJURE_LOG_ERROR(
      LOG_CORE,
      "string_id hash collision: \"{}\" and \"{}\" both hash to {:#018x}",
      existing,
      name,
      hash
    );
    CONJURE_ASSERT(false, "string_id hash collision between \"{}\" and \"{}\"", existing, name);
}

std::string_view conjure::core::detail::find_string_id_name(conjure::u64 hash) noexcept {
    name_table &names = table();
    const std::shared_lock lock(names.mutex);

    const auto found = names.names.find(hash);
    if (found == names.names.end()) {
        return std::string_view("", 0);
    }
    return found->second;
}

conjure::usize conjure::core::detail::string_id_name_count() noexcept {
    name_table &names = table();
    const std::shared_lock lock(names.mutex);
    return names.names.size();
}


/*===========================================================================*/
/*===========================================================================*/
// string_id

string_id string_id::intern(std::string_view name) {
    const string_id id = from_hash(hash::fnv1a64(name));
    detail::register_string_id_name(id.mHash, name);
    return id;
}

std::string string_id::to_string() const {
    const std::string_view name = view();
    if (!name.empty()) {
        return std::string(name);
    }
    return std::format("#{:016x}", mHash);
}
