#pragma once

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <filesystem>
#include <string_view>

// Windows locks a loaded DLL (docs/design/07_modules.md D9). With module_manager_desc::
// shadowCopyBinaries the manager copies a module binary (and its .pdb / .dSYM when present) to
// <cache>/<name>/<n>/ and loads the copy, so a rebuild can overwrite the original while the module
// is loaded. The code is std::filesystem throughout; on Windows each path it hands to a file call takes
// the extended-length form from 248 characters on (long_path.h), so a cache or binary past MAX_PATH works,
// and elsewhere paths are used as given.

namespace conjure::modules::detail {

    /// <summary>
    /// Copies 'binaryPath' and its debug companions to &lt;cacheDirectory&gt;/&lt;moduleName&gt;/&lt;copyIndex&gt;/
    /// and returns the copied binary's path. BINARY_COPY_FAILED with the OS message otherwise.
    /// </summary>
    [[nodiscard]] core::result<std::filesystem::path> shadow_copy_binary(
      const std::filesystem::path &binaryPath,
      const std::filesystem::path &cacheDirectory,
      std::string_view moduleName,
      u32 copyIndex
    );

    /// <summary>
    /// Removes every entry inside the cache directory left by earlier runs. A locked file (a
    /// module still loaded by another process) is skipped with a WARN. A missing directory is fine.
    /// </summary>
    void clean_shadow_copy_cache(const std::filesystem::path &cacheDirectory);

    /// <summary>
    /// Best-effort removal of one shadow copy directory after its library was closed.
    /// </summary>
    void remove_shadow_copy(const std::filesystem::path &copiedBinaryPath) noexcept;
}
