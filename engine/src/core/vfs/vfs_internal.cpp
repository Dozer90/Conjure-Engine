#include "core/vfs/vfs_internal.h"

#include "conjure/core/platform_detect.h"
#include "conjure/core/vfs/path_utils.h"

#include <array>
#include <chrono>
#include <format>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::vfs;

/*===========================================================================*/
/*===========================================================================*/
// Helpers

namespace {
    constexpr std::string_view RESERVED_CHARACTERS = ":*?\"<>|";
    constexpr std::array<std::string_view, 4> DEVICE_NAMES = {"CON", "PRN", "AUX", "NUL"};
    constexpr std::array<std::string_view, 2> NUMBERED_DEVICE_STEMS = {"COM", "LPT"};

    char to_upper_ascii(char character) noexcept {
        if (character >= 'a' && character <= 'z') {
            return static_cast<char>(character - 'a' + 'A');
        }
        return character;
    }

    bool equals_upper_case(std::string_view text, std::string_view upperCase) noexcept {
        if (text.size() != upperCase.size()) {
            return false;
        }
        for (usize index = 0; index < text.size(); ++index) {
            if (to_upper_ascii(text[index]) != upperCase[index]) {
                return false;
            }
        }
        return true;
    }

    bool is_control_character(char character) noexcept {
        const unsigned char value = static_cast<unsigned char>(character);
        return value < 0x20 || value == 0x7F;
    }

    // Windows decides by what stands before the first dot, trailing spaces dropped and case ignored,
    // so "nul", "NUL.txt", "Aux.tar.gz" and "con .log" all name the device, never a file.
    bool is_device_name(std::string_view name) noexcept {
        std::string_view base = name.substr(0, name.find('.'));
        while (!base.empty() && base.back() == ' ') {
            base.remove_suffix(1);
        }

        for (const std::string_view device : DEVICE_NAMES) {
            if (equals_upper_case(base, device)) {
                return true;
            }
        }
        if (base.size() != 4 || base[3] < '1' || base[3] > '9') {
            return false;
        }
        for (const std::string_view stem : NUMBERED_DEVICE_STEMS) {
            if (equals_upper_case(base.substr(0, 3), stem)) {
                return true;
            }
        }
        return false;
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Paths

result<std::string> internal::resolve_backend_path(std::string_view path) {
    const auto escapes = [path] {
        return make_error(error_code::PERMISSION_DENIED, std::format("path '{}' leaves the file system root", path));
    };

    // A single leading separator means "from the backend root"; a second one is a UNC prefix and
    // a drive letter is never relative. Both are escapes however they normalise.
    std::string_view relative = path;
    if (relative.starts_with('/') || relative.starts_with('\\')) {
        relative.remove_prefix(1);
    }
    if (relative.starts_with('/') || relative.starts_with('\\')) {
        return escapes();
    }
    if (relative.size() >= 2 && relative[1] == ':') {
        return escapes();
    }

    // Normalising the now-relative spelling keeps a leading ".." instead of dropping it against an
    // absolute root, so "/../x" is rejected rather than silently remapped to "x".
    const std::string normalized = path_utils::normalize(relative);
    if (normalized == ".." || normalized.starts_with("../")) {
        return escapes();
    }
    // "./C:/x" only reveals its drive letter once normalised.
    if (normalized.starts_with('/') || (normalized.size() >= 2 && normalized[1] == ':')) {
        return escapes();
    }
    return normalized;
}

std::string_view internal::portable_name_problem(std::string_view name) noexcept {
    if (name.empty()) {
        return "is empty";
    }
    for (const char character : name) {
        if (character == '/' || character == '\\') {
            return "holds a path separator";
        }
        if (is_control_character(character)) {
            return "holds a control character";
        }
        if (RESERVED_CHARACTERS.find(character) != std::string_view::npos) {
            return "holds a character Windows reserves (one of : * ? \" < > |)";
        }
    }
    if (name.back() == '.' || name.back() == ' ') {
        return "ends with a dot or a space, which Windows drops";
    }
    if (is_device_name(name)) {
        return "is a Windows device name (CON, PRN, AUX, NUL, COM1 to COM9, LPT1 to LPT9)";
    }
    return {};
}

result<void> internal::validate_new_path(std::string_view normalizedPath, std::string_view displayPath) {
    usize start = 0;
    while (start < normalizedPath.size()) {
        usize end = normalizedPath.find('/', start);
        if (end == std::string_view::npos) {
            end = normalizedPath.size();
        }

        const std::string_view segment = normalizedPath.substr(start, end - start);
        const std::string_view problem = segment.empty() ? std::string_view() : portable_name_problem(segment);
        if (!problem.empty()) {
            return make_error(
              error_code::INVALID_ARGUMENT,
              std::format("'{}' cannot be created: the name '{}' {}", displayPath, segment, problem)
            );
        }
        start = end + 1;
    }
    return result<void>::success();
}

std::vector<std::string_view> internal::split_segments(std::string_view normalizedPath) {
    std::vector<std::string_view> segments = {};
    usize start = 0;
    while (start < normalizedPath.size()) {
        usize end = normalizedPath.find('/', start);
        if (end == std::string_view::npos) {
            end = normalizedPath.size();
        }
        if (end > start) {
            segments.push_back(normalizedPath.substr(start, end - start));
        }
        start = end + 1;
    }
    return segments;
}

bool internal::is_same_or_below(std::string_view path, std::string_view ancestor) noexcept {
    if (ancestor.empty()) {
        return true;
    }
    if (path == ancestor) {
        return true;
    }
    return path.size() > ancestor.size() && path.starts_with(ancestor) && path[ancestor.size()] == '/';
}

bool internal::passes_kind_filter(enumerate_flags flags, bool isDirectory) noexcept {
    const bool wantsFiles = has_flag(flags, enumerate_flags::FILES);
    const bool wantsDirectories = has_flag(flags, enumerate_flags::DIRECTORIES);
    if (!wantsFiles && !wantsDirectories) {
        return true;
    }
    return isDirectory ? wantsDirectories : wantsFiles;
}

error internal::not_found(std::string_view path) {
    return make_error(error_code::NOT_FOUND, std::format("'{}' does not exist", path));
}

error internal::io_error(std::string_view operation, std::string_view path, const std::error_code &code) {
    return make_error(error_code::IO_FAILURE, std::format("{} '{}': {}", operation, path, code.message()));
}

std::filesystem::path internal::path_from_utf8(std::string_view utf8) {
    // Element-wise conversion rather than a reinterpret_cast: char8_t may not alias char.
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

std::string internal::path_to_utf8(const std::filesystem::path &path) {
    const std::u8string generic = path.generic_u8string();
    return std::string(reinterpret_cast<const char *>(generic.data()), generic.size());
}

u64 internal::file_time_to_nanoseconds(std::filesystem::file_time_type time) noexcept {
#if CONJURE_STDLIB_LIBCXX
    // libc++ gained clock_cast late, but its file clock counts from system_clock's Unix epoch, so to_sys
    // converts without reading either clock: the result is exact and the same on every call. Relating the
    // clocks through two now() calls instead gave an unchanged file a different write time on every stat.
    using file_clock = std::filesystem::file_time_type::clock;
    const auto systemTime = file_clock::to_sys(time);
#else
    const auto systemTime = std::chrono::clock_cast<std::chrono::system_clock>(time);
#endif
    const auto sinceEpoch = systemTime.time_since_epoch();
    const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(sinceEpoch).count();
    if (nanoseconds < 0) {
        return 0;
    }
    return static_cast<u64>(nanoseconds);
}
