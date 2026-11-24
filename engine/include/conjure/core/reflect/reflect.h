#pragma once

#include "conjure/core/reflect/field_info.h"
#include "conjure/core/reflect/json_conversion.h"
#include "conjure/core/reflect/type_id.h"
#include "conjure/core/reflect/type_info.h"
#include "conjure/core/reflect/type_registry.h"

#include "conjure/core/api.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/string_utils.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

// Umbrella header of the reflect area plus the description macros.
//
//   struct projectile { math::vec3 position = {}; f32 speed = 1.0f; u32 internalCounter = 0; };
//   CONJURE_REFLECT_STRUCT(projectile, "game.projectile",
//       CONJURE_FIELD(position),
//       CONJURE_FIELD(speed, .min = 0.0, .max = 100.0, .tooltip = "Units per second"),
//       CONJURE_FIELD_HIDDEN(internalCounter));
//
//   enum class damage_type : u8 { BLUNT, FIRE, FROST };
//   CONJURE_REFLECT_ENUM(damage_type, "game.damage_type", BLUNT, FIRE, FROST);
//   CONJURE_REFLECT_FLAGS(collision_flags, "game.collision_flags", NONE, STATIC, DYNAMIC);
//
// The macros are used at global namespace scope (they specialise conjure::core::reflect::reflect_traits),
// take the caller's semicolon like CONJURE_ENUM_FLAGS and CONJURE_DECLARE_ERROR_CATEGORY (a missing one
// is a compile error on every compiler, a stray one would be -Wpedantic noise on GCC/Clang) and take the
// type's fully qualified C++ name plus the registered name the id is hashed from. Field
// attributes use designated initialisers in field_attributes declaration order: min, max, step, tooltip,
// hidden, readOnly, category, unit (the field_unit an F32 or F64 field is stored in, spelled out in full
// at global scope: .unit = conjure::core::reflect::field_unit::RADIANS). A reflected struct must not be
// polymorphic and its members must be reflectable (kind_of<T>()); member offsets come from offsetof, so
// keep reflected structs plain data.
// Members may use the fixed-width aliases or the platform integer types (long, long long, usize, ...),
// which reflect as the fixed-width kind of their width. Enumerations need at least one enumerator (at
// most 64 per macro); a name ending in "_flags" is treated as a flag set even with CONJURE_REFLECT_ENUM.

namespace conjure::core::reflect::detail {

    /// <summary>
    /// Fills kind, type and the struct/enum description pointer for one value type. 'asElement'
    /// targets elementKind instead of kind (array elements).
    /// </summary>
    template <typename ValueType>
    void describe_value_type(field_info &field, bool asElement) {
        using plain_type = std::remove_cvref_t<ValueType>;
        static_assert(!std_array<plain_type> || has_kind_override<plain_type>, "nested arrays are not reflectable");

        const field_kind kind = kind_of<plain_type>();
        if (asElement) {
            field.elementKind = kind;
        } else {
            field.kind = kind;
        }
        field.type = type_id_of<plain_type>();

        if constexpr (reflected_struct<plain_type>) {
            field.structType = type_info_of<plain_type>();
        } else if constexpr (reflected_enum<plain_type>) {
            field.enumType = enum_info_of<plain_type>();
        }
    }

    /// <summary>
    /// Builds the description of one member from its declared type, name and offset.
    /// </summary>
    template <typename MemberType>
    [[nodiscard]] field_info make_field(std::string_view name, usize offset, field_attributes attributes) {
        using plain_type = std::remove_cvref_t<MemberType>;

        field_info field = {};
        field.name = std::string(name);
        field.offset = static_cast<u32>(offset);
        field.size = static_cast<u32>(sizeof(plain_type));
        field.attributes = std::move(attributes);

        if constexpr (std_array<plain_type> && !has_kind_override<plain_type>) {
            using element_type = typename std_array_traits<plain_type>::element_type;
            field.kind = field_kind::ARRAY;
            field.arrayCount = static_cast<u32>(std_array_traits<plain_type>::COUNT);
            describe_value_type<element_type>(field, true);
        } else {
            describe_value_type<plain_type>(field, false);
        }
        return field;
    }

    /// <summary>
    /// Validates a macro-built description; an inconsistent layout is a programming error and asserts.
    /// </summary>
    CONJURE_API void finish_struct_description(type_info &info);

    /// <summary>
    /// Serialises every field of 'defaults' (an instance of the described type) into
    /// field_info::defaultValueJson.
    /// </summary>
    CONJURE_API void fill_default_values(type_info &info, const void *defaults);

    /// <summary>
    /// Assembles the type_info of a macro-described struct.
    /// </summary>
    template <typename StructType>
    [[nodiscard]] type_info build_struct_description(std::string_view name, std::initializer_list<field_info> fields) {
        static_assert(std::is_class_v<StructType>, "CONJURE_REFLECT_STRUCT describes classes and structs");
        static_assert(!std::is_polymorphic_v<StructType>, "reflected structs must not be polymorphic");

        type_info info = {};
        info.name = std::string(name);
        info.type = type_id::from_name(name);
        info.size = static_cast<u32>(sizeof(StructType));
        info.alignment = static_cast<u32>(alignof(StructType));
        info.isTriviallyCopyable = std::is_trivially_copyable_v<StructType>;
        info.fields.assign(fields.begin(), fields.end());
        finish_struct_description(info);

        if constexpr (std::is_default_constructible_v<StructType>) {
            const StructType defaults = StructType();
            fill_default_values(info, &defaults);
        }
        return info;
    }

    /// <summary>
    /// Assembles the enum_info of a macro-described enumeration.
    /// </summary>
    template <typename EnumType>
    [[nodiscard]] enum_info build_enum_description(
      std::string_view name,
      bool isFlags,
      std::initializer_list<std::pair<std::string, i64>> values
    ) {
        static_assert(std::is_enum_v<EnumType>, "CONJURE_REFLECT_ENUM describes enumerations");
        using underlying_type = std::underlying_type_t<EnumType>;

        enum_info info = {};
        info.name = std::string(name);
        info.type = type_id::from_name(name);
        info.underlyingSize = static_cast<u32>(sizeof(underlying_type));
        info.isSigned = std::is_signed_v<underlying_type>;
        info.isFlags = isFlags || str::ends_with(name, "_flags");
        info.values.assign(values.begin(), values.end());
        return info;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Description macros

#define CONJURE_REFLECT_STRUCT(struct_type, display_name, ...)                                              \
    template <>                                                                                             \
    struct conjure::core::reflect::reflect_traits<struct_type> {                                            \
        using self_type = struct_type;                                                                      \
        static constexpr ::std::string_view NAME = display_name;                                            \
        static constexpr ::conjure::core::reflect::field_kind KIND =                                        \
          ::conjure::core::reflect::field_kind::STRUCT;                                                     \
        [[nodiscard]] static ::conjure::core::reflect::type_info describe() {                               \
            return ::conjure::core::reflect::detail::build_struct_description<self_type>(                   \
              NAME,                                                                                         \
              {__VA_ARGS__}                                                                                 \
            );                                                                                              \
        }                                                                                                   \
    }

#define CONJURE_FIELD(member, ...)                                                                          \
    ::conjure::core::reflect::detail::make_field<decltype(self_type::member)>(                              \
      #member,                                                                                              \
      offsetof(self_type, member),                                                                          \
      ::conjure::core::reflect::field_attributes{__VA_ARGS__}                                               \
    )

#define CONJURE_FIELD_HIDDEN(member) CONJURE_FIELD(member, .hidden = true)

#define CONJURE_FIELD_READ_ONLY(member) CONJURE_FIELD(member, .readOnly = true)

#define CONJURE_REFLECT_ENUM(enum_type, display_name, ...)                                                  \
    CONJURE_REFLECT_ENUM_IMPL(enum_type, display_name, false, __VA_ARGS__)

#define CONJURE_REFLECT_FLAGS(enum_type, display_name, ...)                                                 \
    CONJURE_REFLECT_ENUM_IMPL(enum_type, display_name, true, __VA_ARGS__)

#define CONJURE_REFLECT_ENUM_IMPL(enum_type, display_name, is_flags, ...)                                   \
    template <>                                                                                             \
    struct conjure::core::reflect::reflect_traits<enum_type> {                                              \
        using self_type = enum_type;                                                                        \
        static constexpr ::std::string_view NAME = display_name;                                            \
        static constexpr ::conjure::core::reflect::field_kind KIND =                                        \
          ::conjure::core::reflect::field_kind::ENUM;                                                       \
        [[nodiscard]] static ::conjure::core::reflect::enum_info describe() {                               \
            return ::conjure::core::reflect::detail::build_enum_description<self_type>(                     \
              NAME,                                                                                         \
              is_flags,                                                                                     \
              {CONJURE_REFLECT_FOR_EACH(CONJURE_REFLECT_ENUMERATOR, __VA_ARGS__)}                           \
            );                                                                                              \
        }                                                                                                   \
    }

#define CONJURE_REFLECT_ENUMERATOR(value) {#value, static_cast<::conjure::i64>(self_type::value)},

#define CONJURE_REFLECT_FOR_EACH(macro, ...)                                                                \
    CONJURE_CONCAT(CONJURE_REFLECT_FOR_EACH_, CONJURE_REFLECT_ARG_COUNT(__VA_ARGS__))(macro, __VA_ARGS__)

#define CONJURE_REFLECT_ARG_COUNT_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14,   \
  _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, \
  _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, \
  _55, _56, _57, _58, _59, _60, _61, _62, _63, _64, N, ...) N

#define CONJURE_REFLECT_ARG_COUNT(...) CONJURE_REFLECT_ARG_COUNT_IMPL(__VA_ARGS__, 64, 63, 62, 61, 60, \
  59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35,  \
  34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10,  \
  9, 8, 7, 6, 5, 4, 3, 2, 1)

#define CONJURE_REFLECT_FOR_EACH_1(macro, a) macro(a)
#define CONJURE_REFLECT_FOR_EACH_2(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_1(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_3(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_2(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_4(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_3(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_5(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_4(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_6(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_5(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_7(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_6(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_8(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_7(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_9(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_8(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_10(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_9(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_11(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_10(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_12(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_11(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_13(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_12(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_14(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_13(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_15(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_14(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_16(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_15(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_17(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_16(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_18(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_17(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_19(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_18(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_20(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_19(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_21(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_20(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_22(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_21(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_23(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_22(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_24(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_23(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_25(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_24(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_26(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_25(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_27(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_26(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_28(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_27(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_29(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_28(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_30(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_29(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_31(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_30(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_32(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_31(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_33(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_32(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_34(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_33(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_35(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_34(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_36(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_35(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_37(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_36(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_38(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_37(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_39(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_38(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_40(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_39(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_41(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_40(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_42(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_41(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_43(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_42(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_44(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_43(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_45(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_44(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_46(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_45(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_47(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_46(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_48(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_47(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_49(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_48(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_50(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_49(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_51(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_50(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_52(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_51(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_53(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_52(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_54(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_53(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_55(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_54(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_56(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_55(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_57(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_56(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_58(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_57(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_59(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_58(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_60(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_59(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_61(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_60(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_62(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_61(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_63(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_62(macro, __VA_ARGS__)
#define CONJURE_REFLECT_FOR_EACH_64(macro, a, ...) macro(a) CONJURE_REFLECT_FOR_EACH_63(macro, __VA_ARGS__)
