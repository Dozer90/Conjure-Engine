#pragma once

#include "conjure/core/api.h"
#include "conjure/core/types.h"

#include <compare>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace conjure::core {

    /// <summary>
    /// 128-bit universally unique identifier (RFC 4122 version 4 when generated). Used for
    /// assets, prefabs and anything that must stay stable across sessions and machines.
    /// </summary>
    class CONJURE_API uuid {
        private:
            u64 mHigh = 0;
            u64 mLow = 0;

        public:
            constexpr uuid() noexcept = default;

            /// <summary>
            /// Builds an identifier from its two 64-bit halves (most significant first).
            /// </summary>
            constexpr uuid(u64 high, u64 low) noexcept :
                mHigh(high),
                mLow(low) {}

        public:
            /// <summary>
            /// The all-zero identifier. Means "none".
            /// </summary>
            [[nodiscard]] static constexpr uuid null() noexcept {
                return uuid();
            }

            /// <summary>
            /// Generates a random version-4 identifier. Thread-safe. The per-thread generator is
            /// seeded from std::random_device on first use; a host without an entropy source
            /// terminates there, which is acceptable for every supported desktop platform.
            /// </summary>
            [[nodiscard]] static uuid generate();

            /// <summary>
            /// Parses "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" (case-insensitive, braces optional).
            /// Returns nullopt for anything else.
            /// </summary>
            [[nodiscard]] static std::optional<uuid> parse(std::string_view text) noexcept;

            /// <summary>
            /// True for the all-zero identifier.
            /// </summary>
            [[nodiscard]] constexpr bool is_null() const noexcept {
                return mHigh == 0 && mLow == 0;
            }

            /// <summary>
            /// Most significant 64 bits.
            /// </summary>
            [[nodiscard]] constexpr u64 high() const noexcept {
                return mHigh;
            }

            /// <summary>
            /// Least significant 64 bits.
            /// </summary>
            [[nodiscard]] constexpr u64 low() const noexcept {
                return mLow;
            }

            /// <summary>
            /// Lower-case canonical form "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx".
            /// </summary>
            [[nodiscard]] std::string to_string() const;

            /// <summary>
            /// 64-bit hash suitable for hash maps.
            /// </summary>
            [[nodiscard]] constexpr u64 hash() const noexcept {
                // Mix the two halves; the generated bits are already uniformly random.
                return mHigh ^ (mLow * 0x9E3779B97F4A7C15ull);
            }

            [[nodiscard]] constexpr auto operator<=>(const uuid &) const noexcept = default;
            [[nodiscard]] constexpr bool operator==(const uuid &) const noexcept = default;
    };
}

template <>
struct std::hash<conjure::core::uuid> {
    [[nodiscard]] std::size_t operator()(const conjure::core::uuid &value) const noexcept {
        // u64 and size_t are the same width on every supported (64-bit only) target.
        return value.hash();
    }
};

template <>
struct std::formatter<conjure::core::uuid> : std::formatter<std::string> {
    template <typename FormatContext>
    auto format(const conjure::core::uuid &value, FormatContext &context) const {
        return std::formatter<std::string>::format(value.to_string(), context);
    }
};
