#pragma once

#include "conjure/core/reflect/type_id.h"

#include "conjure/core/api.h"
#include "conjure/core/math/glm_types.h"
#include "conjure/core/math/transform.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"

#include <array>
#include <concepts>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// Field descriptions.
//
// Every reflected member is classified into a field_kind, a closed set the editor, the serialiser
// and the C# mirror all understand. The kind of a C++ type is derived at compile time by kind_of<T>():
// the fixed kinds through the reflect_traits specialisations below, nested structs and enums through
// the CONJURE_REFLECT_* macros, std::array<T, N> as ARRAY, and foreign handle types through
// reflect_kind_override<T>.

namespace conjure::core::reflect {

    struct type_info;

    /// <summary>
    /// What a field holds. ARRAY fields describe their element in field_info::elementKind.
    /// </summary>
    enum class field_kind : u8 {
        BOOL,
        I8,
        I16,
        I32,
        I64,
        U8,
        U16,
        U32,
        U64,
        F32,
        F64,
        STRING,         // std::string
        STRING_ID,      // core::string_id
        UUID,           // core::uuid
        VEC2,
        VEC3,
        VEC4,
        QUAT,           // stored x, y, z, w
        MAT4,           // column-major, 16 floats
        TRANSFORM,      // math::transform
        ENUM,           // enum described with CONJURE_REFLECT_ENUM (field_info::enumType)
        STRUCT,         // nested reflected struct (field_info::structType)
        ARRAY,          // std::array<T, N> of any other kind
        ENTITY,         // handle owned by the ecs module (reflect_kind_override)
        ASSET_REF,      // handle owned by the assets module (reflect_kind_override)
        COUNT
    };

    /// <summary>
    /// Returns the stable upper-case enumerator name of a kind ("VEC3"); "UNKNOWN" for COUNT and
    /// out-of-range values.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(field_kind kind) noexcept;

    /// <summary>
    /// Parses an enumerator name produced by to_string() (exact match). Returns nullopt otherwise.
    /// </summary>
    [[nodiscard]] CONJURE_API std::optional<field_kind> parse_field_kind(std::string_view name) noexcept;

    /// <summary>
    /// Size in bytes of a fixed-size kind; 0 for ENUM, STRUCT, ARRAY, ENTITY and ASSET_REF whose
    /// size depends on the field.
    /// </summary>
    [[nodiscard]] constexpr u32 kind_size(field_kind kind) noexcept {
        switch (kind) {
            case field_kind::BOOL:
            case field_kind::I8:
            case field_kind::U8:
                return 1;
            case field_kind::I16:
            case field_kind::U16:
                return 2;
            case field_kind::I32:
            case field_kind::U32:
            case field_kind::F32:
                return 4;
            case field_kind::I64:
            case field_kind::U64:
            case field_kind::F64:
            case field_kind::STRING_ID:
                return 8;
            case field_kind::STRING:
                return static_cast<u32>(sizeof(std::string));
            case field_kind::UUID:
                return 16;
            case field_kind::VEC2:
                return 8;
            case field_kind::VEC3:
                return 12;
            case field_kind::VEC4:
            case field_kind::QUAT:
                return 16;
            case field_kind::MAT4:
                return 64;
            case field_kind::TRANSFORM:
                return 40;
            case field_kind::ENUM:
            case field_kind::STRUCT:
            case field_kind::ARRAY:
            case field_kind::ENTITY:
            case field_kind::ASSET_REF:
            case field_kind::COUNT:
                return 0;
        }
        return 0;
    }

    /// <summary>
    /// Natural alignment of a fixed-size kind; 0 for the kinds whose alignment depends on the field.
    /// </summary>
    [[nodiscard]] constexpr u32 kind_alignment(field_kind kind) noexcept {
        switch (kind) {
            case field_kind::BOOL:
            case field_kind::I8:
            case field_kind::U8:
                return 1;
            case field_kind::I16:
            case field_kind::U16:
                return 2;
            case field_kind::I32:
            case field_kind::U32:
            case field_kind::F32:
            case field_kind::VEC2:
            case field_kind::VEC3:
            case field_kind::VEC4:
            case field_kind::QUAT:
            case field_kind::MAT4:
            case field_kind::TRANSFORM:
                return 4;
            case field_kind::I64:
            case field_kind::U64:
            case field_kind::F64:
            case field_kind::STRING_ID:
            case field_kind::UUID:
                return 8;
            case field_kind::STRING:
                return static_cast<u32>(alignof(std::string));
            case field_kind::ENUM:
            case field_kind::STRUCT:
            case field_kind::ARRAY:
            case field_kind::ENTITY:
            case field_kind::ASSET_REF:
            case field_kind::COUNT:
                return 0;
        }
        return 0;
    }

    /// <summary>
    /// True for the kinds whose size and alignment are fixed by the kind alone.
    /// </summary>
    [[nodiscard]] constexpr bool is_fixed_size_kind(field_kind kind) noexcept {
        return kind_size(kind) != 0;
    }

    /// <summary>
    /// True for the integer kinds I8..U64.
    /// </summary>
    [[nodiscard]] constexpr bool is_integer_kind(field_kind kind) noexcept {
        return kind >= field_kind::I8 && kind <= field_kind::U64;
    }

    /// <summary>
    /// True for F32 and F64.
    /// </summary>
    [[nodiscard]] constexpr bool is_float_kind(field_kind kind) noexcept {
        return kind == field_kind::F32 || kind == field_kind::F64;
    }

    /// <summary>
    /// True for the kinds supplied by other modules through reflect_kind_override.
    /// </summary>
    [[nodiscard]] constexpr bool is_custom_kind(field_kind kind) noexcept {
        return kind == field_kind::ENTITY || kind == field_kind::ASSET_REF;
    }

    /// <summary>
    /// True for the storage widths an enumeration may have: 1, 2, 4 or 8 bytes. Every consumer of an
    /// enum_info (registration, layout validation, JSON conversion) rejects other widths.
    /// </summary>
    [[nodiscard]] constexpr bool is_valid_enum_size(u32 underlyingSize) noexcept {
        return underlyingSize == 1 || underlyingSize == 2 || underlyingSize == 4 || underlyingSize == 8;
    }

    /// <summary>
    /// The unit a float field is stored in, as a display hint (field_attributes::unit). The value, its min, max
    /// and step and its JSON form stay in this unit; the editor's inspector shows a RADIANS field in degrees and
    /// converts what is typed back, and marks a DEGREES field as degrees. Only F32 and F64 fields (and arrays
    /// of them) may carry a unit other than NONE (validate_layout).
    /// </summary>
    enum class field_unit : u8 {
        NONE,
        RADIANS,
        DEGREES
    };

    /// <summary>
    /// The lower-case name of a unit, as the editor protocol sends it in a field's attributes: "none", "radians"
    /// or "degrees"; "unknown" for a value outside the enumeration.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(field_unit unit) noexcept;

    /// <summary>
    /// Editor hints attached to a field. Set through the CONJURE_FIELD designated-initialiser list.
    /// </summary>
    struct field_attributes {
        std::optional<f64> min = std::nullopt;
        std::optional<f64> max = std::nullopt;
        f64 step = 0.0;
        std::string tooltip = {};
        bool hidden = false;
        bool readOnly = false;
        std::string category = {};
        field_unit unit = field_unit::NONE;               // stored unit; min, max and step are in it too

        [[nodiscard]] bool operator==(const field_attributes &) const = default;
    };

    /// <summary>
    /// A reflected enumeration: its enumerators by name and value plus the storage width.
    /// </summary>
    struct CONJURE_API enum_info {
        std::string name = {};
        type_id type = {};
        u32 underlyingSize = 4;
        std::vector<std::pair<std::string, i64>> values = {};
        bool isFlags = false;
        bool isSigned = false;      // signedness of the underlying type; u64 values are stored as bit patterns

        /// <summary>
        /// The enumerator value named 'name', or nullopt. Exact, case-sensitive match.
        /// </summary>
        [[nodiscard]] std::optional<i64> find_value(std::string_view enumeratorName) const noexcept;

        /// <summary>
        /// The name of the first enumerator with this exact value, or nullptr.
        /// </summary>
        [[nodiscard]] const std::string *find_name(i64 value) const noexcept;

        /// <summary>
        /// Text form of a value: the enumerator name when one matches exactly; for flags, the
        /// single-bit enumerator names joined with '|' ("" for zero without a zero enumerator).
        /// Returns nullopt when the value cannot be spelled with the known enumerators.
        /// </summary>
        [[nodiscard]] std::optional<std::string> format_value(i64 value) const;

        /// <summary>
        /// Inverse of format_value(): an enumerator name, or for flags names separated by '|'
        /// (whitespace around names ignored, empty pieces ignored, "" is zero). Returns nullopt on
        /// any unknown name.
        /// </summary>
        [[nodiscard]] std::optional<i64> parse_value(std::string_view text) const;

        /// <summary>
        /// True when 'value' fits the underlying type (bit pattern for 64-bit unsigned). Always
        /// false when underlyingSize is not 1, 2, 4 or 8.
        /// </summary>
        [[nodiscard]] bool is_representable(i64 value) const noexcept;

        [[nodiscard]] bool operator==(const enum_info &) const = default;
    };

    /// <summary>
    /// One reflected member of a struct.
    /// </summary>
    struct field_info {
        std::string name = {};
        type_id type = {};                          // id of the field's value type (the element type for ARRAY)
        field_kind kind = field_kind::COUNT;
        u32 offset = 0;
        u32 size = 0;                               // whole field, including every array element
        u32 arrayCount = 0;                         // ARRAY only
        field_kind elementKind = field_kind::COUNT; // ARRAY only
        std::string defaultValueJson = {};          // canonical JSON of the default-constructed value; "" when unknown
        field_attributes attributes = {};
        const type_info *structType = nullptr;      // STRUCT or ARRAY of STRUCT
        const enum_info *enumType = nullptr;        // ENUM or ARRAY of ENUM

        /// <summary>
        /// True for ARRAY fields.
        /// </summary>
        [[nodiscard]] bool is_array() const noexcept {
            return kind == field_kind::ARRAY;
        }

        /// <summary>
        /// The kind of one value: elementKind for arrays, kind otherwise.
        /// </summary>
        [[nodiscard]] field_kind value_kind() const noexcept {
            return is_array() ? elementKind : kind;
        }

        /// <summary>
        /// Size of one value in bytes: size / arrayCount for arrays, size otherwise.
        /// </summary>
        [[nodiscard]] u32 element_size() const noexcept {
            if (is_array()) {
                return arrayCount == 0 ? 0 : size / arrayCount;
            }
            return size;
        }
    };

    /*===========================================================================*/
    /*===========================================================================*/
    // Compile-time kinds

    template <>
    struct reflect_traits<bool> {
        static constexpr std::string_view NAME = "bool";
        static constexpr field_kind KIND = field_kind::BOOL;
    };

    template <>
    struct reflect_traits<i8> {
        static constexpr std::string_view NAME = "i8";
        static constexpr field_kind KIND = field_kind::I8;
    };

    template <>
    struct reflect_traits<i16> {
        static constexpr std::string_view NAME = "i16";
        static constexpr field_kind KIND = field_kind::I16;
    };

    template <>
    struct reflect_traits<i32> {
        static constexpr std::string_view NAME = "i32";
        static constexpr field_kind KIND = field_kind::I32;
    };

    template <>
    struct reflect_traits<i64> {
        static constexpr std::string_view NAME = "i64";
        static constexpr field_kind KIND = field_kind::I64;
    };

    template <>
    struct reflect_traits<u8> {
        static constexpr std::string_view NAME = "u8";
        static constexpr field_kind KIND = field_kind::U8;
    };

    template <>
    struct reflect_traits<u16> {
        static constexpr std::string_view NAME = "u16";
        static constexpr field_kind KIND = field_kind::U16;
    };

    template <>
    struct reflect_traits<u32> {
        static constexpr std::string_view NAME = "u32";
        static constexpr field_kind KIND = field_kind::U32;
    };

    template <>
    struct reflect_traits<u64> {
        static constexpr std::string_view NAME = "u64";
        static constexpr field_kind KIND = field_kind::U64;
    };

    template <>
    struct reflect_traits<f32> {
        static constexpr std::string_view NAME = "f32";
        static constexpr field_kind KIND = field_kind::F32;
    };

    template <>
    struct reflect_traits<f64> {
        static constexpr std::string_view NAME = "f64";
        static constexpr field_kind KIND = field_kind::F64;
    };

    template <>
    struct reflect_traits<std::string> {
        static constexpr std::string_view NAME = "string";
        static constexpr field_kind KIND = field_kind::STRING;
    };

    template <>
    struct reflect_traits<string_id> {
        static constexpr std::string_view NAME = "string_id";
        static constexpr field_kind KIND = field_kind::STRING_ID;
    };

    template <>
    struct reflect_traits<uuid> {
        static constexpr std::string_view NAME = "uuid";
        static constexpr field_kind KIND = field_kind::UUID;
    };

    template <>
    struct reflect_traits<math::vec2> {
        static constexpr std::string_view NAME = "vec2";
        static constexpr field_kind KIND = field_kind::VEC2;
    };

    template <>
    struct reflect_traits<math::vec3> {
        static constexpr std::string_view NAME = "vec3";
        static constexpr field_kind KIND = field_kind::VEC3;
    };

    template <>
    struct reflect_traits<math::vec4> {
        static constexpr std::string_view NAME = "vec4";
        static constexpr field_kind KIND = field_kind::VEC4;
    };

    template <>
    struct reflect_traits<math::quat> {
        static constexpr std::string_view NAME = "quat";
        static constexpr field_kind KIND = field_kind::QUAT;
    };

    template <>
    struct reflect_traits<math::mat4> {
        static constexpr std::string_view NAME = "mat4";
        static constexpr field_kind KIND = field_kind::MAT4;
    };

    template <>
    struct reflect_traits<math::transform> {
        static constexpr std::string_view NAME = "transform";
        static constexpr field_kind KIND = field_kind::TRANSFORM;
    };

    namespace detail {
        /// <summary>
        /// The platform integer types that are distinct from every fixed-width alias on the current
        /// target: `long` is 32 bits on Windows and 64 bits elsewhere, `long long` is i64 on Windows
        /// and macOS but not on Linux, `usize` is `unsigned long` on macOS and Linux. Members of these
        /// types reflect as the fixed-width kind of the same width and signedness so a struct with a
        /// `usize` or `long` member describes identically on every platform.
        /// </summary>
        template <typename T>
        concept distinct_platform_integer =
          (std::is_same_v<T, long> || std::is_same_v<T, unsigned long> || std::is_same_v<T, long long> ||
            std::is_same_v<T, unsigned long long>) &&
          !std::is_same_v<T, i32> && !std::is_same_v<T, u32> && !std::is_same_v<T, i64> && !std::is_same_v<T, u64>;

        template <typename T>
        [[nodiscard]] consteval std::string_view platform_integer_name() noexcept {
            static_assert(sizeof(T) == 4 || sizeof(T) == 8, "platform integers are 32 or 64 bits wide");
            if constexpr (sizeof(T) == 4) {
                return std::is_signed_v<T> ? "i32" : "u32";
            } else {
                return std::is_signed_v<T> ? "i64" : "u64";
            }
        }

        template <typename T>
        [[nodiscard]] consteval field_kind platform_integer_kind() noexcept {
            if constexpr (sizeof(T) == 4) {
                return std::is_signed_v<T> ? field_kind::I32 : field_kind::U32;
            } else {
                return std::is_signed_v<T> ? field_kind::I64 : field_kind::U64;
            }
        }
    }

    template <typename T>
        requires detail::distinct_platform_integer<T>
    struct reflect_traits<T> {
        static constexpr std::string_view NAME = detail::platform_integer_name<T>();
        static constexpr field_kind KIND = detail::platform_integer_kind<T>();
    };

    static_assert(kind_size(field_kind::STRING_ID) == sizeof(string_id));
    static_assert(kind_size(field_kind::UUID) == sizeof(uuid));
    static_assert(kind_size(field_kind::VEC2) == sizeof(math::vec2));
    static_assert(kind_size(field_kind::VEC3) == sizeof(math::vec3));
    static_assert(kind_size(field_kind::VEC4) == sizeof(math::vec4));
    static_assert(kind_size(field_kind::QUAT) == sizeof(math::quat));
    static_assert(kind_size(field_kind::MAT4) == sizeof(math::mat4));
    static_assert(kind_size(field_kind::TRANSFORM) == sizeof(math::transform));
    static_assert(kind_size(reflect_traits<long>::KIND) == sizeof(long));
    static_assert(kind_size(reflect_traits<unsigned long>::KIND) == sizeof(unsigned long));
    static_assert(kind_size(reflect_traits<long long>::KIND) == sizeof(long long));
    static_assert(kind_size(reflect_traits<unsigned long long>::KIND) == sizeof(unsigned long long));
    static_assert(reflect_traits<usize>::KIND == field_kind::U64);
    static_assert(reflect_traits<isize>::KIND == field_kind::I64);

    /// <summary>
    /// True when reflect_traits&lt;T&gt; provides a KIND (built-in kinds, reflected structs and enums).
    /// </summary>
    template <typename T>
    concept has_reflect_kind = requires {
        { reflect_traits<std::remove_cvref_t<T>>::KIND } -> std::convertible_to<field_kind>;
    };

    namespace detail {
        template <typename T>
        struct std_array_traits {
            static constexpr bool IS_ARRAY = false;
        };

        template <typename ElementType, usize N>
        struct std_array_traits<std::array<ElementType, N>> {
            static constexpr bool IS_ARRAY = true;
            static constexpr usize COUNT = N;
            using element_type = ElementType;
        };

        template <typename T>
        inline constexpr bool ALWAYS_FALSE = false;
    }

    /// <summary>
    /// True for std::array&lt;T, N&gt;.
    /// </summary>
    template <typename T>
    concept std_array = detail::std_array_traits<std::remove_cvref_t<T>>::IS_ARRAY;

    /// <summary>
    /// True when T has a field kind: a kind override, a built-in or macro-described kind, or a
    /// std::array of such a type (arrays of arrays are not reflectable).
    /// </summary>
    template <typename T>
    concept reflectable = has_kind_override<T> || has_reflect_kind<T> ||
      (std_array<T> && (has_kind_override<typename detail::std_array_traits<std::remove_cvref_t<T>>::element_type> ||
        has_reflect_kind<typename detail::std_array_traits<std::remove_cvref_t<T>>::element_type>));

    /// <summary>
    /// The field kind of T, decided at compile time. Fails to compile with a message naming the
    /// problem when T is not reflectable.
    /// </summary>
    template <typename T>
    [[nodiscard]] constexpr field_kind kind_of() noexcept {
        using plain_type = std::remove_cvref_t<T>;
        if constexpr (has_kind_override<plain_type>) {
            static_assert(
              is_custom_kind(reflect_kind_override<plain_type>::KIND),
              "reflect_kind_override<T>::KIND must be ENTITY or ASSET_REF"
            );
            return reflect_kind_override<plain_type>::KIND;
        } else if constexpr (std_array<plain_type>) {
            using element_type = typename detail::std_array_traits<plain_type>::element_type;
            static_assert(
              !std_array<element_type>,
              "arrays of arrays are not reflectable; wrap the inner array in a struct"
            );
            static_assert(reflectable<element_type>, "the array element type is not reflectable");
            return field_kind::ARRAY;
        } else if constexpr (has_reflect_kind<plain_type>) {
            return reflect_traits<plain_type>::KIND;
        } else {
            static_assert(
              detail::ALWAYS_FALSE<plain_type>,
              "type is not reflectable: describe it with CONJURE_REFLECT_STRUCT / CONJURE_REFLECT_ENUM or "
              "specialise reflect_kind_override<T>"
            );
            return field_kind::COUNT;
        }
    }
}

template <>
struct std::formatter<conjure::core::reflect::field_kind> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(conjure::core::reflect::field_kind kind, FormatContext &context) const {
        return std::formatter<std::string_view>::format(conjure::core::reflect::to_string(kind), context);
    }
};
