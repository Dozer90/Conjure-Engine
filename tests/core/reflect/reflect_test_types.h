#pragma once

// Types shared by the reflect tests: one struct with a field of every kind, nested structs, plain
// and flag enumerations, and two fake handle types that stand in for the ecs / assets handles through
// reflect_kind_override. The macro descriptions live here so every test file sees the same ones.

#include "conjure/core/reflect/reflect.h"

#include "conjure/core/math/glm_types.h"
#include "conjure/core/math/transform.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"

#include <array>
#include <limits>
#include <string>
#include <string_view>

namespace reflect_test {

    enum class weapon_kind : conjure::u8 {
        SWORD,
        BOW = 5,
        STAFF
    };

    enum class body_flags : conjure::u32 {
        NONE = 0,
        STATIC = 1u << 0,
        DYNAMIC = 1u << 1,
        KINEMATIC = 1u << 2,
        ALL = STATIC | DYNAMIC | KINEMATIC
    };

    enum class signed_mode : conjure::i16 {
        NEGATIVE = -2,
        ZERO = 0,
        POSITIVE = 7
    };

    // Explicitly flagged through CONJURE_REFLECT_FLAGS although the name lacks the "_flags" suffix.
    enum class layer_mask : conjure::u8 {
        FIRST = 1,
        SECOND = 2
    };

    // 8 byte enumerations: the unsigned one holds a value above INT64_MAX (stored as its bit
    // pattern), the signed one the most negative i64.
    enum class big_unsigned : conjure::u64 {
        ZERO = 0,
        TOP = 0xFFFFFFFFFFFFFFFFull
    };

    enum class big_signed : conjure::i64 {
        LOW = std::numeric_limits<conjure::i64>::min(),
        HIGH = std::numeric_limits<conjure::i64>::max()
    };

    /// <summary>
    /// Stand-in for ecs::entity: an 8 byte handle serialised as an unsigned integer.
    /// </summary>
    struct fake_entity {
        conjure::u64 bits = 0;

        [[nodiscard]] bool operator==(const fake_entity &) const = default;
    };

    /// <summary>
    /// Stand-in for assets::asset_ref: a 16 byte handle serialised as a uuid string.
    /// </summary>
    struct fake_asset {
        conjure::core::uuid id = {};

        [[nodiscard]] bool operator==(const fake_asset &) const = default;
    };

    /// <summary>
    /// A handle of a size that has no JSON representation.
    /// </summary>
    struct odd_handle {
        conjure::u8 bytes[3] = {};

        [[nodiscard]] bool operator==(const odd_handle &) const = default;
    };

    struct inner {
        conjure::math::vec3 offset = conjure::math::vec3(1.0f, 2.0f, 3.0f);
        conjure::f32 weight = 0.5f;

        [[nodiscard]] bool operator==(const inner &) const = default;
    };

    struct everything {
        bool flag = true;
        conjure::i8 tiny = -1;
        conjure::i16 small = -2;
        conjure::i32 medium = -3;
        conjure::i64 large = -4;
        conjure::u8 utiny = 1;
        conjure::u16 usmall = 2;
        conjure::u32 umedium = 3;
        conjure::u64 ularge = 4;
        conjure::f32 single = 1.5f;
        conjure::f64 real = 2.5;
        std::string text = "hello";
        conjure::core::string_id id = {};
        conjure::core::uuid guid = conjure::core::uuid(0x0123456789ABCDEFull, 0xFEDCBA9876543210ull);
        conjure::math::vec2 v2 = conjure::math::vec2(1.0f, 2.0f);
        conjure::math::vec3 v3 = conjure::math::vec3(1.0f, 2.0f, 3.0f);
        conjure::math::vec4 v4 = conjure::math::vec4(1.0f, 2.0f, 3.0f, 4.0f);
        conjure::math::quat rot = conjure::math::quat(1.0f, 0.0f, 0.0f, 0.0f);
        conjure::math::mat4 matrix = conjure::math::mat4(1.0f);
        conjure::math::transform pose = {};
        weapon_kind weapon = weapon_kind::BOW;
        body_flags body = body_flags::STATIC;
        signed_mode mode = signed_mode::NEGATIVE;
        inner nested = {};
        std::array<conjure::i32, 3> counts = {1, 2, 3};
        std::array<inner, 2> parts = {};
        std::array<weapon_kind, 2> weapons = {weapon_kind::SWORD, weapon_kind::STAFF};
        fake_entity entity = {};
        fake_asset asset = {};

        [[nodiscard]] bool operator==(const everything &) const = default;
    };

    struct projectile {
        conjure::math::vec3 position = conjure::math::vec3(0.0f);
        conjure::f32 speed = 1.0f;
        conjure::u32 internalCounter = 0;
        conjure::f32 drag = 0.0f;
    };

    struct turret {
        conjure::f32 yaw = 0.0f;
        conjure::f64 arc = 90.0;
        std::array<conjure::f32, 2> limits = {};
        conjure::f32 range = 10.0f;
    };

    struct with_odd_handle {
        odd_handle handle = {};
        conjure::u8 pad = 0;
    };

    struct empty_struct {};

    struct wide_enums {
        big_unsigned top = big_unsigned::TOP;
        big_signed low = big_signed::LOW;

        [[nodiscard]] bool operator==(const wide_enums &) const = default;
    };

    struct not_reflected {
        int value = 0;
    };
}

template <>
struct conjure::core::reflect::reflect_kind_override<reflect_test::fake_entity> {
    static constexpr conjure::core::reflect::field_kind KIND = conjure::core::reflect::field_kind::ENTITY;
    static constexpr std::string_view NAME = "test.entity";
};

template <>
struct conjure::core::reflect::reflect_kind_override<reflect_test::fake_asset> {
    static constexpr conjure::core::reflect::field_kind KIND = conjure::core::reflect::field_kind::ASSET_REF;
    static constexpr std::string_view NAME = "test.asset_ref";
};

template <>
struct conjure::core::reflect::reflect_kind_override<reflect_test::odd_handle> {
    static constexpr conjure::core::reflect::field_kind KIND = conjure::core::reflect::field_kind::ENTITY;
    static constexpr std::string_view NAME = "test.odd_handle";
};

CONJURE_REFLECT_ENUM(reflect_test::weapon_kind, "test.weapon_kind", SWORD, BOW, STAFF);
CONJURE_REFLECT_ENUM(reflect_test::body_flags, "test.body_flags", NONE, STATIC, DYNAMIC, KINEMATIC, ALL);
CONJURE_REFLECT_ENUM(reflect_test::signed_mode, "test.signed_mode", NEGATIVE, ZERO, POSITIVE);
CONJURE_REFLECT_FLAGS(reflect_test::layer_mask, "test.layer_mask", FIRST, SECOND);

CONJURE_REFLECT_STRUCT(reflect_test::inner, "test.inner", CONJURE_FIELD(offset), CONJURE_FIELD(weight));

CONJURE_REFLECT_STRUCT(reflect_test::everything, "test.everything",
    CONJURE_FIELD(flag),
    CONJURE_FIELD(tiny),
    CONJURE_FIELD(small),
    CONJURE_FIELD(medium),
    CONJURE_FIELD(large),
    CONJURE_FIELD(utiny),
    CONJURE_FIELD(usmall),
    CONJURE_FIELD(umedium),
    CONJURE_FIELD(ularge),
    CONJURE_FIELD(single),
    CONJURE_FIELD(real),
    CONJURE_FIELD(text),
    CONJURE_FIELD(id),
    CONJURE_FIELD(guid),
    CONJURE_FIELD(v2),
    CONJURE_FIELD(v3),
    CONJURE_FIELD(v4),
    CONJURE_FIELD(rot),
    CONJURE_FIELD(matrix),
    CONJURE_FIELD(pose),
    CONJURE_FIELD(weapon),
    CONJURE_FIELD(body),
    CONJURE_FIELD(mode),
    CONJURE_FIELD(nested),
    CONJURE_FIELD(counts),
    CONJURE_FIELD(parts),
    CONJURE_FIELD(weapons),
    CONJURE_FIELD(entity),
    CONJURE_FIELD(asset));

CONJURE_REFLECT_STRUCT(reflect_test::projectile, "test.projectile",
    CONJURE_FIELD(position, .tooltip = "World position", .category = "Motion"),
    CONJURE_FIELD(speed, .min = 0.0, .max = 100.0, .step = 0.5, .tooltip = "Units per second"),
    CONJURE_FIELD_HIDDEN(internalCounter),
    CONJURE_FIELD_READ_ONLY(drag));

CONJURE_REFLECT_STRUCT(reflect_test::turret, "test.turret",
    CONJURE_FIELD(yaw, .min = -3.0, .max = 3.0, .unit = conjure::core::reflect::field_unit::RADIANS),
    CONJURE_FIELD(arc, .unit = conjure::core::reflect::field_unit::DEGREES),
    CONJURE_FIELD(limits, .unit = conjure::core::reflect::field_unit::RADIANS),
    CONJURE_FIELD(range, .min = 0.0));

CONJURE_REFLECT_STRUCT(reflect_test::with_odd_handle, "test.with_odd_handle",
    CONJURE_FIELD(handle),
    CONJURE_FIELD(pad));

CONJURE_REFLECT_STRUCT(reflect_test::empty_struct, "test.empty");

CONJURE_REFLECT_ENUM(reflect_test::big_unsigned, "test.big_unsigned", ZERO, TOP);
CONJURE_REFLECT_ENUM(reflect_test::big_signed, "test.big_signed", LOW, HIGH);
CONJURE_REFLECT_STRUCT(reflect_test::wide_enums, "test.wide_enums", CONJURE_FIELD(top), CONJURE_FIELD(low));
