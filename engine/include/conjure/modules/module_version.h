#pragma once

#include "conjure/core/api.h"
#include "conjure/core/build_info.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <compare>
#include <format>
#include <string>
#include <string_view>

// Semantic versions and version ranges of the module system (docs/design/07_modules.md 2 and 2.1).
//
//   semantic_version v = parse_version("1.2.3").value();
//   version_range r = parse_version_range("^1.2.0").value();      // [1.2.0, 2.0.0)
//   r.contains(v);                                                // true
//
// Range grammar: "1.2.3" exact; "^1.2.3" = [1.2.3, 2.0.0) (for 0.x: [0.2.3, 0.3.0); for 0.0.x:
// [0.0.3, 0.0.4)); "~1.2.3" = [1.2.3, 1.3.0); ">=1.0.0 <2.0.0"; ">=1.0.0"; "<2.0.0"; "*". No
// pre-release tags. Both parsers fail with core::error_code::PARSE_FAILURE.

namespace conjure::modules {

    /// <summary>
    /// major.minor.patch, each at most 65535. Field names avoid `major`/`minor`, which glibc
    /// defines as macros.
    /// </summary>
    struct semantic_version {
        u16 majorVersion = 0;
        u16 minorVersion = 0;
        u16 patchVersion = 0;

        [[nodiscard]] constexpr auto operator<=>(const semantic_version &) const noexcept = default;
    };

    /// <summary>
    /// Largest representable version; the open upper bound of unbounded ranges.
    /// </summary>
    inline constexpr semantic_version MAX_SEMANTIC_VERSION = {0xFFFF, 0xFFFF, 0xFFFF};

    /// <summary>
    /// Half-open range [minInclusive, maxExclusive). The default is "*": every version.
    /// </summary>
    struct version_range {
        semantic_version minInclusive = {};
        semantic_version maxExclusive = MAX_SEMANTIC_VERSION;

        /// <summary>
        /// True when minInclusive &lt;= version &lt; maxExclusive.
        /// </summary>
        [[nodiscard]] constexpr bool contains(semantic_version version) const noexcept {
            return version >= minInclusive && version < maxExclusive;
        }

        /// <summary>
        /// True when the range accepts every version.
        /// </summary>
        [[nodiscard]] constexpr bool is_unbounded() const noexcept {
            return minInclusive == semantic_version{} && maxExclusive == MAX_SEMANTIC_VERSION;
        }

        [[nodiscard]] constexpr auto operator<=>(const version_range &) const noexcept = default;
    };

    /// <summary>
    /// The version of the engine this code was compiled from (build_info.h).
    /// </summary>
    [[nodiscard]] constexpr semantic_version host_engine_version() noexcept {
        return semantic_version{
            static_cast<u16>(CONJURE_VERSION_MAJOR),
            static_cast<u16>(CONJURE_VERSION_MINOR),
            static_cast<u16>(CONJURE_VERSION_PATCH),
        };
    }

    /// <summary>
    /// Parses "major.minor.patch" (all three required, decimal, no sign, no tags, each at most
    /// 65535). PARSE_FAILURE otherwise.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<semantic_version> parse_version(std::string_view text);

    /// <summary>
    /// Parses the range grammar described in the header comment. A reversed or empty range
    /// (">=2.0.0 <1.0.0") is PARSE_FAILURE.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<version_range> parse_version_range(std::string_view text);

    /// <summary>
    /// "major.minor.patch".
    /// </summary>
    [[nodiscard]] CONJURE_API std::string to_string(semantic_version version);

    /// <summary>
    /// Canonical text that parse_version_range() turns back into the same range: "*" for an
    /// unbounded range, "1.2.3" for a single version, ">=a.b.c" for an open range and
    /// ">=a.b.c <d.e.f" otherwise (caret and tilde forms are not preserved).
    /// </summary>
    [[nodiscard]] CONJURE_API std::string to_string(const version_range &range);
}

template <>
struct std::formatter<conjure::modules::semantic_version> : std::formatter<std::string> {
    template <typename FormatContext>
    auto format(const conjure::modules::semantic_version &value, FormatContext &context) const {
        return std::formatter<std::string>::format(conjure::modules::to_string(value), context);
    }
};

template <>
struct std::formatter<conjure::modules::version_range> : std::formatter<std::string> {
    template <typename FormatContext>
    auto format(const conjure::modules::version_range &value, FormatContext &context) const {
        return std::formatter<std::string>::format(conjure::modules::to_string(value), context);
    }
};
