#include "engine/executable_path.h"

#include "conjure/engine/log_category.h"

#include "conjure/core/log.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/types.h"

#include <algorithm>
#include <system_error>
#include <vector>

#if CONJURE_PLATFORM_WINDOWS
    #include <windows.h>
#elif CONJURE_PLATFORM_MACOS
    #include <mach-o/dyld.h>
#else
    #include <unistd.h>
#endif

using namespace conjure;

namespace {

#if CONJURE_PLATFORM_WINDOWS
    // The longest path Windows accepts with long paths enabled (32767 wide characters plus the terminator).
    constexpr usize LONGEST_PATH = 32768;
#elif !CONJURE_PLATFORM_MACOS
    // A generous ceiling for /proc/self/exe; PATH_MAX is 4096, but a link target is not bound by it.
    constexpr usize LONGEST_PATH = 65536;
#endif
}

std::filesystem::path conjure::engine::executable_directory() {
    std::filesystem::path executable = {};
#if CONJURE_PLATFORM_WINDOWS
    // GetModuleFileNameW truncates silently when the buffer is too small (it returns the buffer size), so the
    // buffer grows until the whole path fits.
    std::vector<wchar_t> buffer(MAX_PATH, L'\0');
    while (true) {
        const DWORD size = static_cast<DWORD>(buffer.size());
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), size);
        if (length == 0) {
            break;
        }
        if (length < size) {
            executable = std::filesystem::path(buffer.data(), buffer.data() + length);
            break;
        }
        if (buffer.size() >= LONGEST_PATH) {
            break;
        }
        buffer.assign(std::min(buffer.size() * 2, LONGEST_PATH), L'\0');
    }
#elif CONJURE_PLATFORM_MACOS
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size + 1, '\0');
    if (size > 0 && _NSGetExecutablePath(buffer.data(), &size) == 0) {
        executable = std::filesystem::path(buffer.data());
    }
#else
    // readlink fills at most the buffer and does not say whether it truncated: a result that fills the
    // buffer is retried with a larger one.
    std::vector<char> buffer(4096, '\0');
    while (true) {
        const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (length <= 0) {
            break;
        }
        if (static_cast<usize>(length) < buffer.size()) {
            executable = std::filesystem::path(std::string(buffer.data(), static_cast<usize>(length)));
            break;
        }
        if (buffer.size() >= LONGEST_PATH) {
            break;
        }
        buffer.assign(std::min(buffer.size() * 2, LONGEST_PATH), '\0');
    }
#endif
    if (executable.empty()) {
        std::error_code ignored = {};
        const std::filesystem::path current = std::filesystem::current_path(ignored);
        CONJURE_LOG_WARN(
          LOG_ENGINE,
          "could not determine the executable's directory; using the current directory '{}'",
          current.generic_string()
        );
        return current;
    }
    return executable.parent_path();
}
