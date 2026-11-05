#pragma once

#include <type_traits>

// Bitwise operators and helpers for enumerations used as flag sets.
//
//   enum class window_flags : u32 { NONE = 0, RESIZABLE = 1 << 0, DECORATED = 1 << 1 };
//   CONJURE_ENUM_FLAGS(window_flags);
//
//   window_flags flags = window_flags::RESIZABLE | window_flags::DECORATED;
//   if (has_flag(flags, window_flags::RESIZABLE)) { ... }

#define CONJURE_ENUM_FLAGS(enum_type)                                                                       \
    [[nodiscard]] constexpr enum_type operator|(enum_type left, enum_type right) noexcept {                 \
        using underlying = std::underlying_type_t<enum_type>;                                               \
        return static_cast<enum_type>(static_cast<underlying>(left) | static_cast<underlying>(right));     \
    }                                                                                                       \
    [[nodiscard]] constexpr enum_type operator&(enum_type left, enum_type right) noexcept {                 \
        using underlying = std::underlying_type_t<enum_type>;                                               \
        return static_cast<enum_type>(static_cast<underlying>(left) & static_cast<underlying>(right));     \
    }                                                                                                       \
    [[nodiscard]] constexpr enum_type operator^(enum_type left, enum_type right) noexcept {                 \
        using underlying = std::underlying_type_t<enum_type>;                                               \
        return static_cast<enum_type>(static_cast<underlying>(left) ^ static_cast<underlying>(right));     \
    }                                                                                                       \
    [[nodiscard]] constexpr enum_type operator~(enum_type value) noexcept {                                 \
        using underlying = std::underlying_type_t<enum_type>;                                               \
        return static_cast<enum_type>(~static_cast<underlying>(value));                                     \
    }                                                                                                       \
    constexpr enum_type &operator|=(enum_type &left, enum_type right) noexcept {                            \
        left = left | right;                                                                                \
        return left;                                                                                        \
    }                                                                                                       \
    constexpr enum_type &operator&=(enum_type &left, enum_type right) noexcept {                            \
        left = left & right;                                                                                \
        return left;                                                                                        \
    }                                                                                                       \
    constexpr enum_type &operator^=(enum_type &left, enum_type right) noexcept {                            \
        left = left ^ right;                                                                                \
        return left;                                                                                        \
    }                                                                                                       \
    static_assert(std::is_enum_v<enum_type>, "CONJURE_ENUM_FLAGS requires an enumeration type")

namespace conjure {

    /// <summary>
    /// True when every bit of 'flag' is set in 'value'.
    /// </summary>
    template <typename EnumType>
        requires std::is_enum_v<EnumType>
    [[nodiscard]] constexpr bool has_flag(EnumType value, EnumType flag) noexcept {
        using underlying = std::underlying_type_t<EnumType>;
        const underlying flagBits = static_cast<underlying>(flag);
        return (static_cast<underlying>(value) & flagBits) == flagBits;
    }

    /// <summary>
    /// True when at least one bit of 'flags' is set in 'value'.
    /// </summary>
    template <typename EnumType>
        requires std::is_enum_v<EnumType>
    [[nodiscard]] constexpr bool has_any_flag(EnumType value, EnumType flags) noexcept {
        using underlying = std::underlying_type_t<EnumType>;
        return (static_cast<underlying>(value) & static_cast<underlying>(flags)) != underlying{0};
    }

    /// <summary>
    /// Returns 'value' with the bits of 'flag' set.
    /// </summary>
    template <typename EnumType>
        requires std::is_enum_v<EnumType>
    [[nodiscard]] constexpr EnumType with_flag(EnumType value, EnumType flag) noexcept {
        using underlying = std::underlying_type_t<EnumType>;
        return static_cast<EnumType>(static_cast<underlying>(value) | static_cast<underlying>(flag));
    }

    /// <summary>
    /// Returns 'value' with the bits of 'flag' cleared.
    /// </summary>
    template <typename EnumType>
        requires std::is_enum_v<EnumType>
    [[nodiscard]] constexpr EnumType without_flag(EnumType value, EnumType flag) noexcept {
        using underlying = std::underlying_type_t<EnumType>;
        return static_cast<EnumType>(static_cast<underlying>(value) & ~static_cast<underlying>(flag));
    }

    /// <summary>
    /// Converts an enumeration value to its underlying integer.
    /// </summary>
    template <typename EnumType>
        requires std::is_enum_v<EnumType>
    [[nodiscard]] constexpr std::underlying_type_t<EnumType> to_underlying(EnumType value) noexcept {
        return static_cast<std::underlying_type_t<EnumType>>(value);
    }
}
