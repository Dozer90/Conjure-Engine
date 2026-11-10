#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "scripting_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/entity.h"
#include "conjure/scripting/script_class_registry.h"
#include "conjure/scripting/script_fields.h"
#include "conjure/scripting/scripting_error.h"

#include <array>
#include <cstring>
#include <string>
#include <vector>

using namespace conjure;
using namespace conjure::scripting;
using namespace conjure::testing;
using namespace scripting_test;

namespace {

    /// <summary>
    /// A class with one field of every kind (the LIST holds INT32 elements).
    /// </summary>
    [[nodiscard]] script_class_info every_kind_class() {
        return make_class("Game.EveryKind", {}, {
          make_field("flag", script_field_kind::BOOL, "false"),
          make_field("count", script_field_kind::INT32, "0"),
          make_field("big", script_field_kind::INT64, "0"),
          make_field("ratio", script_field_kind::FLOAT, "0.0"),
          make_field("precise", script_field_kind::DOUBLE, "0.0"),
          make_field("label", script_field_kind::STRING, "\"\""),
          make_field("planar", script_field_kind::VEC2, "[0,0]"),
          make_field("spatial", script_field_kind::VEC3, "[0,0,0]"),
          make_field("homogeneous", script_field_kind::VEC4, "[0,0,0,0]"),
          make_field("orientation", script_field_kind::QUAT, "[0,0,0,1]"),
          make_field("tint", script_field_kind::COLOR, "[1,1,1,1]"),
          make_field("link", script_field_kind::ENTITY, "0"),
          make_field("texture", script_field_kind::ASSET, "\"\""),
          make_field("mode", script_field_kind::ENUM, "0"),
          make_field("ints", script_field_kind::LIST, "[]", script_field_kind::INT32),
        });
    }

    constexpr std::string_view EVERY_KIND_JSON = R"({
        "flag": true, "count": 7, "big": 1099511627776, "ratio": 1.5, "precise": 2.25, "label": "hello",
        "planar": [1, 2], "spatial": [1, 2, 3], "homogeneous": [1, 2, 3, 4],
        "orientation": [0, 0.70710677, 0, 0.70710677],
        "tint": [1, 0.5, 0.25, 1], "link": 4294967297, "texture": "01234567-89ab-cdef-0123-456789abcdef",
        "mode": 2, "ints": [1, 2, 3]
    })";

    [[nodiscard]] std::vector<u8> encode(const script_class_info &info, const core::json::value &fields) {
        const core::result<usize> needed = json_to_blob(info, fields, {});
        REQUIRE(needed.has_value());
        std::vector<u8> blob(needed.value());
        const core::result<usize> written = json_to_blob(info, fields, blob);
        REQUIRE(written.has_value());
        REQUIRE(written.value() == blob.size());
        return blob;
    }
}

TEST_SUITE("scripting/script_fields") {

    TEST_CASE("field_blob - round trips every kind through json_to_blob and blob_to_json") {
        const script_class_info info = every_kind_class();
        const core::json::document input = json(EVERY_KIND_JSON);
        const std::vector<u8> blob = encode(info, input.root());
        CONJURE_REQUIRE_OK(field_blob_reader::validate(blob));

        field_blob_reader reader(blob);
        CHECK(reader.is_valid());
        CHECK(reader.field_count() == 15);

        const core::result<core::json::document> decoded = blob_to_json(info, blob);
        CONJURE_REQUIRE_OK(decoded);
        const core::json::value root = decoded.value().root();
        CHECK(root["flag"].as_bool() == true);
        CHECK(root["count"].as_i64() == 7);
        CHECK(root["big"].as_i64() == 1099511627776);
        CHECK(root["ratio"].as_f64() == doctest::Approx(1.5));
        CHECK(root["precise"].as_f64() == doctest::Approx(2.25));
        CHECK(root["label"].as_string() == "hello");
        CHECK(root["planar"].size() == 2);
        CHECK(root["planar"][1].as_f64() == doctest::Approx(2.0));
        CHECK(root["spatial"][2].as_f64() == doctest::Approx(3.0));
        CHECK(root["homogeneous"][3].as_f64() == doctest::Approx(4.0));
        CHECK(root["orientation"][1].as_f64() == doctest::Approx(0.70710677));
        CHECK(root["tint"][1].as_f64() == doctest::Approx(0.5));
        CHECK(root["link"].as_u64() == 4294967297ull);
        CHECK(root["texture"].as_string() == "01234567-89ab-cdef-0123-456789abcdef");
        CHECK(root["mode"].as_i64() == 2);
        REQUIRE(root["ints"].size() == 3);
        CHECK(root["ints"][2].as_i64() == 3);
        CHECK(decoded.value() == core::json::document::from_value(root));
    }

    TEST_CASE("field_blob - the reader rejects a truncated payload") {
        const script_class_info info = every_kind_class();
        const std::vector<u8> blob = encode(info, json(EVERY_KIND_JSON).root());
        for (
          const usize cut : {blob.size() - 1, blob.size() - 5, FIELD_BLOB_HEADER_SIZE + 3, FIELD_BLOB_HEADER_SIZE + 20}
        ) {
            CAPTURE(cut);
            const std::span<const u8> truncated(blob.data(), cut);
            CONJURE_CHECK_ERROR(field_blob_reader::validate(truncated), script_error::FIELD_BLOB_INVALID);
            CONJURE_CHECK_ERROR(blob_to_json(info, truncated), script_error::FIELD_BLOB_INVALID);
        }
    }

    TEST_CASE("field_blob - the reader rejects bad magic, version and kinds") {
        const script_class_info info = every_kind_class();
        std::vector<u8> blob = encode(info, json(EVERY_KIND_JSON).root());

        SUBCASE("magic") {
            blob[0] = 'X';
            field_blob_reader reader(blob);
            CHECK_FALSE(reader.is_valid());
            field_blob_field field = {};
            CONJURE_CHECK_ERROR(reader.next(field), script_error::FIELD_BLOB_INVALID);
            CONJURE_CHECK_ERROR(blob_to_json(info, blob), script_error::FIELD_BLOB_INVALID);
        }
        SUBCASE("version") {
            blob[4] = 2;
            CONJURE_CHECK_ERROR(field_blob_reader::validate(blob), script_error::FIELD_BLOB_INVALID);
        }
        SUBCASE("kind byte") {
            blob[FIELD_BLOB_HEADER_SIZE + 8] = static_cast<u8>(script_field_kind::COUNT);
            CONJURE_CHECK_ERROR(field_blob_reader::validate(blob), script_error::FIELD_BLOB_INVALID);
        }
        SUBCASE("short header") {
            const std::array<u8, 3> stub = {'C', 'S', 'F'};
            field_blob_reader reader(stub);
            CHECK_FALSE(reader.is_valid());
        }
    }

    TEST_CASE("json_to_blob - uses defaults for missing and mismatched fields") {
        log_capture logs = {};
        const script_class_info info = make_class("Game.Defaults", {}, {
          make_field("speed", script_field_kind::FLOAT, "4.5"),
          make_field("name", script_field_kind::STRING, "\"anon\""),
          make_field("count", script_field_kind::INT32, "9"),
          make_field("nodefault", script_field_kind::INT32),
        });
        const core::json::document input = json(R"({"count": "not a number", "extra": 1})");
        const std::vector<u8> blob = encode(info, input.root());
        const core::result<core::json::document> decoded = blob_to_json(info, blob);
        CONJURE_REQUIRE_OK(decoded);
        CHECK(decoded.value().root()["speed"].as_f64() == doctest::Approx(4.5));
        CHECK(decoded.value().root()["name"].as_string() == "anon");
        CHECK(decoded.value().root()["count"].as_i64() == 9);
        CHECK(decoded.value().root()["nodefault"].as_i64(-1) == 0);
        CHECK_FALSE(decoded.value().root().contains("extra"));
        CHECK(logs.count(core::log_level::WARN) == 1);
        CHECK(logs.contains(core::log_level::WARN, "count"));
    }

    TEST_CASE("blob_to_json - skips hashes the class does not know and keeps unknown JSON keys apart") {
        const script_class_info sender = make_class("Game.Sender", {}, {
          make_field("kept", script_field_kind::INT32, "0"),
          make_field("removed", script_field_kind::INT32, "0"),
        });
        const script_class_info receiver = make_class("Game.Receiver", {}, {
          make_field("kept", script_field_kind::INT32, "0"),
        });
        const std::vector<u8> blob = encode(sender, json(R"({"kept": 1, "removed": 2})").root());
        const core::result<core::json::document> decoded = blob_to_json(receiver, blob);
        CONJURE_REQUIRE_OK(decoded);
        CHECK(decoded.value().root()["kept"].as_i64() == 1);
        CHECK_FALSE(decoded.value().root().contains("removed"));

        // The runtime merges decoded members into the stored JSON, so keys outside the schema survive.
        core::json::document stored = json(R"({"kept": 0, "removed": 2, "custom": "yes"})");
        for (const core::json::member member : decoded.value().root().members()) {
            stored.set(member.key, member.item);
        }
        CHECK(stored.root()["kept"].as_i64() == 1);
        CHECK(stored.root()["removed"].as_i64() == 2);
        CHECK(stored.root()["custom"].as_string() == "yes");
    }

    TEST_CASE("field_blob - list fields of every element kind round trip") {
        const script_class_info info = make_class("Game.Lists", {}, {
          make_field("ints", script_field_kind::LIST, "[]", script_field_kind::INT32),
          make_field("names", script_field_kind::LIST, "[]", script_field_kind::STRING),
          make_field("floats", script_field_kind::LIST, "[]", script_field_kind::FLOAT),
          make_field("links", script_field_kind::LIST, "[]", script_field_kind::ENTITY),
          make_field("points", script_field_kind::LIST, "[]", script_field_kind::VEC3),
          make_field("flags", script_field_kind::LIST, "[]", script_field_kind::BOOL),
          make_field("empty", script_field_kind::LIST, "[]", script_field_kind::INT32),
        });
        const core::json::document input = json(R"({
            "ints": [1, -2, 3], "names": ["a", "", "longer name"], "floats": [0.5, 1.5], "links": [1, 4294967297],
            "points": [[1, 2, 3], [4, 5, 6]], "flags": [true, false, true], "empty": []
        })");
        const std::vector<u8> blob = encode(info, input.root());
        CHECK(blob.size() % 4 == 0);
        const core::result<core::json::document> decoded = blob_to_json(info, blob);
        CONJURE_REQUIRE_OK(decoded);
        const core::json::value root = decoded.value().root();
        CHECK(root["ints"][1].as_i64() == -2);
        REQUIRE(root["names"].size() == 3);
        CHECK(root["names"][1].as_string().empty());
        CHECK(root["names"][2].as_string() == "longer name");
        CHECK(root["floats"][1].as_f64() == doctest::Approx(1.5));
        CHECK(root["links"][1].as_u64() == 4294967297ull);
        CHECK(root["points"][1][2].as_f64() == doctest::Approx(6.0));
        CHECK(root["flags"][2].as_bool() == true);
        CHECK(root["empty"].size() == 0);
        CHECK(root["empty"].is_array());
    }

    TEST_CASE("field_blob_writer - reports the size without writing") {
        field_blob_writer measuring({});
        measuring.begin();
        measuring.write_int32(1, 5);
        measuring.write_string(2, "abc");
        measuring.write_bool(3, true);
        const usize needed = measuring.finish();
        CHECK(measuring.overflowed());
        CHECK(measuring.field_count() == 3);
        CHECK(needed == FIELD_BLOB_HEADER_SIZE + 3 * FIELD_BLOB_FIELD_HEADER_SIZE + 4 + 4 + 4);

        std::vector<u8> buffer(needed);
        field_blob_writer writer(buffer);
        writer.begin();
        writer.write_int32(1, 5);
        writer.write_string(2, "abc");
        writer.write_bool(3, true);
        CHECK(writer.finish() == needed);
        CHECK_FALSE(writer.overflowed());
        CHECK(writer.required_size() == needed);
        CONJURE_CHECK_OK(field_blob_reader::validate(buffer));

        std::vector<u8> small(needed - 1);
        field_blob_writer cramped(small);
        cramped.begin();
        cramped.write_int32(1, 5);
        cramped.write_string(2, "abc");
        cramped.write_bool(3, true);
        CHECK(cramped.finish() == needed);
        CHECK(cramped.overflowed());
    }

    TEST_CASE("field_blob - an empty blob is valid and decodes to an empty object") {
        const std::span<const u8> empty = {};
        field_blob_reader reader(empty);
        CHECK(reader.is_valid());
        CHECK(reader.field_count() == 0);
        field_blob_field field = {};
        const core::result<bool> next = reader.next(field);
        CONJURE_REQUIRE_OK(next);
        CHECK_FALSE(next.value());
        CONJURE_CHECK_OK(field_blob_reader::validate(empty));
        const core::result<core::json::document> decoded = blob_to_json(every_kind_class(), empty);
        CONJURE_REQUIRE_OK(decoded);
        CHECK(decoded.value().root().is_object());
        CHECK(decoded.value().root().size() == 0);

        // A header with zero fields is the shortest real blob.
        std::array<u8, FIELD_BLOB_HEADER_SIZE> headerOnly = {};
        field_blob_writer writer(headerOnly);
        writer.begin();
        CHECK(writer.finish() == FIELD_BLOB_HEADER_SIZE);
        CONJURE_CHECK_OK(field_blob_reader::validate(headerOnly));
    }

    TEST_CASE("remap_entity_fields_json - rewrites entity fields and lists of entities only") {
        const script_class_info info = make_class("Game.Links", {}, {
          make_field("target", script_field_kind::ENTITY, "0"),
          make_field("friends", script_field_kind::LIST, "[]", script_field_kind::ENTITY),
          make_field("count", script_field_kind::INT32, "0"),
          make_field("ints", script_field_kind::LIST, "[]", script_field_kind::INT32),
        });
        const ecs::entity first = {1, 1};
        const ecs::entity second = {2, 1};
        const ecs::entity third = {3, 1};
        core::json::document input = core::json::document::object();
        input.set("target", first.to_u64());
        core::json::document friends = core::json::document::array();
        friends.push_back(second.to_u64());
        friends.push_back(third.to_u64());
        input.set("friends", friends);
        input.set("count", 5);
        core::json::document ints = core::json::document::array();
        ints.push_back(1);
        input.set("ints", ints);
        input.set("extra", "kept");

        const core::json::document remapped = remap_entity_fields_json(info, input.root(), [](ecs::entity value) {
            return ecs::entity{value.index + 10, value.generation};
        });
        CHECK(entity_from_json(remapped.root()["target"]) == ecs::entity{11, 1});
        CHECK(entity_from_json(remapped.root()["friends"][0]) == ecs::entity{12, 1});
        CHECK(entity_from_json(remapped.root()["friends"][1]) == ecs::entity{13, 1});
        CHECK(remapped.root()["count"].as_i64() == 5);
        CHECK(remapped.root()["ints"][0].as_i64() == 1);
        CHECK(remapped.root()["extra"].as_string() == "kept");
        CHECK(entity_from_json(core::json::value()) == ecs::NULL_ENTITY);
        CHECK(entity_from_json(remapped.root()["extra"]) == ecs::NULL_ENTITY);

        const core::json::document notObject = remap_entity_fields_json(info, core::json::value(), [](ecs::entity v) {
            return v;
        });
        CHECK(notObject.root().is_object());
    }

    TEST_CASE("is_value_compatible - matches JSON types to kinds") {
        const core::json::document values = json(R"({
            "b": true, "i": 3, "f": 1.5, "s": "text", "v2": [1, 2], "v3": [1, 2, 3], "v4": [1, 2, 3, 4],
            "bad3": [1, "x", 3], "uuid": "01234567-89ab-cdef-0123-456789abcdef", "notuuid": "x", "arr": [1, 2]
        })");
        const core::json::value root = values.root();
        CHECK(is_value_compatible(script_field_kind::BOOL, script_field_kind::BOOL, root["b"]));
        CHECK_FALSE(is_value_compatible(script_field_kind::BOOL, script_field_kind::BOOL, root["i"]));
        CHECK(is_value_compatible(script_field_kind::INT32, script_field_kind::INT32, root["i"]));
        CHECK(is_value_compatible(script_field_kind::INT64, script_field_kind::INT64, root["i"]));
        CHECK(is_value_compatible(script_field_kind::FLOAT, script_field_kind::FLOAT, root["f"]));
        CHECK(is_value_compatible(script_field_kind::DOUBLE, script_field_kind::DOUBLE, root["i"]));
        CHECK(is_value_compatible(script_field_kind::ENUM, script_field_kind::ENUM, root["i"]));
        CHECK(is_value_compatible(script_field_kind::ENTITY, script_field_kind::ENTITY, root["i"]));
        CHECK_FALSE(is_value_compatible(script_field_kind::INT32, script_field_kind::INT32, root["s"]));
        CHECK(is_value_compatible(script_field_kind::STRING, script_field_kind::STRING, root["s"]));
        CHECK(is_value_compatible(script_field_kind::VEC2, script_field_kind::VEC2, root["v2"]));
        CHECK_FALSE(is_value_compatible(script_field_kind::VEC3, script_field_kind::VEC3, root["v2"]));
        CHECK(is_value_compatible(script_field_kind::VEC3, script_field_kind::VEC3, root["v3"]));
        CHECK_FALSE(is_value_compatible(script_field_kind::VEC3, script_field_kind::VEC3, root["bad3"]));
        CHECK(is_value_compatible(script_field_kind::VEC4, script_field_kind::VEC4, root["v4"]));
        CHECK(is_value_compatible(script_field_kind::QUAT, script_field_kind::QUAT, root["v4"]));
        CHECK(is_value_compatible(script_field_kind::COLOR, script_field_kind::COLOR, root["v4"]));
        CHECK(is_value_compatible(script_field_kind::ASSET, script_field_kind::ASSET, root["uuid"]));
        CHECK_FALSE(is_value_compatible(script_field_kind::ASSET, script_field_kind::ASSET, root["notuuid"]));
        CHECK(is_value_compatible(script_field_kind::LIST, script_field_kind::INT32, root["arr"]));
        CHECK_FALSE(is_value_compatible(script_field_kind::LIST, script_field_kind::STRING, root["arr"]));
        CHECK_FALSE(is_value_compatible(script_field_kind::LIST, script_field_kind::LIST, root["arr"]));
        CHECK_FALSE(is_value_compatible(script_field_kind::LIST, script_field_kind::INT32, root["i"]));
        CHECK_FALSE(is_value_compatible(script_field_kind::INT32, script_field_kind::INT32, root["missing"]));
        CHECK_FALSE(is_value_compatible(script_field_kind::COUNT, script_field_kind::COUNT, root["i"]));
    }

    TEST_CASE("field_blob_writer - typed helpers produce the documented payloads") {
        std::array<u8, 256> buffer = {};
        field_blob_writer writer(buffer);
        writer.begin();
        writer.write_entity(1, ecs::entity{5, 7});
        writer.write_asset(2, core::uuid(0x1122334455667788ull, 0x99AABBCCDDEEFF00ull));
        const std::array<f32, 3> xyz = {1.0f, 2.0f, 3.0f};
        writer.write_floats(3, script_field_kind::VEC3, xyz);
        writer.write_int64(4, -9);
        writer.write_double(5, 0.5);
        const usize size = writer.finish();

        field_blob_reader reader(std::span<const u8>(buffer.data(), size));
        field_blob_field field = {};
        REQUIRE(reader.next(field).value());
        CHECK(field.hash == 1);
        CHECK(field.kind == script_field_kind::ENTITY);
        REQUIRE(field.payload.size() == 8);
        u64 packed = 0;
        std::memcpy(&packed, field.payload.data(), sizeof(packed));
        CHECK(ecs::entity::from_u64(packed) == ecs::entity{5, 7});
        REQUIRE(reader.next(field).value());
        CHECK(field.kind == script_field_kind::ASSET);
        CHECK(field.payload.size() == 16);
        CHECK(field.payload[0] == 0x88);
        REQUIRE(reader.next(field).value());
        CHECK(field.kind == script_field_kind::VEC3);
        CHECK(field.payload.size() == 12);
        REQUIRE(reader.next(field).value());
        CHECK(field.kind == script_field_kind::INT64);
        REQUIRE(reader.next(field).value());
        CHECK(field.kind == script_field_kind::DOUBLE);
        CHECK_FALSE(reader.next(field).value());
    }

    TEST_CASE("read_field_value - rejects short payloads and odd lists") {
        const script_field_info vec = make_field("v", script_field_kind::VEC3);
        const std::array<u8, 4> tooShort = {};
        CONJURE_CHECK_ERROR(read_field_value(vec, tooShort), script_error::FIELD_BLOB_INVALID);

        const script_field_info list = make_field("l", script_field_kind::LIST, "[]", script_field_kind::INT32);
        const std::array<u8, 2> noCount = {};
        CONJURE_CHECK_ERROR(read_field_value(list, noCount), script_error::FIELD_BLOB_INVALID);
        const std::array<u8, 6> truncatedList = {2, 0, 0, 0, 1, 0};
        CONJURE_CHECK_ERROR(read_field_value(list, truncatedList), script_error::FIELD_BLOB_INVALID);

        const script_field_info strings = make_field("s", script_field_kind::LIST, "[]", script_field_kind::STRING);
        const std::array<u8, 8> badLength = {1, 0, 0, 0, 9, 0, 0, 0};
        CONJURE_CHECK_ERROR(read_field_value(strings, badLength), script_error::FIELD_BLOB_INVALID);

        const script_field_info text = make_field("t", script_field_kind::STRING);
        const core::result<core::json::document> empty = read_field_value(text, {});
        CONJURE_REQUIRE_OK(empty);
        CHECK(empty.value().root().as_string().empty());
    }
}
