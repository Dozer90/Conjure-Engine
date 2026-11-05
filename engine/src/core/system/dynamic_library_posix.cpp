// POSIX backend of core::dynamic_library: dlopen / dlsym / dlclose.

#include "conjure/core/dynamic_library.h"

#include "core/system/system_internal.h"
#include "conjure/core/platform_detect.h"

#include <dlfcn.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <system_error>

using namespace conjure::core;

/*===========================================================================*/
/*===========================================================================*/
// Public methods

result<dynamic_library> dynamic_library::load(const std::filesystem::path &path) {
    if (path.empty()) {
        return error(error_code::INVALID_ARGUMENT, "dynamic_library::load: the path is empty");
    }

    void *handle = ::dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr) {
        const char *reason = ::dlerror();
        const std::string message = reason != nullptr ? std::string(reason) : std::string("unknown error");

        // dlopen reports no error code. A path that names a directory is checked directly: when
        // the file exists the loader's "no such file" refers to a dependency, which is a
        // BACKEND_FAILURE. For a bare name resolved through the search path only the loader's
        // text tells ("No such file or directory" from glibc, "no such file" / "image not found"
        // from dyld).
        error_code code = error_code::BACKEND_FAILURE;
        if (path.has_parent_path()) {
            std::error_code ignored = {};
            if (!std::filesystem::exists(path, ignored)) {
                code = error_code::NOT_FOUND;
            }
        } else {
            std::string lowered = message;
            std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
            const bool missingText = lowered.find("no such file") != std::string::npos
              || lowered.find("not found") != std::string::npos;
            if (missingText) {
                code = error_code::NOT_FOUND;
            }
        }
        return make_error(code, "cannot load '{}': {}", detail::path_to_utf8(path), message);
    }

    dynamic_library library = {};
    library.mHandle = handle;
    library.mPath = path;
    return library;
}

void dynamic_library::unload() noexcept {
    if (mHandle != nullptr) {
        ::dlclose(mHandle);
        mHandle = nullptr;
    }
    mPath.clear();
}

void *dynamic_library::symbol(const char *name) const noexcept {
    if (mHandle == nullptr || name == nullptr) {
        return nullptr;
    }
    return ::dlsym(mHandle, name);
}

std::string_view dynamic_library::extension() noexcept {
#if CONJURE_PLATFORM_MACOS
    return ".dylib";
#else
    return ".so";
#endif
}

std::string_view dynamic_library::prefix() noexcept {
    return "lib";
}
