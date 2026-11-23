#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"
#include "scripting_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/scripting/script_class_registry.h"
#include "conjure/scripting/script_component.h"
#include "conjure/scripting/script_fields.h"
#include "conjure/scripting/script_runtime.h"
#include "conjure/scripting/scripting_error.h"

#include <array>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;
using namespace scripting_test;

namespace {

    constexpr std::string_view INSPECTOR_PROBE = "Conjure.TestScripts.InspectorProbe";
    constexpr std::string_view FIELD_KINDS = "Conjure.TestScripts.FieldKinds";
    constexpr std::string_view WIDE_ENUM_FIELDS = "Conjure.TestScripts.WideEnumFields";
    constexpr std::string_view NON_FINITE_DEFAULTS = "Conjure.TestScripts.NonFiniteDefaults";
    constexpr std::string_view FIELD_META_PROBE = "Conjure.TestScripts.FieldMetaProbe";

    /// <summary>
    /// One constant of an enum field's meta: its name and the int32 enumValues must hold beside it.
    /// </summary>
    struct enum_constant {
        std::string_view name = {};
        i64 value = 0;
    };

    /// <summary>
    /// Checks that 'meta' lists exactly 'expected' in enumNames, each with its value at the same index of
    /// enumValues (the order is the managed one, so each name is looked up).
    /// </summary>
    void check_enum_meta(const core::json::value &meta, std::span<const enum_constant> expected) {
        const core::json::value names = meta["enumNames"];
        const core::json::value values = meta["enumValues"];
        REQUIRE(names.size() == expected.size());
        REQUIRE(values.size() == expected.size());
        for (const enum_constant &constant : expected) {
            CAPTURE(constant.name);
            usize found = names.size();
            for (usize index = 0; index < names.size(); ++index) {
                if (names[index].as_string() == constant.name) {
                    found = index;
                }
            }
            REQUIRE(found < names.size());
            CHECK(values[found].as_i64() == constant.value);
        }
    }
}

TEST_SUITE("scripting/script_fields_inspector") {

    TEST_CASE("inspector - set_field on a live instance is visible through collect [dotnet]" * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const script_entry_id probe = fixture.add_script(fixture.create_entity(), INSPECTOR_PROBE);
        CHECK(fixture.field_f64(probe, "Speed") == doctest::Approx(3.0));
        CONJURE_REQUIRE_OK(fixture.runtime.set_field(probe, hash_of("Speed"), json("7.5").root()));
        CHECK(fixture.field_f64(probe, "Speed") == doctest::Approx(7.5));
        CONJURE_REQUIRE_OK(fixture.runtime.set_field(probe, hash_of("Current"), json("2").root()));
        CHECK(fixture.field_i64(probe, "Current") == 2);
        fixture.run_frame();
        CHECK(fixture.field_f64(probe, "Speed") == doctest::Approx(7.5));
        CHECK(fixture.field_i64(probe, "Updates") == 1);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "inspector - an unknown field is an error and the instance is untouched [dotnet]" * doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        dotnet_fixture fixture = {};
        const script_entry_id probe = fixture.add_script(fixture.create_entity(), INSPECTOR_PROBE);
        CONJURE_CHECK_ERROR(
          fixture.runtime.set_field(probe, hash_of("Nope"), json("1").root()),
          script_error::FIELD_NOT_FOUND
        );
        CONJURE_CHECK_ERROR(
          fixture.runtime.set_field(probe, hash_of("Speed"), json("\"fast\"").root()),
          core::error_code::INVALID_ARGUMENT
        );
        CHECK(fixture.field_f64(probe, "Speed") == doctest::Approx(3.0));
    }

    TEST_CASE(
      "inspector - mismatched initial fields fall back to defaults with a warning [dotnet]" * doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const script_entry_id kinds = fixture.add_script(
          fixture.create_entity(),
          FIELD_KINDS,
          R"({"Count": "seven", "Label": 5, "Ratio": 2.5, "Ints": [4, 5]})"
        );
        CHECK(fixture.field_i64(kinds, "Count") == 7);                 // default kept
        CHECK(fixture.field_string(kinds, "Label") == "hello");
        CHECK(fixture.field_f64(kinds, "Ratio") == doctest::Approx(2.5));
        CHECK(fixture.fields_of(kinds).root()["Ints"].size() == 2);
        CHECK(fixture.fields_of(kinds).root()["Ints"][1].as_i64() == 5);
        CHECK(logs.count(core::log_level::WARN) == 2);
        CHECK(logs.contains(core::log_level::WARN, "Count"));
        CHECK(logs.contains(core::log_level::WARN, "Label"));
    }

    TEST_CASE("inspector - every field kind round trips through the live instance [dotnet]" * doctest::timeout(60)) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const ecs::entity link = fixture.create_entity();
        core::json::document initial = core::json::document::object();
        initial.set("Flag", false);
        initial.set("Big", static_cast<i64>(-5));
        initial.set("Precise", 0.125);
        initial.set("Link", link.to_u64());
        initial.set("Texture", "01234567-89ab-cdef-0123-456789abcdef");
        core::json::document names = core::json::document::array();
        names.push_back("x");
        names.push_back("yz");
        initial.set("Names", names);
        core::json::document links = core::json::document::array();
        links.push_back(link.to_u64());
        initial.set("Links", links);
        core::json::document quat = core::json::document::array();
        quat.push_back(0.0f);
        quat.push_back(0.0f);
        quat.push_back(1.0f);
        quat.push_back(0.0f);
        initial.set("Orientation", quat);

        const script_entry_id kinds = fixture.runtime.add_script(
          fixture.create_entity(),
          hash_of(FIELD_KINDS),
          initial.root()
        );
        REQUIRE(kinds.is_valid());
        fixture.runtime.flush_pending();
        const core::json::document collected = fixture.fields_of(kinds);
        const core::json::value root = collected.root();
        CHECK(root["Flag"].as_bool(true) == false);
        CHECK(root["Count"].as_i64() == 7);
        CHECK(root["Big"].as_i64() == -5);
        CHECK(root["Ratio"].as_f64() == doctest::Approx(1.5));
        CHECK(root["Precise"].as_f64() == doctest::Approx(0.125));
        CHECK(root["Label"].as_string() == "hello");
        CHECK(root["Planar"][1].as_f64() == doctest::Approx(2.0));
        CHECK(root["Spatial"][2].as_f64() == doctest::Approx(3.0));
        CHECK(root["Homogeneous"][3].as_f64() == doctest::Approx(4.0));
        CHECK(root["Orientation"][2].as_f64() == doctest::Approx(1.0));
        CHECK(root["Tint"][1].as_f64() == doctest::Approx(0.5));
        CHECK(root["Link"].as_u64() == link.to_u64());
        CHECK(root["Texture"].as_string() == "01234567-89ab-cdef-0123-456789abcdef");
        CHECK(root["Current"].as_i64() == 1);
        REQUIRE(root["Ints"].size() == 3);
        CHECK(root["Floats"][1].as_f64() == doctest::Approx(1.5));
        REQUIRE(root["Names"].size() == 2);
        CHECK(root["Names"][1].as_string() == "yz");
        REQUIRE(root["Links"].size() == 1);
        CHECK(root["Links"][0].as_u64() == link.to_u64());
        CHECK(root["Hidden"].as_i64() == 9);
        CHECK(logs.count(core::log_level::WARN) == 0);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "inspector - range, tooltip, hidden and enum names arrive in the field meta [dotnet]" * doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        dotnet_fixture fixture("v1", false);
        const script_class_info *probe = fixture.runtime.classes().find(INSPECTOR_PROBE);
        REQUIRE(probe != nullptr);
        const script_field_info *speed = probe->find_field("Speed");
        REQUIRE(speed != nullptr);
        CHECK(speed->meta.root()["min"].as_f64() == doctest::Approx(0.0));
        CHECK(speed->meta.root()["max"].as_f64() == doctest::Approx(10.0));
        CHECK(speed->meta.root()["tooltip"].as_string() == "Speed in metres per second");
        CHECK_FALSE(speed->meta.root().contains("hidden"));
        const script_field_info *current = probe->find_field("Current");
        REQUIRE(current != nullptr);
        CHECK(current->kind == script_field_kind::ENUM);
        const core::json::value names = current->meta.root()["enumNames"];
        REQUIRE(names.size() == 3);
        CHECK(names[0].as_string() == "Idle");
        CHECK(names[1].as_string() == "Run");
        CHECK(names[2].as_string() == "Jump");
        CHECK(current->defaultValue.root().as_i64() == 1);
        const script_field_info *internal = probe->find_field("Internal");
        REQUIRE(internal != nullptr);
        CHECK(internal->meta.root()["hidden"].as_bool() == true);
        CHECK(probe->find_field("Updates")->meta.root().size() == 0);
    }

    TEST_CASE(
      "regression - review 15 a list blob with an absurd count is rejected before allocating [dotnet]" *
      doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const script_entry_id kinds = fixture.add_script(fixture.create_entity(), FIELD_KINDS);
        const script_entry *entryValue = fixture.runtime.entry(kinds);
        REQUIRE(entryValue != nullptr);
        REQUIRE(entryValue->is_live());

        // {Ints: LIST<INT32>, count = 0x7FFFFFFF, no elements} and {Names: LIST<STRING>, count = 0x40000000}.
        std::vector<u8> hostile(64);
        field_blob_writer writer(hostile);
        writer.begin();
        writer.begin_field(hash_of("Ints"), script_field_kind::LIST, script_field_kind::INT32);
        writer.write_u32(0x7FFFFFFFu);
        writer.end_field();
        writer.begin_field(hash_of("Names"), script_field_kind::LIST, script_field_kind::STRING);
        writer.write_u32(0x40000000u);
        writer.end_field();
        const usize size = writer.finish();
        REQUIRE(size <= hostile.size());
        const i32 rejected = fixture.runtime.host().managed_api().apply_fields(
          entryValue->managedSlot,
          hostile.data(),
          static_cast<i32>(size)
        );
        CHECK(rejected == 2);                                           // both fields counted as not applied
        CHECK(fixture.fields_of(kinds).root()["Ints"].size() == 3);     // defaults kept
        CHECK(fixture.fields_of(kinds).root()["Names"].size() == 2);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F671: 64-bit enums are skipped and a uint enum default fits its blob [dotnet]" *
      doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        // Without the fix the ulong default overflowed while the catalog was built and the whole assembly failed.
        const script_class_info *wide = fixture.runtime.classes().find(WIDE_ENUM_FIELDS);
        REQUIRE(wide != nullptr);
        CHECK(wide->find_field("Mask") == nullptr);
        CHECK(wide->find_field("Long") == nullptr);
        const script_field_info *highUnsigned = wide->find_field("Unsigned");
        REQUIRE(highUnsigned != nullptr);
        CHECK(highUnsigned->kind == script_field_kind::ENUM);
        CHECK(highUnsigned->defaultValue.root().as_i64() == -1);       // 0xFFFFFFFF as the int32 the blob carries
        CHECK(logs.contains(core::log_level::INFO, "WideEnumFields.Mask"));
        CHECK(logs.contains(core::log_level::INFO, "WideEnumFields.Long"));

        const script_entry_id entry = fixture.add_script(fixture.create_entity(), WIDE_ENUM_FIELDS);
        CHECK(fixture.field_i64(entry, "Unsigned") == -1);
        CHECK(fixture.field_i64(entry, "Plain") == 3);
        CHECK(logs.count(core::log_level::WARN) == 0);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F672: infinite defaults and range bounds reach native as finite JSON [dotnet]" *
      doctest::timeout(60)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const script_class_info *probe = fixture.runtime.classes().find(NON_FINITE_DEFAULTS);
        REQUIRE(probe != nullptr);
        const script_field_info *maxRange = probe->find_field("MaxRange");
        REQUIRE(maxRange != nullptr);
        CHECK(maxRange->defaultValue.root().as_f64() == doctest::Approx(std::numeric_limits<f32>::max()));
        const script_field_info *floorField = probe->find_field("Floor");
        REQUIRE(floorField != nullptr);
        CHECK(floorField->defaultValue.root().as_f64() == -std::numeric_limits<f64>::max());
        const script_field_info *reach = probe->find_field("Reach");
        REQUIRE(reach != nullptr);
        CHECK(reach->meta.root()["min"].as_f64(-1.0) == doctest::Approx(0.0));
        CHECK_FALSE(reach->meta.root().contains("max"));
        CHECK(logs.contains(core::log_level::INFO, "NonFiniteDefaults.MaxRange"));
        CHECK(logs.contains(core::log_level::INFO, "NonFiniteDefaults.Floor"));

        // A field missing from the entry JSON starts from the registered default: the float max, not 0.
        const script_entry_id entry = fixture.add_script(fixture.create_entity(), NON_FINITE_DEFAULTS);
        CHECK(fixture.field_f64(entry, "MaxRange") == doctest::Approx(std::numeric_limits<f32>::max()));
        CHECK(fixture.field_f64(entry, "Floor") == -std::numeric_limits<f64>::max());
        CHECK(logs.count(core::log_level::WARN) == 0);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }

    TEST_CASE(
      "regression - sweep-2026-11-12 F673: assetType is the registered name and enumValues the blob int32 [dotnet]" *
      doctest::timeout(120)
    ) {
        CONJURE_TEST_REQUIRE(precondition::DOTNET_RUNTIME);
        log_capture logs = {};
        dotnet_fixture fixture = {};
        const script_class_info *probe = fixture.runtime.classes().find(FIELD_META_PROBE);
        REQUIRE(probe != nullptr);

        SUBCASE("asset references") {
            // Without the fix assetType was the C# tag name ("AudioClipAsset"), and present for untyped references.
            const script_field_info *clip = probe->find_field("Clip");
            REQUIRE(clip != nullptr);
            CHECK(clip->kind == script_field_kind::ASSET);
            CHECK(clip->meta.root()["assetType"].as_string() == "conjure.audio.clip");
            CHECK(clip->meta.root().size() == 1);
            const script_field_info *prefabs = probe->find_field("Prefabs");
            REQUIRE(prefabs != nullptr);
            CHECK(prefabs->elementKind == script_field_kind::ASSET);
            CHECK(prefabs->meta.root()["assetType"].as_string() == "conjure.scene.prefab");
            const script_field_info *untyped = probe->find_field("Untyped");
            REQUIRE(untyped != nullptr);
            CHECK(untyped->kind == script_field_kind::ASSET);
            CHECK(untyped->meta.root().size() == 0);
            // A tag whose TypeName throws keeps its field and the rest of the catalog; only the key is left out.
            const script_field_info *broken = probe->find_field("Broken");
            REQUIRE(broken != nullptr);
            CHECK(broken->kind == script_field_kind::ASSET);
            CHECK(broken->meta.root().size() == 0);
            CHECK(logs.contains(core::log_level::INFO, "FieldMetaProbe.Broken"));
            CHECK(logs.contains(core::log_level::INFO, "no registered name"));
        }

        SUBCASE("enums") {
            // Without the fix only enumNames was sent: a sparse enum's values were the positions 0, 1, 2.
            const script_field_info *code = probe->find_field("Code");
            REQUIRE(code != nullptr);
            CHECK(code->kind == script_field_kind::ENUM);
            constexpr std::array<enum_constant, 4> SPARSE = {
              {{"Negative", -5}, {"Ten", 10}, {"Hundred", 100}, {"Tenfold", 10}}
            };
            check_enum_meta(code->meta.root(), SPARSE);
            CHECK_FALSE(code->meta.root().contains("enumFlags"));
            CHECK(code->defaultValue.root().as_i64() == 100);

            const script_field_info *terrain = probe->find_field("Terrain");
            REQUIRE(terrain != nullptr);
            constexpr std::array<enum_constant, 4> TERRAIN = {{{"None", 0}, {"Ground", 1}, {"Water", 2}, {"Air", 4}}};
            check_enum_meta(terrain->meta.root(), TERRAIN);
            CHECK(terrain->meta.root()["enumFlags"].as_bool() == true);
            CHECK(terrain->defaultValue.root().as_i64() == 5);           // Ground | Air: the value matches the flags

            // A uint constant above int32 is the int32 the blob carries, the same as the field's default.
            const script_field_info *highUnsigned = probe->find_field("Unsigned");
            REQUIRE(highUnsigned != nullptr);
            constexpr std::array<enum_constant, 2> UNSIGNED = {{{"Low", 1}, {"High", -1}}};
            check_enum_meta(highUnsigned->meta.root(), UNSIGNED);
            CHECK(highUnsigned->defaultValue.root().as_i64() == -1);

            const script_field_info *codes = probe->find_field("Codes");
            REQUIRE(codes != nullptr);
            CHECK(codes->kind == script_field_kind::LIST);
            CHECK(codes->elementKind == script_field_kind::ENUM);
            check_enum_meta(codes->meta.root(), SPARSE);
        }

        SUBCASE("a plain field has neither enum nor asset keys") {
            const script_field_info *plain = probe->find_field("Plain");
            REQUIRE(plain != nullptr);
            CHECK(plain->meta.root().size() == 0);
        }

        CHECK(logs.count(core::log_level::WARN) == 0);
        CHECK(logs.count(core::log_level::ERR) == 0);
    }
}
