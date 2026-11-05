#include "conjure/testing/doctest.h"

#include "reflect_test_types.h"

#include "conjure/core/hash.h"
#include "conjure/core/json/document.h"
#include "conjure/core/reflect/json_conversion.h"
#include "conjure/core/reflect/reflect.h"
#include "conjure/core/reflect/type_info.h"
#include "conjure/core/reflect/type_registry.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::reflect;

namespace {
    // Fields declared in non-alphabetical order: the writer emits them in this order, the recorded
    // default must still be canonical (sorted).
    struct unsorted {
        f32 zeta = 1.0f;
        f32 alpha = 2.0f;
    };

    struct unsorted_holder {
        unsorted body = {};
    };

    // Members typed with the platform integers rather than the fixed-width aliases.
    struct platform_ints {
        long plain = 0;
        unsigned long long wide = 0;
        usize count = 0;
        isize delta = 0;
    };
}

CONJURE_REFLECT_STRUCT(unsorted, "test.unsorted", CONJURE_FIELD(zeta), CONJURE_FIELD(alpha));
CONJURE_REFLECT_STRUCT(unsorted_holder, "test.unsorted_holder", CONJURE_FIELD(body));
CONJURE_REFLECT_STRUCT(platform_ints, "test.platform_ints",
    CONJURE_FIELD(plain),
    CONJURE_FIELD(wide),
    CONJURE_FIELD(count),
    CONJURE_FIELD(delta));

namespace {
    // Hand-written layout of reflect_test::inner, as a dynamic (C#) component would describe it.
    reflect::type_info make_inner_layout() {
        reflect::type_info info = {};
        info.name = "test.manual_inner";
        info.type = type_id::from_name(info.name);
        info.size = 16;
        info.alignment = 4;

        field_info offset = {};
        offset.name = "offset";
        offset.kind = field_kind::VEC3;
        offset.type = type_id_of<math::vec3>();
        offset.offset = 0;
        offset.size = 12;

        field_info weight = {};
        weight.name = "weight";
        weight.kind = field_kind::F32;
        weight.type = type_id_of<f32>();
        weight.offset = 12;
        weight.size = 4;

        info.fields = {offset, weight};
        return info;
    }

    field_info make_field(std::string name, field_kind kind, u32 offset, u32 size) {
        field_info field = {};
        field.name = std::move(name);
        field.kind = kind;
        field.offset = offset;
        field.size = size;
        return field;
    }

    void check_field(const reflect::type_info &type, std::string_view name, field_kind kind, usize offset, usize size) {
        const field_info *field = type.find_field(name);
        REQUIRE_MESSAGE(field != nullptr, name);
        CHECK_MESSAGE(field->kind == kind, name);
        CHECK_MESSAGE(field->offset == offset, name);
        CHECK_MESSAGE(field->size == size, name);
        if (kind != field_kind::ARRAY) {
            CHECK_MESSAGE(field->arrayCount == 0, name);
            CHECK_MESSAGE(field->elementKind == field_kind::COUNT, name);
        }
    }
}

TEST_SUITE("core/type_info") {

    TEST_CASE("type_info - a macro description carries name, id, size and alignment") {
        const reflect::type_info info = describe_struct<reflect_test::inner>();
        CHECK(info.name == "test.inner");
        CHECK(info.type == type_id_of<reflect_test::inner>());
        CHECK(info.size == sizeof(reflect_test::inner));
        CHECK(info.alignment == alignof(reflect_test::inner));
        CHECK(info.isTriviallyCopyable);
        REQUIRE(info.fields.size() == 2);
        CHECK(info.fields[0].name == "offset");
        CHECK(info.fields[1].name == "weight");
    }

    TEST_CASE("type_info - field offsets and sizes match offsetof and sizeof for every kind") {
        using reflect_test::everything;
        const reflect::type_info info = describe_struct<everything>();
        REQUIRE(info.fields.size() == 29);
        CHECK(info.size == sizeof(everything));
        CHECK(info.alignment == alignof(everything));
        CHECK_FALSE(info.isTriviallyCopyable);

        check_field(info, "flag", field_kind::BOOL, offsetof(everything, flag), sizeof(bool));
        check_field(info, "tiny", field_kind::I8, offsetof(everything, tiny), 1);
        check_field(info, "small", field_kind::I16, offsetof(everything, small), 2);
        check_field(info, "medium", field_kind::I32, offsetof(everything, medium), 4);
        check_field(info, "large", field_kind::I64, offsetof(everything, large), 8);
        check_field(info, "utiny", field_kind::U8, offsetof(everything, utiny), 1);
        check_field(info, "usmall", field_kind::U16, offsetof(everything, usmall), 2);
        check_field(info, "umedium", field_kind::U32, offsetof(everything, umedium), 4);
        check_field(info, "ularge", field_kind::U64, offsetof(everything, ularge), 8);
        check_field(info, "single", field_kind::F32, offsetof(everything, single), 4);
        check_field(info, "real", field_kind::F64, offsetof(everything, real), 8);
        check_field(info, "text", field_kind::STRING, offsetof(everything, text), sizeof(std::string));
        check_field(info, "id", field_kind::STRING_ID, offsetof(everything, id), 8);
        check_field(info, "guid", field_kind::UUID, offsetof(everything, guid), 16);
        check_field(info, "v2", field_kind::VEC2, offsetof(everything, v2), 8);
        check_field(info, "v3", field_kind::VEC3, offsetof(everything, v3), 12);
        check_field(info, "v4", field_kind::VEC4, offsetof(everything, v4), 16);
        check_field(info, "rot", field_kind::QUAT, offsetof(everything, rot), 16);
        check_field(info, "matrix", field_kind::MAT4, offsetof(everything, matrix), 64);
        check_field(info, "pose", field_kind::TRANSFORM, offsetof(everything, pose), 40);
        check_field(info, "weapon", field_kind::ENUM, offsetof(everything, weapon), 1);
        check_field(info, "body", field_kind::ENUM, offsetof(everything, body), 4);
        check_field(info, "mode", field_kind::ENUM, offsetof(everything, mode), 2);
        check_field(info, "nested", field_kind::STRUCT, offsetof(everything, nested), sizeof(reflect_test::inner));
        check_field(info, "counts", field_kind::ARRAY, offsetof(everything, counts), 12);
        check_field(info, "parts", field_kind::ARRAY, offsetof(everything, parts), 2 * sizeof(reflect_test::inner));
        check_field(info, "weapons", field_kind::ARRAY, offsetof(everything, weapons), 2);
        check_field(info, "entity", field_kind::ENTITY, offsetof(everything, entity), 8);
        check_field(info, "asset", field_kind::ASSET_REF, offsetof(everything, asset), 16);
    }

    TEST_CASE("type_info - fields record the id of their value type") {
        const reflect::type_info info = describe_struct<reflect_test::everything>();
        CHECK(info.find_field("single")->type == type_id_of<f32>());
        CHECK(info.find_field("text")->type == type_id_of<std::string>());
        CHECK(info.find_field("pose")->type == type_id_of<math::transform>());
        CHECK(info.find_field("weapon")->type == type_id_of<reflect_test::weapon_kind>());
        CHECK(info.find_field("nested")->type == type_id_of<reflect_test::inner>());
        CHECK(info.find_field("counts")->type == type_id_of<i32>());
        CHECK(info.find_field("parts")->type == type_id_of<reflect_test::inner>());
        CHECK(info.find_field("weapons")->type == type_id_of<reflect_test::weapon_kind>());
        CHECK(info.find_field("entity")->type == type_id::from_name("test.entity"));
        CHECK(info.find_field("asset")->type == type_id::from_name("test.asset_ref"));
    }

    TEST_CASE("type_info - array fields describe their element kind and count") {
        const reflect::type_info info = describe_struct<reflect_test::everything>();

        const field_info *counts = info.find_field("counts");
        REQUIRE(counts != nullptr);
        CHECK(counts->is_array());
        CHECK(counts->arrayCount == 3);
        CHECK(counts->elementKind == field_kind::I32);
        CHECK(counts->element_size() == 4);
        CHECK(counts->structType == nullptr);

        const field_info *parts = info.find_field("parts");
        REQUIRE(parts != nullptr);
        CHECK(parts->arrayCount == 2);
        CHECK(parts->elementKind == field_kind::STRUCT);
        CHECK(parts->element_size() == sizeof(reflect_test::inner));
        CHECK(parts->structType == type_info_of<reflect_test::inner>());

        const field_info *weapons = info.find_field("weapons");
        REQUIRE(weapons != nullptr);
        CHECK(weapons->elementKind == field_kind::ENUM);
        CHECK(weapons->enumType == enum_info_of<reflect_test::weapon_kind>());
    }

    TEST_CASE("type_info - nested struct and enum fields point at the registered descriptions") {
        const reflect::type_info info = describe_struct<reflect_test::everything>();
        const field_info *nested = info.find_field("nested");
        REQUIRE(nested != nullptr);
        REQUIRE(nested->structType != nullptr);
        CHECK(nested->structType == type_info_of<reflect_test::inner>());
        CHECK(nested->structType == registry().find(type_id_of<reflect_test::inner>()));
        CHECK(nested->enumType == nullptr);

        const field_info *weapon = info.find_field("weapon");
        REQUIRE(weapon != nullptr);
        REQUIRE(weapon->enumType != nullptr);
        CHECK(weapon->enumType == enum_info_of<reflect_test::weapon_kind>());
        CHECK(weapon->enumType->name == "test.weapon_kind");
        CHECK(weapon->enumType->underlyingSize == 1);
        CHECK_FALSE(weapon->enumType->isSigned);
        CHECK(weapon->structType == nullptr);

        const field_info *mode = info.find_field("mode");
        REQUIRE(mode != nullptr);
        REQUIRE(mode->enumType != nullptr);
        CHECK(mode->enumType->isSigned);
        CHECK(mode->enumType->underlyingSize == 2);

        const field_info *body = info.find_field("body");
        REQUIRE(body != nullptr);
        REQUIRE(body->enumType != nullptr);
        CHECK(body->enumType->isFlags);
    }

    TEST_CASE("type_info - nested defaults are canonical even when the fields are declared out of order") {
        const reflect::type_info nested = describe_struct<unsorted>();
        REQUIRE(nested.fields.size() == 2);
        CHECK(nested.fields[0].name == "zeta");
        CHECK(nested.fields[0].defaultValueJson == "1.0");
        CHECK(nested.fields[1].defaultValueJson == "2.0");

        const reflect::type_info holder = describe_struct<unsorted_holder>();
        const field_info *body = holder.find_field("body");
        REQUIRE(body != nullptr);
        CHECK(body->defaultValueJson == R"({"alpha":2.0,"zeta":1.0})");

        const result<json::document> document = to_json_document(unsorted{});
        REQUIRE(document.has_value());
        CHECK(body->defaultValueJson == document->to_canonical_string());
        CHECK(document->to_string() == R"({"zeta":1.0,"alpha":2.0})");
    }

    TEST_CASE("type_info - platform integer members reflect as the fixed-width kind of their width") {
        constexpr field_kind LONG_KIND = sizeof(long) == 8 ? field_kind::I64 : field_kind::I32;
        static_assert(kind_of<long>() == LONG_KIND);
        static_assert(kind_of<unsigned long>() == (sizeof(long) == 8 ? field_kind::U64 : field_kind::U32));
        static_assert(kind_of<long long>() == field_kind::I64);
        static_assert(kind_of<unsigned long long>() == field_kind::U64);
        static_assert(kind_of<usize>() == field_kind::U64);
        static_assert(kind_of<isize>() == field_kind::I64);
        static_assert(type_id_of<long long>() == type_id_of<i64>());
        static_assert(type_id_of<usize>() == type_id_of<u64>());
        static_assert(reflectable<long> && reflectable<unsigned long> && reflectable<std::array<usize, 2>>);

        const reflect::type_info info = describe_struct<platform_ints>();
        REQUIRE(info.fields.size() == 4);
        CHECK(validate_layout(info).has_value());
        check_field(info, "plain", LONG_KIND, offsetof(platform_ints, plain), sizeof(long));
        check_field(info, "wide", field_kind::U64, offsetof(platform_ints, wide), 8);
        check_field(info, "count", field_kind::U64, offsetof(platform_ints, count), 8);
        check_field(info, "delta", field_kind::I64, offsetof(platform_ints, delta), 8);
        CHECK(info.find_field("plain")->type == (sizeof(long) == 8 ? type_id_of<i64>() : type_id_of<i32>()));
        CHECK(info.find_field("count")->type == type_id_of<u64>());
        CHECK(info.find_field("count")->defaultValueJson == "0");
    }

    TEST_CASE("type_info - default values are recorded as canonical JSON of the default-constructed instance") {
        const reflect::type_info inner = describe_struct<reflect_test::inner>();
        CHECK(inner.find_field("offset")->defaultValueJson == "[1.0,2.0,3.0]");
        CHECK(inner.find_field("weight")->defaultValueJson == "0.5");

        const reflect::type_info info = describe_struct<reflect_test::everything>();
        CHECK(info.find_field("flag")->defaultValueJson == "true");
        CHECK(info.find_field("tiny")->defaultValueJson == "-1");
        CHECK(info.find_field("ularge")->defaultValueJson == "4");
        CHECK(info.find_field("single")->defaultValueJson == "1.5");
        CHECK(info.find_field("text")->defaultValueJson == "\"hello\"");
        CHECK(info.find_field("id")->defaultValueJson == "\"#0000000000000000\"");
        CHECK(info.find_field("guid")->defaultValueJson == "\"01234567-89ab-cdef-fedc-ba9876543210\"");
        CHECK(info.find_field("rot")->defaultValueJson == "[0.0,0.0,0.0,1.0]");
        CHECK(info.find_field("weapon")->defaultValueJson == "\"BOW\"");
        CHECK(info.find_field("body")->defaultValueJson == "\"STATIC\"");
        CHECK(info.find_field("mode")->defaultValueJson == "\"NEGATIVE\"");
        CHECK(info.find_field("nested")->defaultValueJson == R"({"offset":[1.0,2.0,3.0],"weight":0.5})");
        CHECK(info.find_field("counts")->defaultValueJson == "[1,2,3]");
        CHECK(info.find_field("weapons")->defaultValueJson == R"(["SWORD","STAFF"])");
        CHECK(info.find_field("entity")->defaultValueJson == "0");
        CHECK(info.find_field("asset")->defaultValueJson == "\"00000000-0000-0000-0000-000000000000\"");
        CHECK(
          info.find_field("pose")->defaultValueJson ==
          R"({"position":[0.0,0.0,0.0],"rotation":[0.0,0.0,0.0,1.0],"scale":[1.0,1.0,1.0]})"
        );
    }

    TEST_CASE("type_info - a handle kind without a JSON representation has an empty default") {
        const reflect::type_info info = describe_struct<reflect_test::with_odd_handle>();
        REQUIRE(info.fields.size() == 2);
        CHECK(info.fields[0].kind == field_kind::ENTITY);
        CHECK(info.fields[0].size == 3);
        CHECK(info.fields[0].defaultValueJson.empty());
        CHECK(info.fields[1].defaultValueJson == "0");
    }

    TEST_CASE("type_info - field attributes come from the macro designated initialisers") {
        const reflect::type_info info = describe_struct<reflect_test::projectile>();
        REQUIRE(info.fields.size() == 4);

        const field_attributes &position = info.fields[0].attributes;
        CHECK_FALSE(position.min.has_value());
        CHECK(position.tooltip == "World position");
        CHECK(position.category == "Motion");
        CHECK_FALSE(position.hidden);

        const field_attributes &speed = info.fields[1].attributes;
        CHECK(speed.min == 0.0);
        CHECK(speed.max == 100.0);
        CHECK(speed.step == 0.5);
        CHECK(speed.tooltip == "Units per second");
        CHECK_FALSE(speed.hidden);
        CHECK_FALSE(speed.readOnly);

        CHECK(info.fields[2].attributes.hidden);
        CHECK_FALSE(info.fields[2].attributes.readOnly);
        CHECK(info.fields[3].attributes.readOnly);
        CHECK_FALSE(info.fields[3].attributes.hidden);
    }

    TEST_CASE("type_info - an empty struct describes with no fields") {
        const reflect::type_info info = describe_struct<reflect_test::empty_struct>();
        CHECK(info.name == "test.empty");
        CHECK(info.fields.empty());
        CHECK(info.size == sizeof(reflect_test::empty_struct));
        CHECK(info.layout_hash() == hash::FNV1A64_OFFSET_BASIS);
        CHECK(validate_layout(info).has_value());
    }

    TEST_CASE("type_info - find_field matches the exact member name") {
        const reflect::type_info info = describe_struct<reflect_test::inner>();
        REQUIRE(info.find_field("weight") != nullptr);
        CHECK(info.find_field("weight")->kind == field_kind::F32);
        CHECK(info.find_field("Weight") == nullptr);
        CHECK(info.find_field("") == nullptr);
        CHECK(info.find_field("weigh") == nullptr);
    }

    TEST_CASE("type_info - layout_hash matches the documented FNV-1a formula and the pinned vector") {
        const reflect::type_info info = describe_struct<reflect_test::inner>();

        // (name bytes, kind byte, offset u32 LE, size u32 LE) per field, from the offset basis.
        u64 expected = hash::FNV1A64_OFFSET_BASIS;
        expected = hash::fnv1a64("offset", expected);
        expected = hash::fnv1a64_step(expected, static_cast<u8>(field_kind::VEC3));
        for (const u8 byteValue : {u8(0), u8(0), u8(0), u8(0), u8(12), u8(0), u8(0), u8(0)}) {
            expected = hash::fnv1a64_step(expected, byteValue);
        }
        expected = hash::fnv1a64("weight", expected);
        expected = hash::fnv1a64_step(expected, static_cast<u8>(field_kind::F32));
        for (const u8 byteValue : {u8(12), u8(0), u8(0), u8(0), u8(4), u8(0), u8(0), u8(0)}) {
            expected = hash::fnv1a64_step(expected, byteValue);
        }
        CHECK(info.layout_hash() == expected);

        // The constant is shared with the C# mirror; changing it is a deliberate format break.
        CHECK(info.layout_hash() == 0xfe13925eca48303aull);
    }

    TEST_CASE("type_info - layout_hash ignores the type name and reacts to every field component") {
        const reflect::type_info reference = make_inner_layout();
        const u64 baseline = reference.layout_hash();
        CHECK(baseline == describe_struct<reflect_test::inner>().layout_hash());

        reflect::type_info renamed = reference;
        renamed.name = "another.name";
        renamed.type = type_id::from_name(renamed.name);
        CHECK(renamed.layout_hash() == baseline);
        CHECK(renamed.has_same_layout(reference));

        reflect::type_info fieldRenamed = reference;
        fieldRenamed.fields[0].name = "offsets";
        CHECK(fieldRenamed.layout_hash() != baseline);

        reflect::type_info kindChanged = reference;
        kindChanged.fields[1].kind = field_kind::U32;
        CHECK(kindChanged.layout_hash() != baseline);

        reflect::type_info offsetChanged = reference;
        offsetChanged.fields[1].offset = 8;
        CHECK(offsetChanged.layout_hash() != baseline);

        reflect::type_info sizeChanged = reference;
        sizeChanged.fields[1].size = 8;
        CHECK(sizeChanged.layout_hash() != baseline);

        reflect::type_info reordered = reference;
        std::swap(reordered.fields[0], reordered.fields[1]);
        CHECK(reordered.layout_hash() != baseline);

        reflect::type_info attributesChanged = reference;
        attributesChanged.fields[0].attributes.tooltip = "ignored by the hash";
        attributesChanged.fields[0].defaultValueJson = "[0,0,0]";
        CHECK(attributesChanged.layout_hash() == baseline);
    }

    TEST_CASE("type_info - has_same_layout also compares size and alignment") {
        const reflect::type_info reference = make_inner_layout();

        reflect::type_info bigger = reference;
        bigger.size = 32;
        CHECK(bigger.layout_hash() == reference.layout_hash());
        CHECK_FALSE(bigger.has_same_layout(reference));

        reflect::type_info stricter = reference;
        stricter.alignment = 16;
        stricter.size = 16;
        CHECK_FALSE(stricter.has_same_layout(reference));
    }

    TEST_CASE("type_info - validate_layout accepts a consistent description") {
        CHECK(validate_layout(make_inner_layout()).has_value());
        CHECK(validate_layout(describe_struct<reflect_test::everything>()).has_value());
        CHECK(validate_layout(describe_struct<reflect_test::projectile>()).has_value());
    }

    TEST_CASE("type_info - validate_layout rejects a broken type header") {
        reflect::type_info unnamed = make_inner_layout();
        unnamed.name.clear();
        const result<void> noName = validate_layout(unnamed);
        REQUIRE_FALSE(noName.has_value());
        CHECK(noName.get_error().generic_code() == error_code::INVALID_ARGUMENT);

        reflect::type_info badAlignment = make_inner_layout();
        badAlignment.alignment = 3;
        const result<void> alignment = validate_layout(badAlignment);
        REQUIRE_FALSE(alignment.has_value());
        CHECK(alignment.get_error().message().find("power of two") != std::string::npos);

        reflect::type_info zeroAlignment = make_inner_layout();
        zeroAlignment.alignment = 0;
        CHECK_FALSE(validate_layout(zeroAlignment).has_value());

        reflect::type_info oddSize = make_inner_layout();
        oddSize.size = 15;
        const result<void> size = validate_layout(oddSize);
        REQUIRE_FALSE(size.has_value());
        CHECK(size.get_error().message().find("multiple of alignment") != std::string::npos);
    }

    TEST_CASE("type_info - validate_layout rejects broken fields and names the offender") {
        reflect::type_info badName = make_inner_layout();
        badName.fields[0].name = "a.b";
        const result<void> name = validate_layout(badName);
        REQUIRE_FALSE(name.has_value());
        CHECK(name.get_error().message().find("a.b") != std::string::npos);
        badName.fields[0].name = "a[0]";
        CHECK_FALSE(validate_layout(badName).has_value());
        badName.fields[0].name = "has space";
        CHECK_FALSE(validate_layout(badName).has_value());
        badName.fields[0].name = "";
        CHECK_FALSE(validate_layout(badName).has_value());

        reflect::type_info noKind = make_inner_layout();
        noKind.fields[1].kind = field_kind::COUNT;
        const result<void> kind = validate_layout(noKind);
        REQUIRE_FALSE(kind.has_value());
        CHECK(kind.get_error().message().find("'weight'") != std::string::npos);
        CHECK(kind.get_error().message().find("test.manual_inner") != std::string::npos);

        reflect::type_info zeroSize = make_inner_layout();
        zeroSize.fields[1].size = 0;
        CHECK_FALSE(validate_layout(zeroSize).has_value());

        reflect::type_info outside = make_inner_layout();
        outside.fields[1].offset = 16;
        const result<void> fit = validate_layout(outside);
        REQUIRE_FALSE(fit.has_value());
        CHECK(fit.get_error().message().find("does not fit") != std::string::npos);

        reflect::type_info overlapping = make_inner_layout();
        overlapping.fields[1].offset = 8;
        const result<void> overlap = validate_layout(overlapping);
        REQUIRE_FALSE(overlap.has_value());
        CHECK(overlap.get_error().message().find("overlap") != std::string::npos);

        reflect::type_info duplicated = make_inner_layout();
        duplicated.fields[1].name = "offset";
        const result<void> duplicate = validate_layout(duplicated);
        REQUIRE_FALSE(duplicate.has_value());
        CHECK(duplicate.get_error().message().find("declared twice") != std::string::npos);

        reflect::type_info wrongSize = make_inner_layout();
        wrongSize.fields[1].size = 2;
        wrongSize.fields[1].offset = 12;
        const result<void> kindSize = validate_layout(wrongSize);
        REQUIRE_FALSE(kindSize.has_value());
        CHECK(kindSize.get_error().message().find("needs 4") != std::string::npos);

        reflect::type_info misaligned = {};
        misaligned.name = "test.misaligned";
        misaligned.size = 16;
        misaligned.alignment = 8;
        misaligned.fields = {make_field("pad", field_kind::U8, 0, 1), make_field("value", field_kind::F64, 1, 8)};
        const result<void> aligned = validate_layout(misaligned);
        REQUIRE_FALSE(aligned.has_value());
        CHECK(aligned.get_error().message().find("not aligned to 8") != std::string::npos);
    }

    TEST_CASE("type_info - validate_layout rejects a field stricter than the type's own alignment") {
        reflect::type_info weak = {};
        weak.name = "test.weakly_aligned";
        weak.size = 8;
        weak.alignment = 4;
        weak.fields = {make_field("value", field_kind::F64, 0, 8)};
        const result<void> real = validate_layout(weak);
        REQUIRE_FALSE(real.has_value());
        CHECK(real.get_error().generic_code() == error_code::INVALID_ARGUMENT);
        CHECK(real.get_error().message().find("'value'") != std::string::npos);
        const std::string &realMessage = real.get_error().message();
        CHECK(realMessage.find("requires alignment 8 but the type is aligned to 4") != std::string::npos);

        weak.alignment = 8;
        CHECK(validate_layout(weak).has_value());

        reflect::type_info text = {};
        text.name = "test.weak_string";
        text.size = static_cast<u32>(sizeof(std::string));
        text.alignment = 4;
        text.fields = {make_field("text", field_kind::STRING, 0, static_cast<u32>(sizeof(std::string)))};
        CHECK_FALSE(validate_layout(text).has_value());
        text.alignment = static_cast<u32>(alignof(std::string));
        CHECK(validate_layout(text).has_value());

        reflect::type_info handle = {};
        handle.name = "test.weak_handle";
        handle.size = 16;
        handle.alignment = 4;
        handle.fields = {make_field("asset", field_kind::ASSET_REF, 0, 16)};
        CHECK_FALSE(validate_layout(handle).has_value());
        handle.alignment = 8;
        CHECK(validate_layout(handle).has_value());
    }

    TEST_CASE("type_info - validate_layout requires descriptions for enum and struct fields") {
        reflect::type_info enumType = {};
        enumType.name = "test.enum_holder";
        enumType.size = 4;
        enumType.alignment = 4;
        enumType.fields = {make_field("weapon", field_kind::ENUM, 0, 1)};
        enumType.fields[0].type = type_id_of<reflect_test::weapon_kind>();
        const result<void> noEnum = validate_layout(enumType);
        REQUIRE_FALSE(noEnum.has_value());
        CHECK(noEnum.get_error().message().find("enum description") != std::string::npos);

        enumType.fields[0].enumType = enum_info_of<reflect_test::weapon_kind>();
        CHECK(validate_layout(enumType).has_value());
        enumType.fields[0].size = 2;
        CHECK_FALSE(validate_layout(enumType).has_value());

        reflect::type_info structType = {};
        structType.name = "test.struct_holder";
        structType.size = 16;
        structType.alignment = 4;
        structType.fields = {make_field("nested", field_kind::STRUCT, 0, 16)};
        structType.fields[0].type = type_id_of<reflect_test::inner>();
        const result<void> noStruct = validate_layout(structType);
        REQUIRE_FALSE(noStruct.has_value());
        CHECK(noStruct.get_error().message().find("type description") != std::string::npos);

        structType.fields[0].structType = type_info_of<reflect_test::inner>();
        CHECK(validate_layout(structType).has_value());
        structType.fields[0].size = 12;
        CHECK_FALSE(validate_layout(structType).has_value());
    }

    TEST_CASE("type_info - validate_layout requires field ids to match their struct and enum descriptions") {
        reflect::type_info structType = {};
        structType.name = "test.id_struct_holder";
        structType.size = 16;
        structType.alignment = 4;
        structType.fields = {make_field("nested", field_kind::STRUCT, 0, 16)};
        structType.fields[0].structType = type_info_of<reflect_test::inner>();
        structType.fields[0].type = type_id::from_name("test.something_else");
        const result<void> wrongStruct = validate_layout(structType);
        REQUIRE_FALSE(wrongStruct.has_value());
        CHECK(wrongStruct.get_error().generic_code() == error_code::INVALID_ARGUMENT);
        CHECK(wrongStruct.get_error().message().find("does not match its struct description") != std::string::npos);
        structType.fields[0].type = type_id::null();
        CHECK_FALSE(validate_layout(structType).has_value());
        structType.fields[0].type = type_id_of<reflect_test::inner>();
        CHECK(validate_layout(structType).has_value());

        reflect::type_info enumType = {};
        enumType.name = "test.id_enum_holder";
        enumType.size = 4;
        enumType.alignment = 4;
        enumType.fields = {make_field("weapon", field_kind::ENUM, 0, 1)};
        enumType.fields[0].enumType = enum_info_of<reflect_test::weapon_kind>();
        enumType.fields[0].type = type_id_of<reflect_test::signed_mode>();
        const result<void> wrongEnum = validate_layout(enumType);
        REQUIRE_FALSE(wrongEnum.has_value());
        CHECK(wrongEnum.get_error().message().find("does not match its enum description") != std::string::npos);
        enumType.fields[0].type = type_id_of<reflect_test::weapon_kind>();
        CHECK(validate_layout(enumType).has_value());

        // Arrays carry the element id.
        reflect::type_info arrays = {};
        arrays.name = "test.id_array_holder";
        arrays.size = 32;
        arrays.alignment = 4;
        field_info parts = make_field("parts", field_kind::ARRAY, 0, 32);
        parts.elementKind = field_kind::STRUCT;
        parts.arrayCount = 2;
        parts.structType = type_info_of<reflect_test::inner>();
        arrays.fields = {parts};
        CHECK_FALSE(validate_layout(arrays).has_value());
        arrays.fields[0].type = type_id_of<reflect_test::inner>();
        CHECK(validate_layout(arrays).has_value());
    }

    TEST_CASE("type_info - validate_layout rejects enum descriptions whose width is not 1, 2, 4 or 8") {
        for (const u32 width : {0u, 3u, 16u}) {
            enum_info odd = {};
            odd.name = "test.odd_width";
            odd.type = type_id::from_name(odd.name);
            odd.underlyingSize = width;
            odd.values = {{"A", 0}};

            reflect::type_info holder = {};
            holder.name = "test.odd_width_holder";
            holder.size = 16;
            holder.alignment = 8;
            holder.fields = {make_field("mode", field_kind::ENUM, 0, width == 0 ? 4u : width)};
            holder.fields[0].enumType = &odd;
            holder.fields[0].type = odd.type;

            const result<void> rejected = validate_layout(holder);
            REQUIRE_MESSAGE(!rejected.has_value(), width);
            CHECK(rejected.get_error().generic_code() == error_code::INVALID_ARGUMENT);
            CHECK(rejected.get_error().message().find("must be 1, 2, 4 or 8") != std::string::npos);
            CHECK(rejected.get_error().message().find("test.odd_width") != std::string::npos);
            CHECK(rejected.get_error().message().find("'mode'") != std::string::npos);

            // build_type_info is the dynamic-component entry point and must refuse the same.
            CHECK_FALSE(build_type_info("test.odd_width_holder", 16, 8, holder.fields).has_value());
        }
    }

    TEST_CASE("type_info - validate_layout checks array fields") {
        reflect::type_info arrays = {};
        arrays.name = "test.arrays";
        arrays.size = 16;
        arrays.alignment = 4;
        field_info counts = make_field("counts", field_kind::ARRAY, 0, 12);
        counts.elementKind = field_kind::I32;
        counts.arrayCount = 3;
        arrays.fields = {counts};
        CHECK(validate_layout(arrays).has_value());

        arrays.fields[0].arrayCount = 0;
        const result<void> noElements = validate_layout(arrays);
        REQUIRE_FALSE(noElements.has_value());
        CHECK(noElements.get_error().message().find("no elements") != std::string::npos);

        arrays.fields[0].arrayCount = 5;
        CHECK_FALSE(validate_layout(arrays).has_value());

        arrays.fields[0].arrayCount = 3;
        arrays.fields[0].elementKind = field_kind::ARRAY;
        CHECK_FALSE(validate_layout(arrays).has_value());

        arrays.fields[0].elementKind = field_kind::I16;
        CHECK_FALSE(validate_layout(arrays).has_value());

        arrays.fields[0].elementKind = field_kind::STRUCT;
        arrays.fields[0].size = 2 * sizeof(reflect_test::inner);
        arrays.fields[0].arrayCount = 2;
        arrays.fields[0].type = type_id_of<reflect_test::inner>();
        arrays.size = 32;
        CHECK_FALSE(validate_layout(arrays).has_value());
        arrays.fields[0].structType = type_info_of<reflect_test::inner>();
        CHECK(validate_layout(arrays).has_value());

        reflect::type_info notArray = make_inner_layout();
        notArray.fields[1].arrayCount = 2;
        const result<void> stray = validate_layout(notArray);
        REQUIRE_FALSE(stray.has_value());
        CHECK(stray.get_error().message().find("not an array") != std::string::npos);
    }

    TEST_CASE("type_info - validate_layout accepts handle kinds of any size and checks their alignment") {
        reflect::type_info handles = {};
        handles.name = "test.handles";
        handles.size = 32;
        handles.alignment = 8;
        handles.fields = {
            make_field("entity", field_kind::ENTITY, 0, 8),
            make_field("asset", field_kind::ASSET_REF, 8, 16),
            make_field("odd", field_kind::ENTITY, 24, 3),
        };
        CHECK(validate_layout(handles).has_value());

        handles.fields[0].offset = 4;
        handles.fields[0].size = 4;
        CHECK(validate_layout(handles).has_value());
        handles.fields[0].offset = 2;
        CHECK_FALSE(validate_layout(handles).has_value());
    }

    TEST_CASE("type_info - build_type_info assembles and validates a runtime description") {
        const reflect::type_info reference = make_inner_layout();
        const result<reflect::type_info> built = build_type_info("test.manual_inner", 16, 4, reference.fields);
        REQUIRE(built.has_value());
        CHECK(built->name == "test.manual_inner");
        CHECK(built->type == type_id::from_name("test.manual_inner"));
        CHECK(built->size == 16);
        CHECK(built->alignment == 4);
        CHECK(built->isTriviallyCopyable);
        REQUIRE(built->fields.size() == 2);
        CHECK(built->fields[1].name == "weight");
        CHECK(built->has_same_layout(reference));
        CHECK(built->has_same_layout(describe_struct<reflect_test::inner>()));
    }

    TEST_CASE("type_info - build_type_info derives trivial copyability from the fields") {
        const field_info text = make_field("text", field_kind::STRING, 0, static_cast<u32>(sizeof(std::string)));
        const result<reflect::type_info> withString = build_type_info(
          "test.with_string",
          static_cast<u32>(sizeof(std::string)),
          static_cast<u32>(alignof(std::string)),
          std::span<const field_info>(&text, 1)
        );
        REQUIRE(withString.has_value());
        CHECK_FALSE(withString->isTriviallyCopyable);

        constexpr u32 EVERYTHING_SIZE = static_cast<u32>(sizeof(reflect_test::everything));
        field_info nested = make_field("nested", field_kind::STRUCT, 0, EVERYTHING_SIZE);
        nested.structType = type_info_of<reflect_test::everything>();
        nested.type = type_id_of<reflect_test::everything>();
        const result<reflect::type_info> withNested = build_type_info(
          "test.with_everything",
          static_cast<u32>(sizeof(reflect_test::everything)),
          static_cast<u32>(alignof(reflect_test::everything)),
          std::span<const field_info>(&nested, 1)
        );
        REQUIRE(withNested.has_value());
        CHECK_FALSE(withNested->isTriviallyCopyable);

        field_info trivial = make_field("nested", field_kind::STRUCT, 0, static_cast<u32>(sizeof(reflect_test::inner)));
        trivial.structType = type_info_of<reflect_test::inner>();
        trivial.type = type_id_of<reflect_test::inner>();
        const result<reflect::type_info> withTrivial = build_type_info(
          "test.with_inner",
          static_cast<u32>(sizeof(reflect_test::inner)),
          4,
          std::span<const field_info>(&trivial, 1)
        );
        REQUIRE(withTrivial.has_value());
        CHECK(withTrivial->isTriviallyCopyable);
    }

    TEST_CASE("type_info - build_type_info reports an inconsistent layout") {
        const reflect::type_info reference = make_inner_layout();
        const result<reflect::type_info> tooSmall = build_type_info("test.small", 8, 4, reference.fields);
        REQUIRE_FALSE(tooSmall.has_value());
        CHECK(tooSmall.get_error().generic_code() == error_code::INVALID_ARGUMENT);
        CHECK(tooSmall.get_error().message().find("does not fit") != std::string::npos);

        const result<reflect::type_info> unnamed = build_type_info("", 16, 4, reference.fields);
        REQUIRE_FALSE(unnamed.has_value());
        CHECK(unnamed.get_error().generic_code() == error_code::INVALID_ARGUMENT);

        const result<reflect::type_info> empty = build_type_info("test.nothing", 0, 1, {});
        REQUIRE(empty.has_value());
        CHECK(empty->fields.empty());
        CHECK(empty->layout_hash() == hash::FNV1A64_OFFSET_BASIS);
    }
}
