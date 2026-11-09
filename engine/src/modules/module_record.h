#pragma once

#include "conjure/modules/module_consumer.h"
#include "conjure/modules/module_context.h"
#include "conjure/modules/module_entry.h"
#include "conjure/modules/module_manifest.h"
#include "conjure/modules/module_registration_set.h"
#include "conjure/modules/module_types.h"

#include "conjure/core/containers/small_vector.h"
#include "conjure/core/dynamic_library.h"
#include "conjure/core/error.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"

#include <filesystem>
#include <memory>

// The manager's private per-module state (docs/design/07_modules.md 2). Records live in the
// registry, are never destroyed while the manager exists, and keep their slot across unloads; the
// generation inside `id` is what changes.

namespace conjure::modules {

    /// <summary>
    /// A binary an earlier load of the module opened that module_manager_desc::keepBinariesResident
    /// keeps mapped (and its shadow copy on disk) until the manager is destroyed.
    /// </summary>
    struct resident_binary {
        core::dynamic_library library = {};
        std::filesystem::path loadedBinaryPath = {};
    };

    struct module_record {
        module_id id = {};
        core::string_id name = {};
        module_state state = module_state::DISCOVERED;
        module_manifest manifest = {};
        std::filesystem::path rootDirectory = {};
        std::filesystem::path binaryPath = {};              // derived from the manifest; empty = no binary
        std::filesystem::path loadedBinaryPath = {};        // the shadow copy actually opened, if any
        const static_module_desc *staticDesc = nullptr;     // non-null for static modules
        core::dynamic_library library = {};
        core::small_vector<resident_binary, 2> residentBinaries = {};   // keepBinariesResident only
        module_registration_set registrations = {};
        std::unique_ptr<module_context> context = nullptr;
        std::unique_ptr<reload_context> pendingSnapshot = nullptr;
        core::small_vector<module_id, 8> dependencies = {}; // LOADED modules this one depends on
        core::small_vector<module_id, 8> dependents = {};   // LOADED modules depending on this one
        core::error lastError = {};
        u32 loadOrder = 0;
        u32 loadCount = 0;

        /// <summary>
        /// True for the states a load may start from.
        /// </summary>
        [[nodiscard]] bool is_loadable() const noexcept {
            return state == module_state::DISCOVERED || state == module_state::RESOLVED
              || state == module_state::UNLOADED || state == module_state::FAILED;
        }
    };
}
