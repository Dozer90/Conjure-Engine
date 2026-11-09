#include "conjure/modules/module_registry.h"

#include "modules/module_record.h"

#include "conjure/core/assert.h"
#include "conjure/core/containers/flat_map.h"

#include <algorithm>
#include <compare>
#include <memory>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;

namespace {
    struct ownership_key {
        string_id point = {};
        string_id key = {};

        [[nodiscard]] constexpr auto operator<=>(const ownership_key &) const noexcept = default;
    };

    const module_manifest &empty_manifest() noexcept {
        static const module_manifest sEmpty = {};
        return sEmpty;
    }

    const module_registration_set &empty_registrations() noexcept {
        static const module_registration_set sEmpty = {};
        return sEmpty;
    }

    const error &empty_error() noexcept {
        static const error sEmpty = {};
        return sEmpty;
    }

    const std::filesystem::path &empty_path() noexcept {
        static const std::filesystem::path sEmpty = {};
        return sEmpty;
    }
}

struct module_registry::registry_state {
    std::vector<std::unique_ptr<module_record>> records = {};   // stable addresses; slot = index
    std::vector<module_id> allIds = {};                         // current id of every record
    std::vector<module_id> loadOrder = {};
    flat_map<string_id, u16> indexByName = {};
    flat_map<ownership_key, module_id> owners = {};
};

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

module_registry::module_registry() :
    mState(std::make_unique<registry_state>()) {}

module_registry::~module_registry() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

module_id module_registry::find(string_id name) const noexcept {
    const auto found = mState->indexByName.find(name);
    if (found == mState->indexByName.end()) {
        return {};
    }
    return mState->records[found->second]->id;
}

bool module_registry::is_valid(module_id id) const noexcept {
    return record(id) != nullptr;
}

module_state module_registry::state(module_id id) const noexcept {
    const module_record *entry = record(id);
    CONJURE_ASSERT(entry != nullptr, "module_registry::state: invalid or stale id {}", id);
    return entry != nullptr ? entry->state : module_state::UNLOADED;
}

const module_manifest &module_registry::manifest(module_id id) const noexcept {
    const module_record *entry = record(id);
    CONJURE_ASSERT(entry != nullptr, "module_registry::manifest: invalid or stale id {}", id);
    return entry != nullptr ? entry->manifest : empty_manifest();
}

const module_registration_set &module_registry::registrations(module_id id) const noexcept {
    const module_record *entry = record(id);
    CONJURE_ASSERT(entry != nullptr, "module_registry::registrations: invalid or stale id {}", id);
    return entry != nullptr ? entry->registrations : empty_registrations();
}

const error &module_registry::last_error(module_id id) const noexcept {
    const module_record *entry = record(id);
    CONJURE_ASSERT(entry != nullptr, "module_registry::last_error: invalid or stale id {}", id);
    return entry != nullptr ? entry->lastError : empty_error();
}

const std::filesystem::path &module_registry::root_directory(module_id id) const noexcept {
    const module_record *entry = record(id);
    CONJURE_ASSERT(entry != nullptr, "module_registry::root_directory: invalid or stale id {}", id);
    return entry != nullptr ? entry->rootDirectory : empty_path();
}

const std::filesystem::path &module_registry::binary_path(module_id id) const noexcept {
    const module_record *entry = record(id);
    CONJURE_ASSERT(entry != nullptr, "module_registry::binary_path: invalid or stale id {}", id);
    return entry != nullptr ? entry->binaryPath : empty_path();
}

u32 module_registry::load_count(module_id id) const noexcept {
    const module_record *entry = record(id);
    CONJURE_ASSERT(entry != nullptr, "module_registry::load_count: invalid or stale id {}", id);
    return entry != nullptr ? entry->loadCount : 0;
}

std::span<const module_id> module_registry::all() const noexcept {
    return mState->allIds;
}

std::span<const module_id> module_registry::load_order() const noexcept {
    return mState->loadOrder;
}

std::span<const module_id> module_registry::dependents(module_id id) const noexcept {
    const module_record *entry = record(id);
    CONJURE_ASSERT(entry != nullptr, "module_registry::dependents: invalid or stale id {}", id);
    if (entry == nullptr) {
        return {};
    }
    return entry->dependents;
}

std::span<const module_id> module_registry::dependencies(module_id id) const noexcept {
    const module_record *entry = record(id);
    CONJURE_ASSERT(entry != nullptr, "module_registry::dependencies: invalid or stale id {}", id);
    if (entry == nullptr) {
        return {};
    }
    return entry->dependencies;
}

usize module_registry::size() const noexcept {
    return mState->records.size();
}

void module_registry::note_ownership(string_id point, string_id key, module_id owner) {
    CONJURE_ASSERT(is_valid(owner), "module_registry::note_ownership: invalid owner {}", owner);
    if (!is_valid(owner)) {
        return;
    }
    mState->owners.insert_or_assign(ownership_key{point, key}, owner);
}

void module_registry::forget_ownership(module_id owner) {
    auto &owners = mState->owners;
    for (auto iterator = owners.begin(); iterator != owners.end();) {
        if (iterator->second == owner) {
            iterator = owners.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

module_id module_registry::owner_of(string_id point, string_id key) const noexcept {
    const auto found = mState->owners.find(ownership_key{point, key});
    if (found == mState->owners.end()) {
        return {};
    }
    return found->second;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods (manager access)

module_record *module_registry::record(module_id id) noexcept {
    if (!id.is_valid() || id.index >= mState->records.size()) {
        return nullptr;
    }
    module_record &entry = *mState->records[id.index];
    return entry.id.generation == id.generation ? &entry : nullptr;
}

const module_record *module_registry::record(module_id id) const noexcept {
    if (!id.is_valid() || id.index >= mState->records.size()) {
        return nullptr;
    }
    const module_record &entry = *mState->records[id.index];
    return entry.id.generation == id.generation ? &entry : nullptr;
}

module_record &module_registry::record_at(u16 index) noexcept {
    return *mState->records[index];
}

module_record &module_registry::add_record(string_id name) {
    CONJURE_ASSERT(mState->records.size() < module_id::INVALID_INDEX, "module_registry: too many modules");

    std::unique_ptr<module_record> entry = std::make_unique<module_record>();
    entry->id = module_id{static_cast<u16>(mState->records.size()), 0};
    entry->name = name;

    module_record &added = *entry;
    mState->records.push_back(std::move(entry));
    mState->allIds.push_back(added.id);
    mState->indexByName.insert_or_assign(name, added.id.index);
    return added;
}

void module_registry::retire_generation(module_record &entry) {
    ++entry.id.generation;
    mState->allIds[entry.id.index] = entry.id;
}

void module_registry::append_load_order(module_id id) {
    mState->loadOrder.push_back(id);
}

void module_registry::remove_load_order(module_id id) {
    std::vector<module_id> &order = mState->loadOrder;
    order.erase(std::remove(order.begin(), order.end(), id), order.end());
}
