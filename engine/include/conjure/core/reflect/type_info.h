#pragma once

#include "conjure/core/reflect/field_info.h"
#include "conjure/core/reflect/type_id.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <concepts>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// Struct descriptions.
//
// A type_info is a plain, self-contained description of a struct layout: name, size, alignment and
// the fields in declaration order. Macro-described types produce one through reflect_traits<T>::describe();
// dynamic types (C# components) build one at runtime with build_type_info(), which validates the
// layout. Registered descriptions live in a type_registry (type_registry.h) and are addressed by
// pointer; nested STRUCT fields and ENUM fields point at the registered description of their type.

namespace conjure::core::reflect {

    /// <summary>
    /// Layout and fields of a reflected struct.
    /// </summary>
    struct type_info {
        std::string name = {};
        type_id type = {};
        u32 size = 0;
        u32 alignment = 0;
        std::vector<field_info> fields = {};
        bool isTriviallyCopyable = true;

        /// <summary>
        /// The field named 'fieldName', or nullptr. Exact match on the declared member name.
        /// </summary>
        [[nodiscard]] const field_info *find_field(std::string_view fieldName) const noexcept;

        /// <summary>
        /// FNV-1a 64 over (name, kind, offset, size) of every field in order: the UTF-8 bytes of
        /// the name, the kind as one byte, then offset and size as little-endian u32. Starts from
        /// the FNV offset basis and ignores the type name, so two types with the same fields hash
        /// alike. Mirrored by the C# side.
        /// </summary>
        [[nodiscard]] u64 layout_hash() const noexcept;

        /// <summary>
        /// True when size, alignment and layout_hash() all match.
        /// </summary>
        [[nodiscard]] bool has_same_layout(const type_info &other) const noexcept;
    };

    /// <summary>
    /// Checks a description for internal consistency: non-empty names usable in field paths, every
    /// field inside the struct, no overlapping fields, natural alignment of every field (and no
    /// field stricter than the type's own alignment), a size that matches the kind (fixed kinds,
    /// enums, nested structs and arrays), struct/enum pointers present where the kind needs them,
    /// field ids matching the pointed-to description, and enum widths of 1, 2, 4 or 8 bytes. Fails
    /// with INVALID_ARGUMENT naming the offending field.
    /// </summary>
    [[nodiscard]] result<void> validate_layout(const type_info &info);

    /// <summary>
    /// Builds and validates a description at runtime (dynamic C# components). The type id is
    /// derived from the name, isTriviallyCopyable from the fields. Fails with INVALID_ARGUMENT when
    /// the layout is inconsistent (see validate_layout).
    /// </summary>
    [[nodiscard]] result<type_info> build_type_info(
      std::string name,
      u32 size,
      u32 alignment,
      std::span<const field_info> fields
    );

    /// <summary>
    /// True for structs described with CONJURE_REFLECT_STRUCT.
    /// </summary>
    template <typename T>
    concept reflected_struct = has_reflect_kind<T> && requires {
        requires reflect_traits<std::remove_cvref_t<T>>::KIND == field_kind::STRUCT;
        { reflect_traits<std::remove_cvref_t<T>>::describe() } -> std::same_as<type_info>;
    };

    /// <summary>
    /// True for enumerations described with CONJURE_REFLECT_ENUM / CONJURE_REFLECT_FLAGS.
    /// </summary>
    template <typename T>
    concept reflected_enum = has_reflect_kind<T> && requires {
        requires reflect_traits<std::remove_cvref_t<T>>::KIND == field_kind::ENUM;
        { reflect_traits<std::remove_cvref_t<T>>::describe() } -> std::same_as<enum_info>;
    };

    /// <summary>
    /// A fresh description of a macro-reflected struct, without registering it. Nested struct and
    /// enum fields still point at the registered descriptions of their types.
    /// </summary>
    template <reflected_struct T>
    [[nodiscard]] type_info describe_struct() {
        return reflect_traits<std::remove_cvref_t<T>>::describe();
    }

    /// <summary>
    /// A fresh description of a macro-reflected enumeration, without registering it.
    /// </summary>
    template <reflected_enum T>
    [[nodiscard]] enum_info describe_enum() {
        return reflect_traits<std::remove_cvref_t<T>>::describe();
    }
}
