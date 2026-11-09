#pragma once

#include "conjure/modules/module_manager.h"
#include "conjure/modules/module_manifest.h"
#include "conjure/modules/module_types.h"

#include "conjure/core/event_bus.h"

#include <filesystem>
#include <string>
#include <string_view>

// The fixture plugins built by tests/modules/CMakeLists.txt live under CONJURE_TEST_MODULE_DIR/<name>/
// with a copy of their module.json and the binary at bin/<platform>-<arch>-<config>/.

namespace conjure::modules::testing {

    inline std::filesystem::path fixture_root() {
        return std::filesystem::path(CONJURE_TEST_MODULE_DIR);
    }

    inline std::filesystem::path fixture_directory(std::string_view name) {
        return fixture_root() / std::string(name);
    }

    inline std::filesystem::path fixture_manifest(std::string_view name) {
        return fixture_directory(name) / MODULE_MANIFEST_FILE_NAME;
    }

    /// <summary>
    /// The binary of a fixture whose manifest "binary" equals its directory name, for this build.
    /// </summary>
    inline std::filesystem::path fixture_binary(std::string_view name) {
        module_manifest manifest = {};
        manifest.binary = std::string(name);
        return module_binary_path(manifest, fixture_directory(name), host_binary_triplet());
    }

    inline module_manager_desc shared_desc() noexcept {
        module_manager_desc desc = {};
        desc.host = module_host_kind::TEST;
        desc.allowSharedLibraries = true;
        return desc;
    }

    /// <summary>
    /// A manager that loads shared libraries, with its own event bus.
    /// </summary>
    struct shared_host {
        core::event_bus events = {};
        module_manager_desc desc = {};
        module_manager manager;     // no default: built from desc below

        shared_host() :
            desc(shared_desc()),
            manager(desc, events) {}

        explicit shared_host(const module_manager_desc &customDesc) :
            desc(customDesc),
            manager(desc, events) {}
    };
}
