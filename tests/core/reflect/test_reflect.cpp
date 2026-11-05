#include "conjure/testing/doctest.h"

#include "reflect_test_types.h"

#include "conjure/core/json/document.h"
#include "conjure/core/reflect/reflect.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::reflect;

namespace {
    enum class local_mode : u8 {
        ONE = 1,
        TWO = 2
    };

    enum class crowd : u16 {
        A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15,
        A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29, A30, A31,
        A32, A33, A34, A35, A36, A37, A38, A39, A40, A41, A42, A43, A44, A45, A46, A47,
        A48, A49, A50, A51, A52, A53, A54, A55, A56, A57, A58, A59, A60, A61, A62, A63
    };

    // A struct whose only member is a reflected struct with an enum: exercises the nested pointers
    // in a type described in a second translation unit.
    struct wrapper {
        reflect_test::inner body = {};
        local_mode mode = local_mode::TWO;
    };

    /// <summary>
    /// A dynamic (C#) component with the exact layout of reflect_test::everything, built from a
    /// field list instead of the macro.
    /// </summary>
    result<reflect::type_info> build_everything_by_hand() {
        const reflect::type_info *macroBuilt = type_info_of<reflect_test::everything>();
        REQUIRE(macroBuilt != nullptr);

        std::vector<field_info> fields = {};
        for (const field_info &source : macroBuilt->fields) {
            field_info field = {};
            field.name = source.name;
            field.type = source.type;
            field.kind = source.kind;
            field.offset = source.offset;
            field.size = source.size;
            field.arrayCount = source.arrayCount;
            field.elementKind = source.elementKind;
            field.structType = source.structType;
            field.enumType = source.enumType;
            fields.push_back(field);
        }
        return build_type_info(
          "test.everything",
          static_cast<u32>(sizeof(reflect_test::everything)),
          static_cast<u32>(alignof(reflect_test::everything)),
          fields
        );
    }
}

CONJURE_REFLECT_ENUM(local_mode, "test.local_mode", ONE, TWO);
CONJURE_REFLECT_ENUM(crowd, "test.crowd",
    A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15,
    A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29, A30, A31,
    A32, A33, A34, A35, A36, A37, A38, A39, A40, A41, A42, A43, A44, A45, A46, A47,
    A48, A49, A50, A51, A52, A53, A54, A55, A56, A57, A58, A59, A60, A61, A62, A63);
CONJURE_REFLECT_STRUCT(wrapper, "test.wrapper", CONJURE_FIELD(body), CONJURE_FIELD(mode));

TEST_SUITE("core/reflect") {

    TEST_CASE("reflect - a runtime-built reflect::type_info has the same layout as the macro-built one") {
        const result<reflect::type_info> built = build_everything_by_hand();
        REQUIRE(built.has_value());
        const reflect::type_info *macroBuilt = type_info_of<reflect_test::everything>();
        REQUIRE(macroBuilt != nullptr);

        CHECK(built->name == macroBuilt->name);
        CHECK(built->type == macroBuilt->type);
        CHECK(built->size == macroBuilt->size);
        CHECK(built->alignment == macroBuilt->alignment);
        CHECK(built->isTriviallyCopyable == macroBuilt->isTriviallyCopyable);
        CHECK(built->layout_hash() == macroBuilt->layout_hash());
        CHECK(built->has_same_layout(*macroBuilt));
        REQUIRE(built->fields.size() == macroBuilt->fields.size());
        for (usize index = 0; index < built->fields.size(); ++index) {
            const field_info &manual = built->fields[index];
            const field_info &described = macroBuilt->fields[index];
            CHECK(manual.name == described.name);
            CHECK(manual.kind == described.kind);
            CHECK(manual.type == described.type);
            CHECK(manual.offset == described.offset);
            CHECK(manual.size == described.size);
            CHECK(manual.arrayCount == described.arrayCount);
            CHECK(manual.elementKind == described.elementKind);
            CHECK(manual.structType == described.structType);
            CHECK(manual.enumType == described.enumType);
        }
    }

    TEST_CASE("reflect - registering the runtime-built twin returns the macro-built description") {
        const result<reflect::type_info> built = build_everything_by_hand();
        REQUIRE(built.has_value());
        const result<const reflect::type_info *> registered = registry().register_type(*built);
        REQUIRE(registered.has_value());
        CHECK(registered.value() == type_info_of<reflect_test::everything>());
    }

    TEST_CASE("reflect - a runtime-built type serialises and deserialises exactly like the macro-built one") {
        const result<reflect::type_info> built = build_everything_by_hand();
        REQUIRE(built.has_value());

        reflect_test::everything object = {};
        object.text = "runtime";
        object.counts = {4, 5, 6};
        json::writer manualOut = {};
        REQUIRE(to_json(*built, &object, manualOut).has_value());
        json::writer macroOut = {};
        REQUIRE(to_json(object, macroOut).has_value());
        const std::string manualText = manualOut.finish();
        CHECK(manualText == macroOut.finish());

        reflect_test::everything restored = {};
        const result<json::document> document = json::document::parse(manualText);
        REQUIRE(document.has_value());
        REQUIRE(from_json(*built, &restored, document->root()).has_value());
        CHECK(restored == object);
    }

    TEST_CASE("reflect - the macros describe types declared in another translation unit") {
        const reflect::type_info *info = type_info_of<wrapper>();
        REQUIRE(info != nullptr);
        CHECK(info->name == "test.wrapper");
        REQUIRE(info->fields.size() == 2);
        CHECK(info->fields[0].kind == field_kind::STRUCT);
        CHECK(info->fields[0].structType == type_info_of<reflect_test::inner>());
        CHECK(info->fields[0].offset == offsetof(wrapper, body));
        CHECK(info->fields[1].kind == field_kind::ENUM);
        CHECK(info->fields[1].enumType == enum_info_of<local_mode>());
        CHECK(info->fields[1].offset == offsetof(wrapper, mode));
        CHECK(info->fields[1].defaultValueJson == "\"TWO\"");
        CHECK(info->isTriviallyCopyable);
    }

    TEST_CASE("reflect - the enum macro records every enumerator with its value and width") {
        const enum_info weapon = describe_enum<reflect_test::weapon_kind>();
        CHECK(weapon.name == "test.weapon_kind");
        CHECK(weapon.type == type_id_of<reflect_test::weapon_kind>());
        CHECK(weapon.underlyingSize == 1);
        CHECK_FALSE(weapon.isSigned);
        CHECK_FALSE(weapon.isFlags);
        REQUIRE(weapon.values.size() == 3);
        CHECK(weapon.values[0] == std::pair<std::string, i64>("SWORD", 0));
        CHECK(weapon.values[1] == std::pair<std::string, i64>("BOW", 5));
        CHECK(weapon.values[2] == std::pair<std::string, i64>("STAFF", 6));

        const enum_info mode = describe_enum<reflect_test::signed_mode>();
        CHECK(mode.underlyingSize == 2);
        CHECK(mode.isSigned);
        CHECK(mode.values[0].second == -2);

        const enum_info crowded = describe_enum<crowd>();
        REQUIRE(crowded.values.size() == 64);
        CHECK(crowded.values[0].first == "A0");
        CHECK(crowded.values[63].first == "A63");
        CHECK(crowded.values[63].second == 63);
        CHECK(crowded.underlyingSize == 2);
    }

    TEST_CASE("reflect - flag sets are recognised by the explicit macro or the _flags suffix") {
        CHECK(describe_enum<reflect_test::body_flags>().isFlags);
        CHECK(describe_enum<reflect_test::layer_mask>().isFlags);
        CHECK_FALSE(describe_enum<reflect_test::weapon_kind>().isFlags);
        CHECK_FALSE(describe_enum<local_mode>().isFlags);
    }

    TEST_CASE("reflect - describe_struct returns a fresh copy each time while type_info_of is stable") {
        const reflect::type_info first = describe_struct<reflect_test::inner>();
        const reflect::type_info second = describe_struct<reflect_test::inner>();
        CHECK(&first != &second);
        CHECK(first.has_same_layout(second));
        CHECK(first.fields[0].defaultValueJson == second.fields[0].defaultValueJson);
        CHECK(type_info_of<reflect_test::inner>() == type_info_of<reflect_test::inner>());
        CHECK(type_info_of<reflect_test::inner>()->has_same_layout(first));
    }

    TEST_CASE("reflect - reflect_traits of macro types expose NAME, KIND and describe") {
        static_assert(reflect_traits<reflect_test::inner>::NAME == "test.inner");
        static_assert(reflect_traits<reflect_test::inner>::KIND == field_kind::STRUCT);
        static_assert(reflect_traits<reflect_test::weapon_kind>::KIND == field_kind::ENUM);
        static_assert(reflected_struct<reflect_test::inner>);
        static_assert(reflected_struct<const reflect_test::everything &>);
        static_assert(!reflected_struct<reflect_test::weapon_kind>);
        static_assert(!reflected_struct<f32>);
        static_assert(!reflected_struct<reflect_test::not_reflected>);
        static_assert(reflected_enum<reflect_test::weapon_kind>);
        static_assert(reflected_enum<local_mode>);
        static_assert(!reflected_enum<reflect_test::inner>);
        static_assert(!reflected_enum<int>);
        CHECK(true);
    }

    TEST_CASE("reflect - the umbrella header exposes the whole area") {
        // Compiles only if reflect.h pulls in every public header.
        const type_id id = type_id_of<reflect_test::inner>();
        const field_kind kind = kind_of<reflect_test::inner>();
        const reflect::type_info *info = type_info_of<reflect_test::inner>();
        REQUIRE(info != nullptr);
        reflect_test::inner object = {};
        const result<json::document> document = to_json_document(object);
        const result<field_ref> ref = resolve_path(*info, &object, "weight");

        CHECK(id == info->type);
        CHECK(kind == field_kind::STRUCT);
        CHECK(document.has_value());
        CHECK(ref.has_value());
    }
}
