// Windows backend of core::dynamic_library: LoadLibraryExW / GetProcAddress / FreeLibrary.

#include "conjure/core/dynamic_library.h"

#include "core/system/system_internal.h"
#include "core/system/win32_utils.h"

#include <Windows.h>

#include <system_error>

using namespace conjure::core;

/*===========================================================================*/
/*===========================================================================*/
// Public methods

result<dynamic_library> dynamic_library::load(const std::filesystem::path &path) {
    if (path.empty()) {
        return error(error_code::INVALID_ARGUMENT, "dynamic_library::load: the path is empty");
    }

    // An absolute path (a plugin) also gets its own directory searched for its dependencies, so a
    // module shipped with a sibling DLL loads wherever the engine's current directory is. The
    // search-flag form needs an absolute path; bare and relative names keep the default search.
    HMODULE module = nullptr;
    if (path.is_absolute()) {
        const DWORD searchFlags = LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;
        module = LoadLibraryExW(path.c_str(), nullptr, searchFlags);
    } else {
        module = LoadLibraryW(path.c_str());
    }
    if (module == nullptr) {
        const DWORD code = GetLastError();
        error_code mapped = detail::error_code_from_win32(code);
        std::string reason = detail::win32_error_message(code);

        // ERROR_MOD_NOT_FOUND covers both the requested file and any of its dependencies being
        // missing; when the file itself is there the dependency is at fault.
        std::error_code ignored = {};
        if (code == ERROR_MOD_NOT_FOUND && path.has_parent_path() && std::filesystem::exists(path, ignored)) {
            mapped = error_code::BACKEND_FAILURE;
            reason += " (the file exists; a library it depends on could not be found)";
        }
        return make_error(mapped, "cannot load '{}': {}", detail::path_to_utf8(path), reason);
    }

    dynamic_library library = {};
    library.mHandle = module;
    library.mPath = path;
    return library;
}

void dynamic_library::unload() noexcept {
    if (mHandle != nullptr) {
        FreeLibrary(static_cast<HMODULE>(mHandle));
        mHandle = nullptr;
    }
    mPath.clear();
}

void *dynamic_library::symbol(const char *name) const noexcept {
    if (mHandle == nullptr || name == nullptr) {
        return nullptr;
    }
    const FARPROC address = GetProcAddress(static_cast<HMODULE>(mHandle), name);
    return reinterpret_cast<void *>(address);
}

std::string_view dynamic_library::extension() noexcept {
    return ".dll";
}

std::string_view dynamic_library::prefix() noexcept {
    return "";
}
