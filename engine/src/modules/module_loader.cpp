#include "modules/module_loader.h"

#include "conjure/modules/module_error.h"
#include "modules/shadow_copy.h"

#include <filesystem>
#include <string_view>
#include <system_error>
#include <utility>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;

result<modules::detail::opened_binary> conjure::modules::detail::open_module_binary(
  const module_manifest &manifest,
  const std::filesystem::path &binaryPath,
  const module_manager_desc &desc,
  u32 copyIndex
) {
    if (!desc.allowSharedLibraries) {
        return make_module_error(
          module_error::SHARED_LIBRARIES_DISABLED,
          "module '{}' needs the shared library '{}' but this host does not load shared libraries",
          manifest.name,
          binaryPath.generic_string()
        );
    }

    std::error_code ignored = {};
    if (!std::filesystem::is_regular_file(binaryPath, ignored)) {
        return make_module_error(
          module_error::BINARY_NOT_FOUND,
          "module '{}': binary '{}' does not exist",
          manifest.name,
          binaryPath.generic_string()
        );
    }

    opened_binary opened = {};
    std::filesystem::path pathToOpen = binaryPath;
    if (desc.shadowCopyBinaries) {
        CONJURE_TRY_ASSIGN(
          pathToOpen,
          shadow_copy_binary(binaryPath, desc.shadowCopyDirectory, manifest.name, copyIndex)
        );
        opened.loadedPath = pathToOpen;
    }

    result<dynamic_library> loaded = dynamic_library::load(std::filesystem::absolute(pathToOpen, ignored));
    if (!loaded.has_value()) {
        remove_shadow_copy(opened.loadedPath);
        return make_module_error(
          module_error::BINARY_LOAD_FAILED,
          "module '{}': {}",
          manifest.name,
          loaded.get_error().message()
        );
    }
    opened.library = std::move(loaded).value();

    const auto describe = opened.library.symbol_as<conjure_module_describe_fn>(CONJURE_MODULE_DESCRIBE_SYMBOL);
    opened.entry = opened.library.symbol_as<conjure_module_entry_fn>(CONJURE_MODULE_ENTRY_SYMBOL);
    if (describe == nullptr || opened.entry == nullptr) {
        const std::string_view missing =
          describe == nullptr ? CONJURE_MODULE_DESCRIBE_SYMBOL : CONJURE_MODULE_ENTRY_SYMBOL;
        opened.library.unload();
        remove_shadow_copy(opened.loadedPath);
        return make_module_error(
          module_error::ENTRY_SYMBOL_MISSING,
          "module '{}': '{}' does not export {} (is it built with CONJURE_DECLARE_MODULE?)",
          manifest.name,
          binaryPath.generic_string(),
          missing
        );
    }

    // describe() is a static-data getter: the only module code that runs before the check.
    const conjure_module_info *info = describe();
    if (info == nullptr) {
        opened.library.unload();
        remove_shadow_copy(opened.loadedPath);
        return make_module_error(
          module_error::ABI_MISMATCH,
          "module '{}': conjure_module_describe returned null",
          manifest.name
        );
    }

    const result<void> compared = compare_abi(info->abi, host_abi_signature());
    if (!compared.has_value()) {
        opened.library.unload();
        remove_shadow_copy(opened.loadedPath);
        return make_module_error(
          module_error::ABI_MISMATCH,
          "module '{}': {}",
          manifest.name,
          compared.get_error().message()
        );
    }

    // Copied before the binary is closed: the name points into the module's own image.
    const std::string binaryName = info->name == nullptr ? std::string() : std::string(info->name);
    if (binaryName != manifest.name) {
        opened.library.unload();
        remove_shadow_copy(opened.loadedPath);
        return make_module_error(
          module_error::NAME_MISMATCH,
          "module '{}': the binary describes itself as '{}'",
          manifest.name,
          binaryName
        );
    }

    return opened;
}
