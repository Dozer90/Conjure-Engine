#include "conjure/modules/module_version.h"

#include "conjure/core/error.h"
#include "conjure/core/string_utils.h"

#include <charconv>
#include <format>
#include <optional>
#include <string>
#include <string_view>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;

namespace {

    /*=======================================================================*/
    /*=======================================================================*/
    // Parsing helpers

    std::optional<u16> parse_component(std::string_view text) noexcept {
        if (text.empty() || text.size() > 5) {
            return std::nullopt;
        }
        for (const char character : text) {
            if (character < '0' || character > '9') {
                return std::nullopt;
            }
        }

        u32 value = 0;
        const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), value);
        if (parsed.ec != std::errc() || parsed.ptr != text.data() + text.size() || value > 0xFFFF) {
            return std::nullopt;
        }
        return static_cast<u16>(value);
    }

    std::optional<semantic_version> parse_version_text(std::string_view text) noexcept {
        const usize firstDot = text.find('.');
        if (firstDot == std::string_view::npos) {
            return std::nullopt;
        }
        const usize secondDot = text.find('.', firstDot + 1);
        if (secondDot == std::string_view::npos || text.find('.', secondDot + 1) != std::string_view::npos) {
            return std::nullopt;
        }

        const std::optional<u16> majorVersion = parse_component(text.substr(0, firstDot));
        const std::optional<u16> minorVersion = parse_component(text.substr(firstDot + 1, secondDot - firstDot - 1));
        const std::optional<u16> patchVersion = parse_component(text.substr(secondDot + 1));
        if (!majorVersion.has_value() || !minorVersion.has_value() || !patchVersion.has_value()) {
            return std::nullopt;
        }
        return semantic_version{*majorVersion, *minorVersion, *patchVersion};
    }

    error range_error(std::string_view text, std::string_view reason) {
        return make_error(error_code::PARSE_FAILURE, "invalid version range '{}': {}", text, reason);
    }

    /// <summary>
    /// The exclusive upper bound of "^a.b.c": the next major, or the next minor for 0.x, or the
    /// next patch for 0.0.x.
    /// </summary>
    std::optional<semantic_version> caret_upper_bound(semantic_version lower) noexcept {
        if (lower.majorVersion != 0) {
            if (lower.majorVersion == 0xFFFF) {
                return std::nullopt;
            }
            return semantic_version{static_cast<u16>(lower.majorVersion + 1), 0, 0};
        }
        if (lower.minorVersion != 0) {
            if (lower.minorVersion == 0xFFFF) {
                return std::nullopt;
            }
            return semantic_version{0, static_cast<u16>(lower.minorVersion + 1), 0};
        }
        if (lower.patchVersion == 0xFFFF) {
            return std::nullopt;
        }
        return semantic_version{0, 0, static_cast<u16>(lower.patchVersion + 1)};
    }

    /// <summary>
    /// The exclusive upper bound of "~a.b.c": the next minor.
    /// </summary>
    std::optional<semantic_version> tilde_upper_bound(semantic_version lower) noexcept {
        if (lower.minorVersion == 0xFFFF) {
            return std::nullopt;
        }
        return semantic_version{lower.majorVersion, static_cast<u16>(lower.minorVersion + 1), 0};
    }

    std::optional<semantic_version> next_patch(semantic_version version) noexcept {
        if (version.patchVersion == 0xFFFF) {
            return std::nullopt;
        }
        return semantic_version{version.majorVersion, version.minorVersion, static_cast<u16>(version.patchVersion + 1)};
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Public functions

result<semantic_version> conjure::modules::parse_version(std::string_view text) {
    const std::string_view trimmed = str::trim(text);
    const std::optional<semantic_version> parsed = parse_version_text(trimmed);
    if (!parsed.has_value()) {
        return make_error(error_code::PARSE_FAILURE, "invalid version '{}': expected major.minor.patch", text);
    }
    return *parsed;
}

result<version_range> conjure::modules::parse_version_range(std::string_view text) {
    const std::string_view trimmed = str::trim(text);
    if (trimmed.empty()) {
        return range_error(text, "empty");
    }
    if (trimmed == "*") {
        return version_range{};
    }

    // Single-token forms: exact, caret and tilde.
    if (trimmed.front() == '^' || trimmed.front() == '~') {
        const std::optional<semantic_version> lower = parse_version_text(str::trim(trimmed.substr(1)));
        if (!lower.has_value()) {
            return range_error(text, "expected a version after the operator");
        }
        const std::optional<semantic_version> upper =
          trimmed.front() == '^' ? caret_upper_bound(*lower) : tilde_upper_bound(*lower);
        if (!upper.has_value()) {
            return range_error(text, "the upper bound is not representable");
        }
        return version_range{*lower, *upper};
    }
    if (const std::optional<semantic_version> exact = parse_version_text(trimmed)) {
        const std::optional<semantic_version> upper = next_patch(*exact);
        if (!upper.has_value()) {
            return range_error(text, "the upper bound is not representable");
        }
        return version_range{*exact, *upper};
    }

    // Comparator forms: ">=a.b.c", "<d.e.f" or both, in either order, each at most once.
    version_range range = {};
    bool sawLower = false;
    bool sawUpper = false;
    for (const std::string_view token : str::split(trimmed, ' ')) {
        if (token.empty()) {
            continue;
        }
        if (token.starts_with(">=")) {
            const std::optional<semantic_version> lower = parse_version_text(token.substr(2));
            if (!lower.has_value() || sawLower) {
                return range_error(text, "expected one '>=major.minor.patch'");
            }
            range.minInclusive = *lower;
            sawLower = true;
            continue;
        }
        if (token.starts_with('<') && !token.starts_with("<=")) {
            const std::optional<semantic_version> upper = parse_version_text(token.substr(1));
            if (!upper.has_value() || sawUpper) {
                return range_error(text, "expected one '<major.minor.patch'");
            }
            range.maxExclusive = *upper;
            sawUpper = true;
            continue;
        }
        return range_error(text, std::format("unexpected token '{}'", token));
    }
    if (!sawLower && !sawUpper) {
        return range_error(text, "expected a version, '^', '~', '>=', '<' or '*'");
    }
    if (range.minInclusive >= range.maxExclusive) {
        return range_error(text, "the range is empty (lower bound is not below the upper bound)");
    }
    return range;
}

std::string conjure::modules::to_string(semantic_version version) {
    return std::format("{}.{}.{}", version.majorVersion, version.minorVersion, version.patchVersion);
}

std::string conjure::modules::to_string(const version_range &range) {
    if (range.is_unbounded()) {
        return "*";
    }
    if (range.maxExclusive == MAX_SEMANTIC_VERSION) {
        return std::format(">={}", to_string(range.minInclusive));
    }
    if (next_patch(range.minInclusive) == range.maxExclusive) {
        return to_string(range.minInclusive);
    }
    return std::format(">={} <{}", to_string(range.minInclusive), to_string(range.maxExclusive));
}
