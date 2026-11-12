// POSIX backend of runtime_platform.h (Linux and macOS): argv is already bytes the file system understands, the
// executable is found through /proc/self/exe or _NSGetExecutablePath, standard error and standard output are
// probed with fcntl, and there is no dialog (the text is on the standard streams, which a desktop launcher
// captures in its journal).

#include "runtime_platform.h"

#include "conjure/core/platform_detect.h"

#include <fcntl.h>
#include <unistd.h>

#include <cstdint>
#include <string>
#include <system_error>
#include <vector>

#if CONJURE_PLATFORM_MACOS
    #include <mach-o/dyld.h>
#endif

std::vector<std::string> conjure::runtime::utf8_arguments(int argc, char **argv) {
    std::vector<std::string> arguments = {};
    arguments.reserve(argc > 1 ? static_cast<size_t>(argc) - 1 : 0);
    for (int index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }
    return arguments;
}

std::filesystem::path conjure::runtime::executable_directory() {
    std::filesystem::path executable = {};
#if CONJURE_PLATFORM_MACOS
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

bool conjure::runtime::has_standard_error() {
    return fcntl(STDERR_FILENO, F_GETFD) != -1;
}

bool conjure::runtime::has_standard_output() {
    return fcntl(STDOUT_FILENO, F_GETFD) != -1;
}

void conjure::runtime::show_error_dialog(std::string_view title, std::string_view text) {
    static_cast<void>(title);
    static_cast<void>(text);
}

void conjure::runtime::show_info_dialog(std::string_view title, std::string_view text) {
    static_cast<void>(title);
    static_cast<void>(text);
}
