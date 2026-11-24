// POSIX backend of the module system's long-path helpers (long_path.h): Linux and macOS have no
// classic path limit like Windows' MAX_PATH below their PATH_MAX, so every path is used as given.

#include "modules/long_path.h"

#include <filesystem>
#include <string>

std::filesystem::path conjure::modules::detail::extend_path(const std::filesystem::path &path) {
    return path;
}

std::filesystem::path conjure::modules::detail::always_extended_path(const std::filesystem::path &path) {
    return path;
}

std::string conjure::modules::detail::path_length_note(const std::filesystem::path &path) {
    static_cast<void>(path);
    return {};
}
