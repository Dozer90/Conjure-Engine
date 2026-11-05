#pragma once

#include "conjure/core/hash.h"
#include "conjure/core/types.h"

#include <compare>
#include <concepts>
#include <format>
#include <functional>
#include <string_view>
#include <type_traits>

// Type identifiers.
//
// A type_id is the 64-bit FNV-1a hash of a type's registered, fully qualified name ("ns.my_struct").
// It is derived from the name and nothing else, so it is identical in every shared library, in the
// C# mirror and across sessions; the C++ type is only a way to find the name. Two customisation
// points carry the name:
//
//   - reflect_traits<T>: specialised by CONJURE_REFLECT_STRUCT / CONJURE_REFLECT_ENUM (reflect.h)
//     and, for the built-in field kinds (bool, f32, std::string, vec3, ...), by field_info.h.
//   - reflect_kind_override<T>: specialised by the ecs and assets modules for their handle types
//     so they serialise as the ENTITY / ASSET_REF kinds; core knows nothing about those types.

namespace conjure::core::reflect {

    /// <summary>
    /// Identity of a reflected type: the FNV-1a 64 hash of its registered name. The null id (0)
    /// means "no type".
    /// </summary>
    struct type_id {
        u64 value = 0;

        /// <summary>
        /// The id a type registered under 'name' has.
        /// </summary>
        [[nodiscard]] static constexpr type_id from_name(std::string_view name) noexcept {
            return type_id{hash::fnv1a64(name)};
        }

        /// <summary>
        /// The null id. Equivalent to type_id{}.
        /// </summary>
        [[nodiscard]] static constexpr type_id null() noexcept {
            return type_id{};
        }

        /// <summary>
        /// True for the null id.
        /// </summary>
        [[nodiscard]] constexpr bool is_null() const noexcept {
            return value == 0;
        }

        [[nodiscard]] constexpr auto operator<=>(const type_id &) const noexcept = default;
        [[nodiscard]] constexpr bool operator==(const type_id &) const noexcept = default;
    };

    static_assert(sizeof(type_id) == sizeof(u64), "type_id must be exactly its hash");
    static_assert(std::is_trivially_copyable_v<type_id>, "type_id must be trivially copyable");

    /// <summary>
    /// Compile-time description of a reflected type. Specialisations provide at least
    /// `static constexpr std::string_view NAME` and `static constexpr field_kind KIND`; structs and
    /// enums described with the reflect.h macros also provide `describe()`.
    /// </summary>
    template <typename T>
    struct reflect_traits;

    /// <summary>
    /// Customisation point for handle types owned by other modules: specialise it with
    /// `static constexpr field_kind KIND` (ENTITY or ASSET_REF) and `static constexpr std::string_view NAME`
    /// and the type becomes a reflectable field of that kind without core knowing the type.
    /// </summary>
    template <typename T>
    struct reflect_kind_override;

    /// <summary>
    /// True when reflect_traits is specialised for T with a NAME.
    /// </summary>
    template <typename T>
    concept has_reflect_name = requires {
        { reflect_traits<std::remove_cvref_t<T>>::NAME } -> std::convertible_to<std::string_view>;
    };

    /// <summary>
    /// True when reflect_kind_override is specialised for T with a NAME.
    /// </summary>
    template <typename T>
    concept has_kind_override = requires {
        { reflect_kind_override<std::remove_cvref_t<T>>::NAME } -> std::convertible_to<std::string_view>;
    };

    /// <summary>
    /// True when a registered name is known for T through either customisation point.
    /// </summary>
    template <typename T>
    concept reflect_named = has_reflect_name<T> || has_kind_override<T>;

    /// <summary>
    /// The registered name of T. The kind override wins so a module may rename a type it reflects.
    /// </summary>
    template <reflect_named T>
    [[nodiscard]] constexpr std::string_view reflect_name_of() noexcept {
        if constexpr (has_kind_override<T>) {
            return reflect_kind_override<std::remove_cvref_t<T>>::NAME;
        } else {
            return reflect_traits<std::remove_cvref_t<T>>::NAME;
        }
    }

    /// <summary>
    /// The type_id of T, computed at compile time from its registered name.
    /// </summary>
    template <reflect_named T>
    [[nodiscard]] constexpr type_id type_id_of() noexcept {
        return type_id::from_name(reflect_name_of<T>());
    }
}

template <>
struct std::hash<conjure::core::reflect::type_id> {
    [[nodiscard]] std::size_t operator()(const conjure::core::reflect::type_id &id) const noexcept {
        // u64 and size_t are the same width on every supported (64-bit only) target.
        return id.value;
    }
};

template <>
struct std::formatter<conjure::core::reflect::type_id> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(const conjure::core::reflect::type_id &id, FormatContext &context) const {
        // "#" + 16 hex digits, the same spelling string_id uses for an unresolved hash.
        char text[17] = {};
        text[0] = '#';
        constexpr char DIGITS[] = "0123456789abcdef";
        for (int index = 0; index < 16; ++index) {
            const int shift = (15 - index) * 4;
            text[index + 1] = DIGITS[(id.value >> shift) & 0xFu];
        }
        return std::formatter<std::string_view>::format(std::string_view(text, 17), context);
    }
};
