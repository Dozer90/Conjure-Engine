#include "engine/executable_path.h"

#include "conjure/core/platform_detect.h"

#include <system_error>

#if CONJURE_PLATFORM_WINDOWS
    #include <windows.h>
#elif CONJURE_PLATFORM_MACOS
    #include <mach-o/dyld.h>
    #include <vector>
#else
    #include <unistd.h>
    #include <vector>
#endif

using namespace conjure;

std::filesystem::path conjure::engine::executable_directory() {
    std::filesystem::path executable = {};
#if CONJURE_PLATFORM_WINDOWS
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        executable = std::filesystem::path(buffer, buffer + length);
    }
#elif CONJURE_PLATFORM_MACOS
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size + 1, '\0');
    if (size > 0 && _NSGetExecutablePath(buffer.data(), &size) == 0) {
        executable = std::filesystem::path(buffer.data());
    }
#else
    std::vector<char> buffer(4096, '\0');
    const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length > 0) {
        executable = std::filesystem::path(std::string(buffer.data(), static_cast<size_t>(length)));
    }
#endif
    if (executable.empty()) {
        std::error_code ignored = {};
        return std::filesystem::current_path(ignored);
    }
    return executable.parent_path();
}
