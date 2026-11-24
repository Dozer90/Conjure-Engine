#include "conjure/testing/doctest.h"

#include "reflect_test_types.h"

#include "conjure/core/reflect/field_info.h"

#include <array>
#include <format>
#include <optional>
#include <string>
#include <string_view>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::reflect;

namespace {
    enum_info make_weapon_info() {
        enum_info info = {};
        info.name = "test.weapon_kind";
        info.type = type_id::from_name(info.name);
        info.underlyingSize = 1;
        info.isSigned = false;
        info.values = {{"SWORD", 0}, {"BOW", 5}, {"STAFF", 6}};
        return info;
    }

    enum_info make_body_flags_info() {
        enum_info info = {};
        info.name = "test.body_flags";
        info.type = type_id::from_name(info.name);
        info.underlyingSize = 4;
        info.isFlags = true;
        info.values = {{"NONE", 0}, {"STATIC", 1}, {"DYNAMIC", 2}, {"KINEMATIC", 4}, {"ALL", 7}};
        return info;
    }
}

TEST_SUITE("core/field_info") {

    TEST_CASE("field_info - every kind has a stable name that parses back") {
        for (u8 index = 0; index < static_cast<u8>(field_kind::COUNT); ++index) {
            const field_kind kind = static_cast<field_kind>(index);
            const std::string_view name = to_string(kind);
            CHECK_FALSE(name.empty());
            CHECK(name != "UNKNOWN");
            REQUIRE(parse_field_kind(name).has_value());
            CHECK(*parse_field_kind(name) == kind);
        }
        CHECK(to_string(field_kind::BOOL) == "BOOL");
        CHECK(to_string(field_kind::STRING_ID) == "STRING_ID");
        CHECK(to_string(field_kind::ASSET_REF) == "ASSET_REF");
        CHECK(to_string(field_kind::COUNT) == "UNKNOWN");
        CHECK(to_string(static_cast<field_kind>(200)) == "UNKNOWN");
        CHECK_FALSE(parse_field_kind("vec3").has_value());
        CHECK_FALSE(parse_field_kind("UNKNOWN").has_value());
        CHECK_FALSE(parse_field_kind("").has_value());
        CHECK(std::format("{}", field_kind::VEC3) == "VEC3");
        CHECK(std::format("{:>6}", field_kind::I8) == "    I8");
    }

    TEST_CASE("field_info - kind_size and kind_alignment match the C++ types") {
        static_assert(kind_size(field_kind::BOOL) == sizeof(bool));
        static_assert(kind_size(field_kind::I8) == 1 && kind_size(field_kind::U8) == 1);
        static_assert(kind_size(field_kind::I16) == 2 && kind_size(field_kind::U16) == 2);
        static_assert(kind_size(field_kind::I32) == 4 && kind_size(field_kind::U32) == 4);
        static_assert(kind_size(field_kind::F32) == 4);
        static_assert(kind_size(field_kind::I64) == 8 && kind_size(field_kind::U64) == 8);
        static_assert(kind_size(field_kind::F64) == 8);
        static_assert(kind_size(field_kind::STRING) == sizeof(std::string));
        static_assert(kind_size(field_kind::STRING_ID) == 8);
        static_assert(kind_size(field_kind::UUID) == 16);
        static_assert(kind_size(field_kind::VEC2) == 8 && kind_size(field_kind::VEC3) == 12);
        static_assert(kind_size(field_kind::VEC4) == 16);
        static_assert(kind_size(field_kind::QUAT) == 16 && kind_size(field_kind::MAT4) == 64);
        static_assert(kind_size(field_kind::TRANSFORM) == 40);
        static_assert(kind_size(field_kind::ENUM) == 0 && kind_size(field_kind::STRUCT) == 0);
        static_assert(kind_size(field_kind::ARRAY) == 0);
        static_assert(kind_size(field_kind::ENTITY) == 0 && kind_size(field_kind::ASSET_REF) == 0);
        static_assert(kind_size(field_kind::COUNT) == 0);

        static_assert(kind_alignment(field_kind::I8) == 1 && kind_alignment(field_kind::I16) == 2);
        static_assert(kind_alignment(field_kind::F32) == 4 && kind_alignment(field_kind::F64) == 8);
        static_assert(kind_alignment(field_kind::VEC3) == alignof(math::vec3));
        static_assert(kind_alignment(field_kind::MAT4) == alignof(math::mat4));
        static_assert(kind_alignment(field_kind::TRANSFORM) == alignof(math::transform));
        static_assert(kind_alignment(field_kind::UUID) == alignof(uuid));
        static_assert(kind_alignment(field_kind::STRING_ID) == alignof(string_id));
        static_assert(kind_alignment(field_kind::STRING) == alignof(std::string));
        static_assert(kind_alignment(field_kind::STRUCT) == 0 && kind_alignment(field_kind::ENTITY) == 0);
        CHECK(true);
    }

    TEST_CASE("field_info - kind predicates classify the kinds") {
        static_assert(is_fixed_size_kind(field_kind::F32));
        static_assert(is_fixed_size_kind(field_kind::TRANSFORM));
        static_assert(!is_fixed_size_kind(field_kind::STRUCT));
        static_assert(!is_fixed_size_kind(field_kind::ARRAY));
        static_assert(is_integer_kind(field_kind::I8) && is_integer_kind(field_kind::U64));
        static_assert(!is_integer_kind(field_kind::BOOL) && !is_integer_kind(field_kind::F32));
        static_assert(is_float_kind(field_kind::F32) && is_float_kind(field_kind::F64));
        static_assert(!is_float_kind(field_kind::I32));
        static_assert(is_custom_kind(field_kind::ENTITY) && is_custom_kind(field_kind::ASSET_REF));
        static_assert(!is_custom_kind(field_kind::STRUCT));
        CHECK(true);
    }

    TEST_CASE("field_info - kind_of derives the kind of every reflectable type at compile time") {
        static_assert(kind_of<bool>() == field_kind::BOOL);
        static_assert(kind_of<i8>() == field_kind::I8);
        static_assert(kind_of<i16>() == field_kind::I16);
        static_assert(kind_of<i32>() == field_kind::I32);
        static_assert(kind_of<i64>() == field_kind::I64);
        static_assert(kind_of<u8>() == field_kind::U8);
        static_assert(kind_of<u16>() == field_kind::U16);
        static_assert(kind_of<u32>() == field_kind::U32);
        static_assert(kind_of<u64>() == field_kind::U64);
        static_assert(kind_of<f32>() == field_kind::F32);
        static_assert(kind_of<f64>() == field_kind::F64);
        static_assert(kind_of<std::string>() == field_kind::STRING);
        static_assert(kind_of<string_id>() == field_kind::STRING_ID);
        static_assert(kind_of<uuid>() == field_kind::UUID);
        static_assert(kind_of<math::vec2>() == field_kind::VEC2);
        static_assert(kind_of<math::vec3>() == field_kind::VEC3);
        static_assert(kind_of<math::vec4>() == field_kind::VEC4);
        static_assert(kind_of<math::quat>() == field_kind::QUAT);
        static_assert(kind_of<math::mat4>() == field_kind::MAT4);
        static_assert(kind_of<math::transform>() == field_kind::TRANSFORM);
        static_assert(kind_of<reflect_test::weapon_kind>() == field_kind::ENUM);
        static_assert(kind_of<reflect_test::body_flags>() == field_kind::ENUM);
        static_assert(kind_of<reflect_test::inner>() == field_kind::STRUCT);
        static_assert(kind_of<std::array<i32, 3>>() == field_kind::ARRAY);
        static_assert(kind_of<std::array<reflect_test::inner, 2>>() == field_kind::ARRAY);
        static_assert(kind_of<reflect_test::fake_entity>() == field_kind::ENTITY);
        static_assert(kind_of<reflect_test::fake_asset>() == field_kind::ASSET_REF);
        static_assert(kind_of<const math::vec3 &>() == field_kind::VEC3);
        CHECK(true);
    }

    TEST_CASE("field_info - platform integer types map to the fixed-width kind of their width") {
        static_assert(kind_of<long>() == (sizeof(long) == 8 ? field_kind::I64 : field_kind::I32));
        static_assert(kind_of<unsigned long>() == (sizeof(long) == 8 ? field_kind::U64 : field_kind::U32));
        static_assert(kind_of<long long>() == field_kind::I64);
        static_assert(kind_of<unsigned long long>() == field_kind::U64);
        static_assert(kind_of<usize>() == field_kind::U64);
        static_assert(kind_of<isize>() == field_kind::I64);
        static_assert(reflectable<long> && reflectable<unsigned long>);
        static_assert(reflectable<long long> && reflectable<unsigned long long>);
        static_assert(type_id_of<long long>() == type_id_of<i64>());
        static_assert(type_id_of<unsigned long long>() == type_id_of<u64>());
        static_assert(type_id_of<usize>() == type_id_of<u64>());
        static_assert(reflect_name_of<isize>() == "i64");
        static_assert(reflect_name_of<long>() == (sizeof(long) == 8 ? "i64" : "i32"));
        CHECK(true);
    }

    TEST_CASE("field_info - is_valid_enum_size accepts exactly the four storage widths") {
        static_assert(is_valid_enum_size(1) && is_valid_enum_size(2));
        static_assert(is_valid_enum_size(4) && is_valid_enum_size(8));
        static_assert(!is_valid_enum_size(0) && !is_valid_enum_size(3));
        static_assert(!is_valid_enum_size(16) && !is_valid_enum_size(6));
        CHECK(true);
    }

    TEST_CASE("field_info - the reflectable concept accepts reflected types and rejects everything else") {
        static_assert(reflectable<f32>);
        static_assert(reflectable<reflect_test::inner>);
        static_assert(reflectable<reflect_test::weapon_kind>);
        static_assert(reflectable<reflect_test::fake_entity>);
        static_assert(reflectable<std::array<f32, 4>>);
        static_assert(reflectable<std::array<reflect_test::fake_entity, 2>>);
        static_assert(!reflectable<reflect_test::not_reflected>);
        static_assert(!reflectable<int *>);
        static_assert(!reflectable<std::array<reflect_test::not_reflected, 2>>);
        static_assert(!reflectable<std::array<std::array<f32, 2>, 2>>);
        static_assert(std_array<std::array<f32, 2>>);
        static_assert(!std_array<f32>);
        static_assert(has_reflect_kind<f32>);
        static_assert(!has_reflect_kind<reflect_test::fake_entity>);
        CHECK(true);
    }

    TEST_CASE("field_info - field_info helpers describe arrays and scalars") {
        field_info scalar = {};
        scalar.kind = field_kind::F32;
        scalar.size = 4;
        CHECK_FALSE(scalar.is_array());
        CHECK(scalar.value_kind() == field_kind::F32);
        CHECK(scalar.element_size() == 4);

        field_info array = {};
        array.kind = field_kind::ARRAY;
        array.elementKind = field_kind::I16;
        array.arrayCount = 5;
        array.size = 10;
        CHECK(array.is_array());
        CHECK(array.value_kind() == field_kind::I16);
        CHECK(array.element_size() == 2);

        array.arrayCount = 0;
        CHECK(array.element_size() == 0);

        const field_info unset = {};
        CHECK(unset.kind == field_kind::COUNT);
        CHECK(unset.value_kind() == field_kind::COUNT);
        CHECK(unset.structType == nullptr);
        CHECK(unset.enumType == nullptr);
        CHECK(unset.defaultValueJson.empty());
    }

    TEST_CASE("field_info - field_attributes default to no hints and compare by value") {
        const field_attributes defaults = {};
        CHECK_FALSE(defaults.min.has_value());
        CHECK_FALSE(defaults.max.has_value());
        CHECK(defaults.step == 0.0);
        CHECK(defaults.tooltip.empty());
        CHECK_FALSE(defaults.hidden);
        CHECK_FALSE(defaults.readOnly);
        CHECK(defaults.category.empty());
        CHECK(defaults.unit == field_unit::NONE);

        const field_attributes ranged = {.min = 0.0, .max = 1.0, .tooltip = "t"};
        CHECK(ranged == field_attributes{.min = 0.0, .max = 1.0, .tooltip = "t"});
        CHECK(ranged != defaults);
        CHECK(field_attributes{.unit = field_unit::RADIANS} != defaults);
        CHECK(field_attributes{.unit = field_unit::RADIANS} != field_attributes{.unit = field_unit::DEGREES});
    }

    TEST_CASE("field_info - to_string of a field_unit is the lower-case name the editor protocol sends") {
        CHECK(to_string(field_unit::NONE) == "none");
        CHECK(to_string(field_unit::RADIANS) == "radians");
        CHECK(to_string(field_unit::DEGREES) == "degrees");
        CHECK(to_string(static_cast<field_unit>(3)) == "unknown");
        CHECK(to_string(static_cast<field_unit>(255)) == "unknown");
    }

    TEST_CASE("field_info - enum_info finds enumerators by name and value") {
        const enum_info weapon = make_weapon_info();
        CHECK(weapon.find_value("BOW") == 5);
        CHECK(weapon.find_value("SWORD") == 0);
        CHECK_FALSE(weapon.find_value("bow").has_value());
        CHECK_FALSE(weapon.find_value("").has_value());
        REQUIRE(weapon.find_name(6) != nullptr);
        CHECK(*weapon.find_name(6) == "STAFF");
        CHECK(weapon.find_name(1) == nullptr);
    }

    TEST_CASE("field_info - enum_info formats and parses plain enumerations by exact name") {
        const enum_info weapon = make_weapon_info();
        CHECK(weapon.format_value(5) == "BOW");
        CHECK(weapon.format_value(0) == "SWORD");
        CHECK_FALSE(weapon.format_value(3).has_value());
        CHECK(weapon.parse_value("STAFF") == 6);
        CHECK_FALSE(weapon.parse_value("STAFF|BOW").has_value());
        CHECK_FALSE(weapon.parse_value(" BOW").has_value());
    }

    TEST_CASE("field_info - enum_info decomposes and recombines flag sets") {
        const enum_info flags = make_body_flags_info();
        CHECK(flags.format_value(0) == "NONE");
        CHECK(flags.format_value(1) == "STATIC");
        CHECK(flags.format_value(3) == "STATIC|DYNAMIC");
        CHECK(flags.format_value(6) == "DYNAMIC|KINEMATIC");
        CHECK(flags.format_value(7) == "ALL");
        CHECK_FALSE(flags.format_value(8).has_value());
        CHECK_FALSE(flags.format_value(9).has_value());

        CHECK(flags.parse_value("STATIC|DYNAMIC") == 3);
        CHECK(flags.parse_value(" STATIC | KINEMATIC ") == 5);
        CHECK(flags.parse_value("ALL") == 7);
        CHECK(flags.parse_value("ALL|STATIC") == 7);
        CHECK(flags.parse_value("DYNAMIC|") == 2);
        CHECK(flags.parse_value("") == 0);
        CHECK(flags.parse_value("NONE") == 0);
        CHECK_FALSE(flags.parse_value("STATIC|BOGUS").has_value());
        CHECK_FALSE(flags.parse_value("static").has_value());
    }

    TEST_CASE("field_info - flag sets without a zero enumerator format zero as the empty string") {
        enum_info flags = make_body_flags_info();
        flags.values.erase(flags.values.begin());
        CHECK(flags.format_value(0) == "");
        CHECK(flags.parse_value("") == 0);
    }

    TEST_CASE("field_info - enum_info knows which values fit the underlying type") {
        enum_info info = {};
        info.underlyingSize = 1;
        info.isSigned = false;
        CHECK(info.is_representable(0));
        CHECK(info.is_representable(255));
        CHECK_FALSE(info.is_representable(256));
        CHECK_FALSE(info.is_representable(-1));

        info.isSigned = true;
        CHECK(info.is_representable(-128));
        CHECK(info.is_representable(127));
        CHECK_FALSE(info.is_representable(128));
        CHECK_FALSE(info.is_representable(-129));

        info.underlyingSize = 2;
        CHECK(info.is_representable(-32768));
        CHECK_FALSE(info.is_representable(32768));
        info.isSigned = false;
        CHECK(info.is_representable(65535));
        CHECK_FALSE(info.is_representable(65536));

        info.underlyingSize = 4;
        CHECK(info.is_representable(4294967295ll));
        CHECK_FALSE(info.is_representable(4294967296ll));
        info.isSigned = true;
        CHECK(info.is_representable(-2147483648ll));
        CHECK_FALSE(info.is_representable(2147483648ll));

        info.underlyingSize = 8;
        CHECK(info.is_representable(-1));
        CHECK(info.is_representable(std::numeric_limits<i64>::max()));
        info.isSigned = false;
        CHECK(info.is_representable(-1));   // the bit pattern of u64 max
    }

    TEST_CASE("field_info - is_representable is false for every width other than 1, 2, 4 and 8") {
        for (const u32 width : {0u, 3u, 5u, 16u}) {
            enum_info info = {};
            info.underlyingSize = width;
            CHECK_FALSE(info.is_representable(0));
            CHECK_FALSE(info.is_representable(1));
            CHECK_FALSE(info.is_representable(-1));
            info.isSigned = true;
            CHECK_FALSE(info.is_representable(0));
        }
    }

    TEST_CASE("field_info - enum_info compares by value") {
        const enum_info first = make_weapon_info();
        enum_info second = make_weapon_info();
        CHECK(first == second);
        second.values.back().second = 9;
        CHECK(first != second);
    }
}
