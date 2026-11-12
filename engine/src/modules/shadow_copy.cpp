#include "modules/shadow_copy.h"

#include "conjure/modules/module_error.h"
#include "modules/log_category.h"

#include "conjure/core/log.h"

#include <filesystem>
#include <format>
#include <string>
#include <system_error>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;

namespace {
    /// <summary>
    /// Copies a file or a directory tree if the source exists; a missing source is not an error.
    /// </summary>
    result<void> copy_companion(const std::filesystem::path &source, const std::filesystem::path &destination) {
        std::error_code failure = {};
        if (!std::filesystem::exists(source, failure)) {
            return {};
        }
        const std::filesystem::copy_options options =
          std::filesystem::copy_options::overwrite_existing | std::filesystem::copy_options::recursive;
        std::filesystem::copy(source, destination, options, failure);
        if (failure) {
            return make_module_error(
              module_error::BINARY_COPY_FAILED,
              "cannot copy '{}' to '{}': {}",
              source.generic_string(),
              destination.generic_string(),
              failure.message()
            );
        }
        return {};
    }
}

result<std::filesystem::path> conjure::modules::detail::shadow_copy_binary(
  const std::filesystem::path &binaryPath,
  const std::filesystem::path &cacheDirectory,
  std::string_view moduleName,
  u32 copyIndex
) {
    if (cacheDirectory.empty()) {
        return make_module_error(
          module_error::BINARY_COPY_FAILED,
          "cannot shadow copy '{}': module_manager_desc::shadowCopyDirectory is empty",
          binaryPath.generic_string()
        );
    }

    const std::filesystem::path targetDirectory = cacheDirectory / std::string(moduleName) / std::to_string(copyIndex);
    std::error_code failure = {};
    std::filesystem::create_directories(targetDirectory, failure);
    if (failure) {
        return make_module_error(
          module_error::BINARY_COPY_FAILED,
          "cannot create shadow copy directory '{}': {}",
          targetDirectory.generic_string(),
          failure.message()
        );
    }

    const std::filesystem::path target = targetDirectory / binaryPath.filename();
    std::filesystem::copy_file(binaryPath, target, std::filesystem::copy_options::overwrite_existing, failure);
    if (failure) {
        return make_module_error(
          module_error::BINARY_COPY_FAILED,
          "cannot copy '{}' to '{}': {}",
          binaryPath.generic_string(),
          target.generic_string(),
          failure.message()
        );
    }

    // Debug companions: <stem>.pdb next to a DLL, <name>.dSYM next to a dylib.
    std::filesystem::path pdb = binaryPath;
    pdb.replace_extension(".pdb");
    CONJURE_TRY(copy_companion(pdb, targetDirectory / pdb.filename()));

    const std::filesystem::path dsym = binaryPath.string() + ".dSYM";
    CONJURE_TRY(copy_companion(dsym, targetDirectory / dsym.filename()));

    return target;
}

void conjure::modules::detail::clean_shadow_copy_cache(const std::filesystem::path &cacheDirectory) {
    std::error_code failure = {};
    if (cacheDirectory.empty() || !std::filesystem::is_directory(cacheDirectory, failure)) {
        return;
    }

    // The non-throwing increment: engine code never throws, and a listing that fails halfway ends
    // in the warning below.
    std::filesystem::directory_iterator iterator(cacheDirectory, failure);
    for (const std::filesystem::directory_iterator end = {}; !failure && iterator != end; iterator.increment(failure)) {
        const std::filesystem::directory_entry &entry = *iterator;
        std::error_code removeFailure = {};
        std::filesystem::remove_all(entry.path(), removeFailure);
        if (removeFailure) {
            CONJURE_LOG_WARN(
              LOG_MODULES,
              "shadow copy cache: cannot remove '{}' ({}); a loaded module may still hold it",
              entry.path().generic_string(),
              removeFailure.message()
            );
        }
    }
    if (failure) {
        CONJURE_LOG_WARN(
          LOG_MODULES,
          "shadow copy cache: cannot list '{}': {}",
          cacheDirectory.generic_string(),
          failure.message()
        );
    }
}

void conjure::modules::detail::remove_shadow_copy(const std::filesystem::path &copiedBinaryPath) noexcept {
    if (copiedBinaryPath.empty()) {
        return;
    }
    std::error_code ignored = {};
    std::filesystem::remove_all(copiedBinaryPath.parent_path(), ignored);
}
