// Windows backend of the module system's long-path helpers (long_path.h).

#include "modules/long_path.h"

#include <format>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

using namespace conjure;

namespace {
    // MAX_PATH of the Windows headers, which this file does not need to include.
    constexpr usize CLASSIC_PATH_LIMIT = 260;

    constexpr std::wstring_view EXTENDED_PREFIX = L"\\\\?\\";
    constexpr std::wstring_view EXTENDED_UNC_PREFIX = L"\\\\?\\UNC\\";
    constexpr std::wstring_view DEVICE_PREFIX = L"\\\\.\\";
    constexpr std::wstring_view NT_PREFIX = L"\\??\\";
    constexpr std::wstring_view UNC_PREFIX = L"\\\\";

    // The forms Windows takes literally, without completing or normalising them. Only the backslash
    // spellings: "//?/" is an ordinary device path that Win32 normalises like any other (absolute()
    // turns it into the backslash form, which absolute_or_literal() then recognises).
    usize literal_prefix_length(std::wstring_view text) noexcept {
        if (text.starts_with(EXTENDED_PREFIX)) {
            return EXTENDED_PREFIX.size();
        }
        if (text.starts_with(DEVICE_PREFIX)) {
            return DEVICE_PREFIX.size();
        }
        if (text.starts_with(NT_PREFIX)) {
            return NT_PREFIX.size();
        }
        return 0;
    }

    // Normalises the part after the prefix on its own. There "C:" (or "UNC") is the first element and a
    // root of its own, so a ".." cannot remove it; behind the standard library's root name "\\?" it would
    // be an ordinary element that a ".." folds away.
    std::filesystem::path normalise_literal(std::wstring_view text, usize prefixLength) {
        const std::filesystem::path rest = std::filesystem::path(text.substr(prefixLength)).lexically_normal();
        std::wstring normalised(text.substr(0, prefixLength));
        normalised.append(rest.native());
        return std::filesystem::path(std::move(normalised)).make_preferred();
    }

    // The absolute, normalised classic form; empty when the path cannot be made absolute.
    std::filesystem::path absolute_classic(const std::filesystem::path &path) {
        std::error_code failure = {};
        const std::filesystem::path absolute = std::filesystem::absolute(path, failure);
        if (failure) {
            return {};
        }
        return absolute.lexically_normal().make_preferred();
    }

    // Only a classic "\\server\share\..." start takes the UNC form; a literal form never reaches here.
    std::filesystem::path to_extended(const std::filesystem::path &classic) {
        const std::wstring &text = classic.native();
        std::wstring extended = {};
        if (text.starts_with(UNC_PREFIX)) {
            extended.append(EXTENDED_UNC_PREFIX);
            extended.append(text.substr(UNC_PREFIX.size()));
        } else {
            extended.append(EXTENDED_PREFIX);
            extended.append(text);
        }
        return std::filesystem::path(std::move(extended));
    }

    // A path that is, or that absolute() completes to, a literal form is used as it is after normalising:
    // prefixing it again would make "\\?\UNC\?\C:\..." of "//?/C:/...".
    struct absolute_form {
        std::filesystem::path path = {};
        bool literal = false;
    };

    absolute_form absolute_or_literal(const std::filesystem::path &path) {
        const usize givenPrefixLength = literal_prefix_length(path.native());
        if (givenPrefixLength != 0) {
            return {normalise_literal(path.native(), givenPrefixLength), true};
        }
        std::filesystem::path classic = absolute_classic(path);
        const bool literal = literal_prefix_length(classic.native()) != 0;
        return {std::move(classic), literal};
    }
}

std::filesystem::path conjure::modules::detail::extend_path(const std::filesystem::path &path) {
    if (path.empty()) {
        return path;
    }
    const absolute_form form = absolute_or_literal(path);
    if (form.literal) {
        return form.path;
    }
    if (form.path.empty() || form.path.native().size() < EXTENDED_PATH_THRESHOLD) {
        return path;
    }
    return to_extended(form.path);
}

std::filesystem::path conjure::modules::detail::always_extended_path(const std::filesystem::path &path) {
    if (path.empty()) {
        return path;
    }
    const absolute_form form = absolute_or_literal(path);
    if (form.literal) {
        return form.path;
    }
    if (form.path.empty()) {
        return path;
    }
    return to_extended(form.path);
}

std::string conjure::modules::detail::path_length_note(const std::filesystem::path &path) {
    if (path.empty()) {
        return {};
    }
    const absolute_form form = absolute_or_literal(path);
    const usize length = form.path.native().size();
    if (form.literal || form.path.empty() || length < EXTENDED_PATH_THRESHOLD) {
        return {};
    }

    // Facts only: the extended-length form opens a path of any length up to about 32,767 characters,
    // and core::dynamic_library still searches the module's own folder for its dependencies (the
    // standard library calls that form absolute), so the length is not named as the cause.
    return std::format(
      "; the path has {} characters and was opened in Windows' extended-length form, which the module "
      "system uses from {} characters on (MAX_PATH is {})",
      length,
      EXTENDED_PATH_THRESHOLD,
      CLASSIC_PATH_LIMIT
    );
}
