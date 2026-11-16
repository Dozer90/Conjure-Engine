#include "conjure/core/vfs/path_utils.h"

#include "conjure/core/types.h"
#include "core/vfs/vfs_internal.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::core::vfs;

/*===========================================================================*/
/*===========================================================================*/
// Helpers

namespace {
    constexpr std::string_view SEPARATORS = "/\\";

    bool is_separator(char character) noexcept {
        return character == '/' || character == '\\';
    }

    bool is_ascii_alpha(char character) noexcept {
        return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
    }

    char to_lower_ascii(char character) noexcept {
        if (character >= 'A' && character <= 'Z') {
            return static_cast<char>(character - 'A' + 'a');
        }
        return character;
    }

    bool has_drive_prefix(std::string_view path) noexcept {
        return path.size() >= 2 && is_ascii_alpha(path[0]) && path[1] == ':';
    }

    bool has_unc_prefix(std::string_view path) noexcept {
        return path.size() >= 2 && is_separator(path[0]) && is_separator(path[1]);
    }

    // The root prefix of a path: "" (relative), "/" (absolute), "C:/" (drive) or "//" (UNC).
    // The returned prefix is already spelt with forward slashes.
    struct root_split {
        std::string prefix = {};
        std::string_view remainder = {};
    };

    root_split split_root(std::string_view path) {
        root_split split = {};
        if (has_unc_prefix(path)) {
            split.prefix = "//";
            split.remainder = path.substr(2);
            return split;
        }
        if (has_drive_prefix(path)) {
            split.prefix = std::string(path.substr(0, 2)) + "/";
            split.remainder = path.substr(2);
            return split;
        }
        if (!path.empty() && is_separator(path[0])) {
            split.prefix = "/";
            split.remainder = path.substr(1);
            return split;
        }
        split.remainder = path;
        return split;
    }

    std::vector<std::string_view> split_segments(std::string_view path) {
        std::vector<std::string_view> segments = {};
        usize start = 0;
        while (start <= path.size()) {
            usize end = path.find_first_of(SEPARATORS, start);
            if (end == std::string_view::npos) {
                end = path.size();
            }
            if (end > start) {
                segments.push_back(path.substr(start, end - start));
            }
            start = end + 1;
        }
        return segments;
    }

    std::vector<std::string_view> normalized_segments(std::string_view remainder, bool absolute) {
        std::vector<std::string_view> result = {};
        for (const std::string_view segment : split_segments(remainder)) {
            if (segment == ".") {
                continue;
            }
            if (segment != "..") {
                result.push_back(segment);
                continue;
            }
            if (!result.empty() && result.back() != "..") {
                result.pop_back();
                continue;
            }
            // Nothing left to climb out of: an absolute path stays at its root, a relative path
            // remembers that it points above its starting point.
            if (!absolute) {
                result.push_back(segment);
            }
        }
        return result;
    }

    std::string join_segments(std::string_view prefix, const std::vector<std::string_view> &segments) {
        std::string result(prefix);
        for (usize index = 0; index < segments.size(); ++index) {
            if (index > 0) {
                result.push_back('/');
            }
            result.append(segments[index]);
        }
        return result;
    }

    std::string_view strip_trailing_separators(std::string_view path) noexcept {
        // Keep a lone root separator so parent("/") stays "/", and the separator of a drive root so
        // "C:/" does not turn into the drive-relative "C:".
        while (path.size() > 1 && is_separator(path.back()) && !(path.size() == 3 && has_drive_prefix(path))) {
            path.remove_suffix(1);
        }
        return path;
    }

    bool equals_ignore_case(std::string_view left, std::string_view right) noexcept {
        if (left.size() != right.size()) {
            return false;
        }
        for (usize index = 0; index < left.size(); ++index) {
            if (to_lower_ascii(left[index]) != to_lower_ascii(right[index])) {
                return false;
            }
        }
        return true;
    }

    std::string_view extension_view(std::string_view path) noexcept {
        const std::string_view name = path_utils::filename(path);
        const usize dot = name.rfind('.');
        // A leading dot marks a hidden file, not an extension; "." and ".." have none either.
        if (dot == std::string_view::npos || dot == 0 || name == "..") {
            return {};
        }
        return name.substr(dot);
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Public functions

std::string path_utils::normalize(std::string_view path) {
    const root_split root = split_root(path);
    const bool absolute = !root.prefix.empty();
    const std::vector<std::string_view> segments = normalized_segments(root.remainder, absolute);
    return join_segments(root.prefix, segments);
}

std::string path_utils::join(std::string_view left, std::string_view right) {
    if (left.empty() || is_absolute(right)) {
        return std::string(right);
    }
    if (right.empty()) {
        return std::string(left);
    }

    std::string result(left);
    if (!is_separator(result.back())) {
        result.push_back('/');
    }
    result.append(right);
    return result;
}

std::string_view path_utils::parent(std::string_view path) noexcept {
    const std::string_view trimmed = strip_trailing_separators(path);
    const usize separator = trimmed.find_last_of(SEPARATORS);
    if (separator == std::string_view::npos) {
        return {};
    }
    if (separator == 0) {
        return trimmed.substr(0, 1);
    }
    if (separator == 2 && has_drive_prefix(trimmed)) {
        return trimmed.substr(0, 3);
    }
    return trimmed.substr(0, separator);
}

std::string_view path_utils::filename(std::string_view path) noexcept {
    const usize separator = path.find_last_of(SEPARATORS);
    if (separator == std::string_view::npos) {
        return path;
    }
    return path.substr(separator + 1);
}

std::string_view path_utils::stem(std::string_view path) noexcept {
    const std::string_view name = filename(path);
    const std::string_view extensionText = extension_view(path);
    return name.substr(0, name.size() - extensionText.size());
}

std::string path_utils::extension(std::string_view path) {
    std::string result(extension_view(path));
    std::transform(result.begin(), result.end(), result.begin(), &to_lower_ascii);
    return result;
}

bool path_utils::has_extension(std::string_view path, std::string_view extension) noexcept {
    std::string_view actual = extension_view(path);
    if (!actual.empty()) {
        actual.remove_prefix(1);
    }
    std::string_view expected = extension;
    if (!expected.empty() && expected.front() == '.') {
        expected.remove_prefix(1);
    }
    return !actual.empty() && equals_ignore_case(actual, expected);
}

bool path_utils::is_absolute(std::string_view path) noexcept {
    if (path.empty()) {
        return false;
    }
    return is_separator(path[0]) || has_drive_prefix(path);
}

std::string path_utils::relative_to(std::string_view path, std::string_view base) {
    const std::string normalizedPath = normalize(path);
    const std::string normalizedBase = normalize(base);

    const root_split pathRoot = split_root(normalizedPath);
    const root_split baseRoot = split_root(normalizedBase);
    if (!equals_ignore_case(pathRoot.prefix, baseRoot.prefix)) {
        return normalizedPath;
    }

    const std::vector<std::string_view> pathSegments = split_segments(pathRoot.remainder);
    const std::vector<std::string_view> baseSegments = split_segments(baseRoot.remainder);

    usize common = 0;
    while (common < pathSegments.size() && common < baseSegments.size()
           && pathSegments[common] == baseSegments[common]) {
        ++common;
    }

    // A relative base that still points above its origin cannot be climbed out of lexically.
    for (usize index = common; index < baseSegments.size(); ++index) {
        if (baseSegments[index] == "..") {
            return normalizedPath;
        }
    }

    std::vector<std::string_view> relative = {};
    for (usize index = common; index < baseSegments.size(); ++index) {
        relative.push_back("..");
    }
    for (usize index = common; index < pathSegments.size(); ++index) {
        relative.push_back(pathSegments[index]);
    }
    return join_segments({}, relative);
}

bool path_utils::is_portable_name(std::string_view name) noexcept {
    return internal::portable_name_problem(name).empty();
}
