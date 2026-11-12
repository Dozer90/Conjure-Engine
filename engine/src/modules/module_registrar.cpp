#include "conjure/modules/module_registrar.h"
#include "modules/manifest_paths.h"

#include "conjure/core/assert.h"

#include <algorithm>
#include <cstring>
#include <format>
#include <string>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

module_registrar::module_registrar(module_registration_set &set) noexcept :
    mSet(&set) {}

module_registrar::~module_registrar() {}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

void module_registrar::register_managed_assembly(std::string_view relativePath) {
    CONJURE_ASSERT(!relativePath.empty(), "register_managed_assembly: the path is empty");
    if (relativePath.empty()) {
        return;
    }

    // export_layout() writes these into the exported manifest, which must pass the same check as
    // the manifest's own "managed" entries or the shipped game would refuse the module.
    const bool insideRoot = modules::detail::stays_inside_root(relativePath);
    CONJURE_ASSERT(insideRoot, "register_managed_assembly: '{}' leaves the module root", relativePath);
    if (!insideRoot) {
        mSet->errors.emplace_back(
          std::format("register_managed_assembly: '{}' must be a relative path inside the module root", relativePath)
        );
        return;
    }

    std::vector<std::string> &assemblies = mSet->managedAssemblies;
    if (std::find(assemblies.begin(), assemblies.end(), relativePath) != assemblies.end()) {
        return;
    }
    assemblies.emplace_back(relativePath);
}

void module_registrar::set_lifecycle(const module_lifecycle &lifecycle) noexcept {
    mSet->lifecycle = lifecycle;
}

void module_registrar::fail(std::string_view reason) {
    mSet->errors.emplace_back(reason.empty() ? std::string_view("unspecified reason") : reason);
}

const module_registration_set &module_registrar::registrations() const noexcept {
    return *mSet;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

void module_registrar::append_extension(string_id point, const void *data, usize size, usize alignment) {
    CONJURE_ASSERT(!point.is_null(), "register_extension: the extension point is null");
    CONJURE_ASSERT(size > 0 && alignment > 0, "register_extension: empty descriptor");
    if (point.is_null() || size == 0 || alignment == 0) {
        return;
    }

    std::vector<std::byte> &storage = mSet->extensionStorage;
    const usize padded = (storage.size() + alignment - 1) / alignment * alignment;
    storage.resize(padded + size);
    std::memcpy(storage.data() + padded, data, size);

    extension_record record = {};
    record.point = point;
    record.offset = static_cast<u32>(padded);
    record.size = static_cast<u32>(size);
    record.alignment = static_cast<u32>(alignment);
    mSet->extensions.push_back(record);
}
