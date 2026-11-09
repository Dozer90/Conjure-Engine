#pragma once

#include "conjure/modules/module_abi.h"
#include "conjure/modules/module_manager.h"
#include "conjure/modules/module_manifest.h"

#include "conjure/core/dynamic_library.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <filesystem>

// The shared-library half of load_one (docs/design/07_modules.md 3.3): everything that happens
// before any C++ object crosses into the module. Every failure here is side-effect free except for
// a closed library handle.

namespace conjure::modules::detail {

    struct opened_binary {
        core::dynamic_library library = {};
        conjure_module_entry_fn entry = nullptr;
        std::filesystem::path loadedPath = {};      // the shadow copy when one was made
    };

    /// <summary>
    /// Checks the manager settings, finds (and optionally shadow copies) the binary, opens it,
    /// looks up the two entry symbols, runs describe() and compares the ABI signature and name
    /// with the manifest. SHARED_LIBRARIES_DISABLED, BINARY_NOT_FOUND, BINARY_COPY_FAILED,
    /// BINARY_LOAD_FAILED, ENTRY_SYMBOL_MISSING, ABI_MISMATCH or NAME_MISMATCH on failure.
    /// </summary>
    [[nodiscard]] core::result<opened_binary> open_module_binary(
      const module_manifest &manifest,
      const std::filesystem::path &binaryPath,
      const module_manager_desc &desc,
      u32 copyIndex
    );
}
