#include "conjure/testing/doctest.h"

#include "reflect_test_types.h"

#include "conjure/core/json/document.h"
#include "conjure/core/json/json.h"
#include "conjure/core/json/value.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/reflect/json_conversion.h"
#include "conjure/core/reflect/type_info.h"
#include "conjure/core/reflect/type_registry.h"

#include <cmath>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::reflect;

namespace {
    json::document parse(std::string_view text) {
        result<json::document> parsed = json::document::parse(text);
        REQUIRE_MESSAGE(parsed.has_value(), text);
        return std::move(parsed).value();
    }

    std::string serialise(const reflect::type_info &type, const void *object) {
        json::writer out = {};
        const result<void> written = to_json(type, object, out);
        REQUIRE(written.has_value());
        return out.finish();
    }

    template <typename T>
    std::string serialise(const T &object) {
        json::writer out = {};
        const result<void> written = to_json(object, out);
        REQUIRE(written.has_value());
        return out.finish();
    }

    reflect_test::everything make_populated() {
        reflect_test::everything object = {};
        object.flag = false;
        object.tiny = -100;
        object.small = -30000;
        object.medium = -2000000000;
        object.large = -9000000000000000000ll;
        object.utiny = 200;
        object.usmall = 60000;
        object.umedium = 4000000000u;
        object.ularge = 18000000000000000000ull;
        object.single = -0.25f;
        object.real = 1e300;
        object.text = "quoted \"text\" with unicode \xC3\xA9";
        object.id = string_id(std::string_view("test.populated_id"));
        object.guid = uuid(0xFFEEDDCCBBAA9988ull, 0x7766554433221100ull);
        object.v2 = math::vec2(-1.5f, 2.25f);
        object.v3 = math::vec3(0.5f, -0.5f, 100.0f);
        object.v4 = math::vec4(1.0f, 2.0f, 3.0f, -4.0f);
        object.rot = math::quat(0.5f, 0.5f, 0.5f, 0.5f);
        object.matrix = math::mat4(2.0f);
        object.matrix[3] = math::vec4(1.0f, 2.0f, 3.0f, 1.0f);
        object.pose.position = math::vec3(10.0f, 20.0f, 30.0f);
        object.pose.rotation = math::quat(0.0f, 1.0f, 0.0f, 0.0f);
        object.pose.scale = math::vec3(2.0f, 2.0f, 2.0f);
        object.weapon = reflect_test::weapon_kind::STAFF;
        object.body = static_cast<reflect_test::body_flags>(3);
        object.mode = reflect_test::signed_mode::POSITIVE;
        object.nested.offset = math::vec3(7.0f, 8.0f, 9.0f);
        object.nested.weight = 0.125f;
        object.counts = {10, 20, 30};
        object.parts[1].weight = 42.0f;
        object.weapons = {reflect_test::weapon_kind::BOW, reflect_test::weapon_kind::BOW};
        object.entity.bits = 0xFFFFFFFF00000001ull;
        object.asset.id = uuid(1, 2);
        return object;
    }
}

TEST_SUITE("core/json_conversion") {

    TEST_CASE("json_conversion - string_id converts from its name, from a raw hash string and from a number") {
        const json::document document = parse(R"(["test.converter_name", "#000000000000abcd", 42, "#zz", -1, true])");
        const json::value root = document.root();

        const result<string_id> named = root[0].to<string_id>();
        REQUIRE(named.has_value());
        CHECK(named.value() == string_id("test.converter_name"));
        CHECK(named.value().view() == "test.converter_name");

        const result<string_id> hashed = root[1].to<string_id>();
        REQUIRE(hashed.has_value());
        CHECK(hashed.value() == string_id::from_hash(0xABCDull));

        const result<string_id> number = root[2].to<string_id>();
        REQUIRE(number.has_value());
        CHECK(number.value() == string_id::from_hash(42));

        // Not 16 hex digits after '#': it is just a name.
        const result<string_id> notHash = root[3].to<string_id>();
        REQUIRE(notHash.has_value());
        CHECK(notHash.value() == string_id("#zz"));

        const result<string_id> negative = root[4].to<string_id>();
        REQUIRE_FALSE(negative.has_value());
        CHECK(negative.get_error().generic_code() == error_code::INVALID_ARGUMENT);

        const result<string_id> boolean = root[5].to<string_id>();
        REQUIRE_FALSE(boolean.has_value());
        CHECK(boolean.get_error().message().find("found bool") != std::string::npos);

        const result<string_id> missing = root[99].to<string_id>();
        REQUIRE_FALSE(missing.has_value());
        CHECK(missing.get_error().generic_code() == error_code::NOT_FOUND);
    }

    TEST_CASE("json_conversion - string_id writes the interned name or the hex hash") {
        json::writer out = {};
        write_json(string_id(std::string_view("test.written_id")), out);
        CHECK(out.finish() == "\"test.written_id\"");

        write_json(string_id::from_hash(0x1234ull), out);
        CHECK(out.finish() == "\"#0000000000001234\"");

        write_json(string_id(), out);
        const std::string nullText = out.finish();
        CHECK(nullText == "\"#0000000000000000\"");
        CHECK(parse(nullText).root().to<string_id>().value().is_null());
    }

    TEST_CASE("json_conversion - vectors convert from arrays of the right length") {
        const json::document document =
          parse(R"([[1, 2], [1, 2, 3], [1, 2, 3, 4], [1, 2, 3], [1, "x"], {"x": 1}, [1.5e40, 0]])");
        const json::value root = document.root();

        CHECK(root[0].to<math::vec2>().value() == math::vec2(1.0f, 2.0f));
        CHECK(root[1].to<math::vec3>().value() == math::vec3(1.0f, 2.0f, 3.0f));
        CHECK(root[2].to<math::vec4>().value() == math::vec4(1.0f, 2.0f, 3.0f, 4.0f));

        const result<math::vec2> tooLong = root[1].to<math::vec2>();
        REQUIRE_FALSE(tooLong.has_value());
        CHECK(tooLong.get_error().generic_code() == error_code::INVALID_ARGUMENT);
        CHECK(tooLong.get_error().message().find("array of 2 numbers") != std::string::npos);

        const result<math::vec4> tooShort = root[3].to<math::vec4>();
        REQUIRE_FALSE(tooShort.has_value());
        CHECK(tooShort.get_error().message().find("array of 3 elements") != std::string::npos);

        const result<math::vec2> notNumber = root[4].to<math::vec2>();
        REQUIRE_FALSE(notNumber.has_value());
        CHECK(notNumber.get_error().message().find("element 1") != std::string::npos);

        const result<math::vec3> object = root[5].to<math::vec3>();
        REQUIRE_FALSE(object.has_value());
        CHECK(object.get_error().message().find("found object") != std::string::npos);

        const result<math::vec2> outOfRange = root[6].to<math::vec2>();
        REQUIRE_FALSE(outOfRange.has_value());
        CHECK(outOfRange.get_error().generic_code() == error_code::OUT_OF_RANGE);

        CHECK_FALSE(root[99].to<math::vec3>().has_value());
    }

    TEST_CASE("json_conversion - quaternions use storage order x, y, z, w") {
        const json::document document = parse("[0.25, 0.5, 0.75, 1.0]");
        const result<math::quat> converted = document.root().to<math::quat>();
        REQUIRE(converted.has_value());
        CHECK(converted->x == 0.25f);
        CHECK(converted->y == 0.5f);
        CHECK(converted->z == 0.75f);
        CHECK(converted->w == 1.0f);

        json::writer out = {};
        write_json(math::quat(1.0f, 0.25f, 0.5f, 0.75f), out);
        CHECK(parse(out.finish()) == document);

        // f32 components are written as their exact value, so inexact decimals round-trip bit for bit.
        const result<math::quat> inexact = parse("[0.1, 0.2, 0.3, 0.4]").root().to<math::quat>();
        REQUIRE(inexact.has_value());
        CHECK(inexact->x == 0.1f);
        write_json(inexact.value(), out);
        const result<math::quat> again = parse(out.finish()).root().to<math::quat>();
        REQUIRE(again.has_value());
        CHECK(again.value() == inexact.value());
    }

    TEST_CASE("json_conversion - matrices use 16 column-major numbers") {
        math::mat4 matrix(0.0f);
        f32 next = 0.0f;
        for (glm::length_t column = 0; column < 4; ++column) {
            for (glm::length_t row = 0; row < 4; ++row) {
                matrix[column][row] = next;
                next += 1.0f;
            }
        }

        json::writer out = {};
        write_json(matrix, out);
        const std::string text = out.finish();
        CHECK(text == "[0.0,1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0,11.0,12.0,13.0,14.0,15.0]");

        const result<math::mat4> back = parse(text).root().to<math::mat4>();
        REQUIRE(back.has_value());
        CHECK(back.value() == matrix);
        CHECK(back.value()[1][0] == 4.0f);

        CHECK_FALSE(parse("[1, 2, 3]").root().to<math::mat4>().has_value());
    }

    TEST_CASE("json_conversion - transforms convert from an object whose members are optional") {
        const json::document full = parse(R"({"position": [1, 2, 3], "rotation": [0, 0, 0, 1], "scale": [2, 2, 2]})");
        const result<math::transform> converted = full.root().to<math::transform>();
        REQUIRE(converted.has_value());
        CHECK(converted->position == math::vec3(1.0f, 2.0f, 3.0f));
        CHECK(converted->rotation == math::quat(1.0f, 0.0f, 0.0f, 0.0f));
        CHECK(converted->scale == math::vec3(2.0f));

        const result<math::transform> partial = parse(R"({"scale": [3, 3, 3]})").root().to<math::transform>();
        REQUIRE(partial.has_value());
        CHECK(partial->position == math::vec3(0.0f));
        CHECK(partial->rotation == math::quat(1.0f, 0.0f, 0.0f, 0.0f));
        CHECK(partial->scale == math::vec3(3.0f));

        const result<math::transform> wrongMember = parse(R"({"position": "here"})").root().to<math::transform>();
        REQUIRE_FALSE(wrongMember.has_value());
        CHECK(wrongMember.get_error().generic_code() == error_code::INVALID_ARGUMENT);

        const result<math::transform> notObject = parse("[1, 2, 3]").root().to<math::transform>();
        REQUIRE_FALSE(notObject.has_value());
        CHECK(notObject.get_error().message().find("expected object") != std::string::npos);

        json::writer out = {};
        write_json(converted.value(), out);
        CHECK(out.finish() == R"({"position":[1.0,2.0,3.0],"rotation":[0.0,0.0,0.0,1.0],"scale":[2.0,2.0,2.0]})");
    }

    TEST_CASE("json_conversion - to_json writes every field in declaration order") {
        const reflect_test::inner object = {};
        CHECK(serialise(object) == R"({"offset":[1.0,2.0,3.0],"weight":0.5})");

        const reflect::type_info *type = type_info_of<reflect_test::everything>();
        REQUIRE(type != nullptr);
        const reflect_test::everything defaults = {};
        const json::document document = parse(serialise(*type, &defaults));
        const json::value root = document.root();
        REQUIRE(root.is_object());
        CHECK(root.size() == 29);
        CHECK(root["flag"].as_bool() == true);
        CHECK(root["tiny"].as_i64() == -1);
        CHECK(root["ularge"].as_u64() == 4);
        CHECK(root["single"].as_f64() == 1.5);
        CHECK(root["real"].as_f64() == 2.5);
        CHECK(root["text"].as_string() == "hello");
        CHECK(root["id"].as_string() == "#0000000000000000");
        CHECK(root["guid"].as_string() == "01234567-89ab-cdef-fedc-ba9876543210");
        CHECK(root["v2"] == parse("[1.0, 2.0]").root());
        CHECK(root["v4"] == parse("[1.0, 2.0, 3.0, 4.0]").root());
        CHECK(root["rot"] == parse("[0.0, 0.0, 0.0, 1.0]").root());
        CHECK(root["matrix"].size() == 16);
        CHECK(root["matrix"][5].as_f64() == 1.0);
        CHECK(root["pose"]["scale"] == parse("[1.0, 1.0, 1.0]").root());
        CHECK(root["weapon"].as_string() == "BOW");
        CHECK(root["body"].as_string() == "STATIC");
        CHECK(root["mode"].as_string() == "NEGATIVE");
        CHECK(root["nested"]["weight"].as_f64() == 0.5);
        CHECK(root["counts"] == parse("[1, 2, 3]").root());
        CHECK(root["parts"].size() == 2);
        CHECK(root["parts"][1]["offset"] == parse("[1.0, 2.0, 3.0]").root());
        CHECK(root["weapons"] == parse(R"(["SWORD", "STAFF"])").root());
        CHECK(root["entity"].as_u64() == 0);
        CHECK(root["asset"].as_string() == "00000000-0000-0000-0000-000000000000");
    }

    TEST_CASE("json_conversion - every field kind round-trips through JSON") {
        const reflect_test::everything original = make_populated();
        const std::string text = serialise(original);

        reflect_test::everything restored = {};
        std::vector<std::string> unknown = {};
        const json::document document = parse(text);
        const result<void> read = from_json(restored, document.root(), &unknown);
        REQUIRE(read.has_value());
        CHECK(unknown.empty());
        CHECK(restored == original);
        CHECK(restored.id.view() == "test.populated_id");
        CHECK(serialise(restored) == text);
    }

    TEST_CASE("json_conversion - to_json_document equals the parsed writer output and is canonical-stable") {
        const reflect_test::everything object = make_populated();
        const result<json::document> document = to_json_document(object);
        REQUIRE(document.has_value());
        CHECK(*document == parse(serialise(object)));
        CHECK(document->to_canonical_string() == parse(serialise(object)).to_canonical_string());

        const reflect::type_info *type = type_info_of<reflect_test::everything>();
        REQUIRE(type != nullptr);
        const result<json::document> untyped = to_json_document(*type, &object);
        REQUIRE(untyped.has_value());
        CHECK(*untyped == *document);
    }

    TEST_CASE("json_conversion - missing members keep the current value and unknown members are reported") {
        reflect_test::everything object = make_populated();
        const reflect_test::everything before = object;
        const json::document document = parse(
          R"({"medium": 7, "bogus": 1, "nested": {"weight": 2.0, "extra": true},)"
          R"( "pose": {"scale": [5, 5, 5], "skew": 0}})"
        );

        std::vector<std::string> unknown = {};
        REQUIRE(from_json(object, document.root(), &unknown).has_value());
        CHECK(object.medium == 7);
        CHECK(object.nested.weight == 2.0f);
        CHECK(object.nested.offset == before.nested.offset);
        CHECK(object.pose.scale == math::vec3(5.0f));
        CHECK(object.pose.position == before.pose.position);
        CHECK(object.text == before.text);
        CHECK(object.counts == before.counts);
        REQUIRE(unknown.size() == 3);
        CHECK(unknown[0] == "bogus");
        CHECK(unknown[1] == "nested.extra");
        CHECK(unknown[2] == "pose.skew");

        // Without a list unknown members are silently ignored.
        REQUIRE(from_json(object, document.root()).has_value());
        REQUIRE(from_json(object, parse("{}").root()).has_value());
        CHECK(object.medium == 7);
    }

    TEST_CASE("json_conversion - from_json rejects a non-object and a null object pointer") {
        reflect_test::inner object = {};
        const result<void> array = from_json(object, parse("[1, 2]").root());
        REQUIRE_FALSE(array.has_value());
        CHECK(array.get_error().generic_code() == error_code::INVALID_ARGUMENT);
        CHECK(array.get_error().message().find("test.inner") != std::string::npos);
        CHECK(array.get_error().message().find("expected object, found array") != std::string::npos);

        const result<void> missing = from_json(object, json::value());
        REQUIRE_FALSE(missing.has_value());
        CHECK(missing.get_error().generic_code() == error_code::NOT_FOUND);

        const reflect::type_info *type = type_info_of<reflect_test::inner>();
        REQUIRE(type != nullptr);
        const result<void> nullObject = from_json(*type, nullptr, parse("{}").root());
        REQUIRE_FALSE(nullObject.has_value());
        CHECK(nullObject.get_error().generic_code() == error_code::INVALID_ARGUMENT);

        json::writer out = {};
        const result<void> nullWrite = to_json(*type, nullptr, out);
        REQUIRE_FALSE(nullWrite.has_value());
        CHECK(nullWrite.get_error().generic_code() == error_code::INVALID_ARGUMENT);
    }

    TEST_CASE("json_conversion - type mismatches and out-of-range numbers fail with the field path") {
        reflect_test::everything object = {};

        const result<void> wrongType = from_json(object, parse(R"({"medium": "seven"})").root());
        REQUIRE_FALSE(wrongType.has_value());
        CHECK(wrongType.get_error().generic_code() == error_code::INVALID_ARGUMENT);
        CHECK(wrongType.get_error().message().find("field 'medium'") != std::string::npos);
        CHECK(object.medium == -3);

        const result<void> tooBig = from_json(object, parse(R"({"utiny": 300})").root());
        REQUIRE_FALSE(tooBig.has_value());
        CHECK(tooBig.get_error().generic_code() == error_code::OUT_OF_RANGE);
        CHECK(tooBig.get_error().message().find("field 'utiny'") != std::string::npos);
        CHECK(object.utiny == 1);

        const result<void> negativeUnsigned = from_json(object, parse(R"({"ularge": -1})").root());
        REQUIRE_FALSE(negativeUnsigned.has_value());
        CHECK(negativeUnsigned.get_error().generic_code() == error_code::OUT_OF_RANGE);

        const result<void> nested = from_json(object, parse(R"({"nested": {"weight": [1]}})").root());
        REQUIRE_FALSE(nested.has_value());
        CHECK(nested.get_error().message().find("field 'nested.weight'") != std::string::npos);

        const result<void> notObject = from_json(object, parse(R"({"nested": 5})").root());
        REQUIRE_FALSE(notObject.has_value());
        CHECK(notObject.get_error().message().find("field 'nested'") != std::string::npos);
        CHECK(notObject.get_error().message().find("expected object, found number") != std::string::npos);

        const result<void> text = from_json(object, parse(R"({"text": 5})").root());
        REQUIRE_FALSE(text.has_value());
        CHECK(text.get_error().message().find("expected string, found number") != std::string::npos);

        const result<void> boolean = from_json(object, parse(R"({"flag": 1})").root());
        REQUIRE_FALSE(boolean.has_value());
        CHECK(boolean.get_error().generic_code() == error_code::INVALID_ARGUMENT);

        const result<void> badUuid = from_json(object, parse(R"({"guid": "not-a-uuid"})").root());
        REQUIRE_FALSE(badUuid.has_value());
        CHECK(badUuid.get_error().message().find("field 'guid'") != std::string::npos);
    }

    TEST_CASE("json_conversion - fields read before a failure keep their new values") {
        reflect_test::everything object = {};
        const result<void> partial = from_json(object, parse(R"({"medium": 9, "utiny": 999, "large": 11})").root());
        REQUIRE_FALSE(partial.has_value());
        CHECK(object.medium == 9);
        CHECK(object.utiny == 1);
        CHECK(object.large == -4);
    }

    TEST_CASE("json_conversion - integral reals are accepted for integers and non-finite floats travel as null") {
        reflect_test::everything object = {};
        REQUIRE(from_json(object, parse(R"({"medium": 3.0, "usmall": 12.0})").root()).has_value());
        CHECK(object.medium == 3);
        CHECK(object.usmall == 12);
        CHECK_FALSE(from_json(object, parse(R"({"medium": 3.5})").root()).has_value());

        object.single = std::numeric_limits<f32>::quiet_NaN();
        object.real = std::numeric_limits<f64>::infinity();
        object.v3.y = std::numeric_limits<f32>::infinity();
        const json::document document = parse(serialise(object));
        CHECK(document.root()["single"].is_null());
        CHECK(document.root()["real"].is_null());
        CHECK(document.root()["v3"][1].is_null());

        reflect_test::everything restored = {};
        REQUIRE(from_json(restored, document.root()).has_value());
        CHECK(std::isnan(restored.single));
        CHECK(std::isnan(restored.real));
        CHECK(std::isnan(restored.v3.y));
        CHECK(restored.v3.x == 1.0f);
    }

    TEST_CASE("json_conversion - fixed arrays accept shorter input and reject longer input") {
        reflect_test::everything object = {};
        REQUIRE(from_json(object, parse(R"({"counts": [7]})").root()).has_value());
        CHECK(object.counts == std::array<i32, 3>{7, 2, 3});

        REQUIRE(from_json(object, parse(R"({"counts": []})").root()).has_value());
        CHECK(object.counts == std::array<i32, 3>{7, 2, 3});

        const result<void> tooLong = from_json(object, parse(R"({"counts": [1, 2, 3, 4]})").root());
        REQUIRE_FALSE(tooLong.has_value());
        CHECK(tooLong.get_error().generic_code() == error_code::OUT_OF_RANGE);
        CHECK(tooLong.get_error().message().find("field 'counts'") != std::string::npos);
        CHECK(tooLong.get_error().message().find("4 elements") != std::string::npos);

        const result<void> badElement = from_json(object, parse(R"({"counts": [1, "two"]})").root());
        REQUIRE_FALSE(badElement.has_value());
        CHECK(badElement.get_error().message().find("field 'counts[1]'") != std::string::npos);
        CHECK(object.counts[0] == 1);

        const result<void> notArray = from_json(object, parse(R"({"counts": 5})").root());
        REQUIRE_FALSE(notArray.has_value());
        CHECK(notArray.get_error().message().find("expected array, found number") != std::string::npos);
    }

    TEST_CASE("json_conversion - arrays of structs and enums read element by element with paths") {
        reflect_test::everything object = {};
        REQUIRE(from_json(object, parse(R"({"parts": [{"weight": 5}, {"offset": [9, 9, 9]}]})").root()).has_value());
        CHECK(object.parts[0].weight == 5.0f);
        CHECK(object.parts[0].offset == math::vec3(1.0f, 2.0f, 3.0f));
        CHECK(object.parts[1].offset == math::vec3(9.0f));
        CHECK(object.parts[1].weight == 0.5f);

        const result<void> nestedFailure = from_json(object, parse(R"({"parts": [{}, {"weight": "x"}]})").root());
        REQUIRE_FALSE(nestedFailure.has_value());
        CHECK(nestedFailure.get_error().message().find("field 'parts[1].weight'") != std::string::npos);

        std::vector<std::string> unknown = {};
        REQUIRE(from_json(object, parse(R"({"parts": [{"stray": 1}]})").root(), &unknown).has_value());
        REQUIRE(unknown.size() == 1);
        CHECK(unknown[0] == "parts[0].stray");

        REQUIRE(from_json(object, parse(R"({"weapons": ["STAFF", 5]})").root()).has_value());
        CHECK(object.weapons[0] == reflect_test::weapon_kind::STAFF);
        CHECK(object.weapons[1] == reflect_test::weapon_kind::BOW);
        const result<void> badEnum = from_json(object, parse(R"({"weapons": ["AXE"]})").root());
        REQUIRE_FALSE(badEnum.has_value());
        CHECK(badEnum.get_error().message().find("field 'weapons[0]'") != std::string::npos);
    }

    TEST_CASE("json_conversion - enums read by name or by number and write their name when one exists") {
        reflect_test::everything object = {};
        REQUIRE(from_json(object, parse(R"({"weapon": "STAFF", "mode": -2})").root()).has_value());
        CHECK(object.weapon == reflect_test::weapon_kind::STAFF);
        CHECK(object.mode == reflect_test::signed_mode::NEGATIVE);

        REQUIRE(from_json(object, parse(R"({"weapon": 5, "mode": "POSITIVE"})").root()).has_value());
        CHECK(object.weapon == reflect_test::weapon_kind::BOW);
        CHECK(object.mode == reflect_test::signed_mode::POSITIVE);

        const result<void> unknownName = from_json(object, parse(R"({"weapon": "AXE"})").root());
        REQUIRE_FALSE(unknownName.has_value());
        CHECK(unknownName.get_error().generic_code() == error_code::INVALID_ARGUMENT);
        const std::string &unknownMessage = unknownName.get_error().message();
        CHECK(unknownMessage.find("'AXE' is not an enumerator of test.weapon_kind") != std::string::npos);

        const result<void> tooLarge = from_json(object, parse(R"({"weapon": 256})").root());
        REQUIRE_FALSE(tooLarge.has_value());
        CHECK(tooLarge.get_error().generic_code() == error_code::OUT_OF_RANGE);

        const result<void> negativeUnsigned = from_json(object, parse(R"({"weapon": -1})").root());
        REQUIRE_FALSE(negativeUnsigned.has_value());
        CHECK(negativeUnsigned.get_error().generic_code() == error_code::OUT_OF_RANGE);

        const result<void> real = from_json(object, parse(R"({"weapon": 1.5})").root());
        REQUIRE_FALSE(real.has_value());
        CHECK(real.get_error().generic_code() == error_code::INVALID_ARGUMENT);

        const result<void> tooSmall = from_json(object, parse(R"({"mode": -40000})").root());
        REQUIRE_FALSE(tooSmall.has_value());
        CHECK(tooSmall.get_error().generic_code() == error_code::OUT_OF_RANGE);

        // A value without a name is written as its number and reads back unchanged.
        object.weapon = static_cast<reflect_test::weapon_kind>(9);
        object.mode = static_cast<reflect_test::signed_mode>(-7);
        const json::document document = parse(serialise(object));
        CHECK(document.root()["weapon"].as_i64() == 9);
        CHECK(document.root()["mode"].as_i64() == -7);
        reflect_test::everything restored = {};
        REQUIRE(from_json(restored, document.root()).has_value());
        CHECK(restored.weapon == static_cast<reflect_test::weapon_kind>(9));
        CHECK(restored.mode == static_cast<reflect_test::signed_mode>(-7));
    }

    TEST_CASE("json_conversion - 8 byte enums round-trip by name and by number") {
        const enum_info *unsignedInfo = enum_info_of<reflect_test::big_unsigned>();
        REQUIRE(unsignedInfo != nullptr);
        CHECK(unsignedInfo->underlyingSize == 8);
        CHECK_FALSE(unsignedInfo->isSigned);
        CHECK(unsignedInfo->find_value("TOP") == -1);   // the bit pattern of u64 max
        const enum_info *signedInfo = enum_info_of<reflect_test::big_signed>();
        REQUIRE(signedInfo != nullptr);
        CHECK(signedInfo->isSigned);
        CHECK(signedInfo->find_value("LOW") == std::numeric_limits<i64>::min());

        const reflect_test::wide_enums defaults = {};
        CHECK(serialise(defaults) == R"({"top":"TOP","low":"LOW"})");
        reflect_test::wide_enums restored = {};
        restored.top = reflect_test::big_unsigned::ZERO;
        restored.low = reflect_test::big_signed::HIGH;
        REQUIRE(from_json(restored, parse(serialise(defaults)).root()).has_value());
        CHECK(restored == defaults);

        // Values without a name travel as numbers: the full u64 range for the unsigned enum.
        reflect_test::wide_enums numeric = {};
        numeric.top = static_cast<reflect_test::big_unsigned>(0xFFFFFFFFFFFFFFFEull);
        numeric.low = static_cast<reflect_test::big_signed>(-5);
        const json::document document = parse(serialise(numeric));
        CHECK(document.root()["top"].as_u64() == 0xFFFFFFFFFFFFFFFEull);
        CHECK(document.root()["low"].as_i64() == -5);
        REQUIRE(from_json(restored, document.root()).has_value());
        CHECK(restored == numeric);

        REQUIRE(from_json(restored, parse(R"({"top": 18446744073709551615, "low": -9223372036854775808})").root())
          .has_value());
        CHECK(restored.top == reflect_test::big_unsigned::TOP);
        CHECK(restored.low == reflect_test::big_signed::LOW);
        REQUIRE(from_json(restored, parse(R"({"low": 9223372036854775807})").root()).has_value());
        CHECK(restored.low == reflect_test::big_signed::HIGH);

        // Above INT64_MAX is a u64 bit pattern that only the unsigned enum may hold.
        const result<void> tooBig = from_json(restored, parse(R"({"low": 18446744073709551615})").root());
        REQUIRE_FALSE(tooBig.has_value());
        CHECK(tooBig.get_error().generic_code() == error_code::OUT_OF_RANGE);
        CHECK(tooBig.get_error().message().find("field 'low'") != std::string::npos);
        CHECK(tooBig.get_error().message().find("18446744073709551615") != std::string::npos);
        CHECK(restored.low == reflect_test::big_signed::HIGH);
        const result<void> justAbove = from_json(restored, parse(R"({"low": 9223372036854775808})").root());
        REQUIRE_FALSE(justAbove.has_value());
        CHECK(justAbove.get_error().generic_code() == error_code::OUT_OF_RANGE);
    }

    TEST_CASE("json_conversion - a field without a kind fails with INVALID_STATE in both directions") {
        // Hand-built and never validated, as a broken dynamic description would be.
        reflect::type_info type = {};
        type.name = "test.no_kind";
        type.size = 4;
        type.alignment = 4;
        field_info value = {};
        value.name = "value";
        value.kind = field_kind::COUNT;
        value.size = 4;
        type.fields = {value};
        CHECK_FALSE(validate_layout(type).has_value());

        u32 storage = 7;
        json::writer out = {};
        const result<void> written = to_json(type, &storage, out);
        REQUIRE_FALSE(written.has_value());
        CHECK(written.get_error().generic_code() == error_code::INVALID_STATE);
        CHECK(written.get_error().message().find("field 'value'") != std::string::npos);
        CHECK(written.get_error().message().find("no kind") != std::string::npos);

        const result<void> read = from_json(type, &storage, parse(R"({"value": 1})").root());
        REQUIRE_FALSE(read.has_value());
        CHECK(read.get_error().generic_code() == error_code::INVALID_STATE);
        CHECK(read.get_error().message().find("field 'value'") != std::string::npos);
        CHECK(storage == 7);

        const field_ref ref = {&storage, &type.fields[0], false};
        CHECK_FALSE(get_as_json(ref).has_value());
        CHECK(set_from_json(ref, parse("1").root()).get_error().generic_code() == error_code::INVALID_STATE);
    }

    TEST_CASE("json_conversion - a member repeated in the JSON object is read last-wins") {
        reflect_test::everything object = {};
        std::vector<std::string> unknown = {};
        REQUIRE(from_json(object, parse(R"({"medium": 1, "medium": 2})").root(), &unknown).has_value());
        CHECK(object.medium == 2);
        CHECK(unknown.empty());

        REQUIRE(from_json(object, parse(R"({"nested": {"weight": 1}, "nested": {"weight": 3}})").root()).has_value());
        CHECK(object.nested.weight == 3.0f);
    }

    TEST_CASE("json_conversion - flag enums write and read '|' separated names") {
        reflect_test::everything object = {};
        object.body = static_cast<reflect_test::body_flags>(3);
        CHECK(parse(serialise(object)).root()["body"].as_string() == "STATIC|DYNAMIC");

        object.body = reflect_test::body_flags::ALL;
        CHECK(parse(serialise(object)).root()["body"].as_string() == "ALL");

        object.body = reflect_test::body_flags::NONE;
        CHECK(parse(serialise(object)).root()["body"].as_string() == "NONE");

        object.body = static_cast<reflect_test::body_flags>(16);
        CHECK(parse(serialise(object)).root()["body"].as_u64() == 16);

        REQUIRE(from_json(object, parse(R"({"body": "KINEMATIC | STATIC"})").root()).has_value());
        CHECK(object.body == static_cast<reflect_test::body_flags>(5));
        REQUIRE(from_json(object, parse(R"({"body": ""})").root()).has_value());
        CHECK(object.body == reflect_test::body_flags::NONE);
        REQUIRE(from_json(object, parse(R"({"body": 6})").root()).has_value());
        CHECK(object.body == static_cast<reflect_test::body_flags>(6));

        const result<void> unknownBit = from_json(object, parse(R"({"body": "STATIC|GHOST"})").root());
        REQUIRE_FALSE(unknownBit.has_value());
        CHECK(unknownBit.get_error().message().find("STATIC|GHOST") != std::string::npos);
    }

    TEST_CASE("json_conversion - entity and asset handles travel as raw integers and uuid strings") {
        reflect_test::everything object = {};
        object.entity.bits = 0x0102030405060708ull;
        object.asset.id = uuid(0xAAAAAAAAAAAAAAAAull, 0x5555555555555555ull);
        const json::document document = parse(serialise(object));
        CHECK(document.root()["entity"].as_u64() == 0x0102030405060708ull);
        CHECK(document.root()["asset"].as_string() == "aaaaaaaa-aaaa-aaaa-5555-555555555555");
        CHECK(document.root()["asset"].as_string() == object.asset.id.to_string());

        reflect_test::everything restored = {};
        REQUIRE(from_json(restored, document.root()).has_value());
        CHECK(restored.entity == object.entity);
        CHECK(restored.asset == object.asset);

        const result<void> negative = from_json(restored, parse(R"({"entity": -5})").root());
        REQUIRE_FALSE(negative.has_value());
        CHECK(negative.get_error().message().find("expected unsigned integer") != std::string::npos);

        const result<void> text = from_json(restored, parse(R"({"entity": "e1"})").root());
        REQUIRE_FALSE(text.has_value());
        CHECK(text.get_error().generic_code() == error_code::INVALID_ARGUMENT);

        const result<void> badAsset = from_json(restored, parse(R"({"asset": 12})").root());
        REQUIRE_FALSE(badAsset.has_value());
        CHECK(badAsset.get_error().message().find("field 'asset'") != std::string::npos);
    }

    TEST_CASE("json_conversion - small handles are range-checked and odd-sized handles are unsupported") {
        field_info handle = {};
        handle.name = "handle";
        handle.kind = field_kind::ENTITY;
        handle.type = type_id::from_name("test.small_handle");
        handle.offset = 0;
        handle.size = 2;
        const result<reflect::type_info> built =
          build_type_info("test.small_handle_holder", 2, 2, std::span<const field_info>(&handle, 1));
        REQUIRE(built.has_value());

        u16 storage = 0;
        REQUIRE(from_json(*built, &storage, parse(R"({"handle": 65535})").root()).has_value());
        CHECK(storage == 65535);
        const result<void> tooBig = from_json(*built, &storage, parse(R"({"handle": 65536})").root());
        REQUIRE_FALSE(tooBig.has_value());
        CHECK(tooBig.get_error().generic_code() == error_code::OUT_OF_RANGE);
        CHECK(serialise(*built, &storage) == R"({"handle":65535})");

        const reflect_test::with_odd_handle odd = {};
        json::writer out = {};
        const result<void> written = to_json(odd, out);
        REQUIRE_FALSE(written.has_value());
        CHECK(written.get_error().generic_code() == error_code::UNSUPPORTED);
        CHECK(written.get_error().message().find("field 'handle'") != std::string::npos);
        CHECK(written.get_error().message().find("size 3") != std::string::npos);

        reflect_test::with_odd_handle target = {};
        const result<void> read = from_json(target, parse(R"({"handle": 1})").root());
        REQUIRE_FALSE(read.has_value());
        CHECK(read.get_error().generic_code() == error_code::UNSUPPORTED);
    }

    TEST_CASE("json_conversion - a description without its struct or enum pointer fails with INVALID_STATE") {
        reflect::type_info type = describe_struct<reflect_test::everything>();
        for (field_info &field : type.fields) {
            if (field.name == "nested") {
                field.structType = nullptr;
            }
            if (field.name == "weapon") {
                field.enumType = nullptr;
            }
        }
        REQUIRE(type.find_field("nested")->structType == nullptr);
        REQUIRE(type.find_field("weapon")->enumType == nullptr);

        const reflect_test::everything object = {};
        json::writer out = {};
        const result<void> written = to_json(type, &object, out);
        REQUIRE_FALSE(written.has_value());
        CHECK(written.get_error().generic_code() == error_code::INVALID_STATE);
        CHECK(written.get_error().message().find("field 'weapon'") != std::string::npos);

        reflect_test::everything target = {};
        const result<void> readEnum = from_json(type, &target, parse(R"({"weapon": "BOW"})").root());
        REQUIRE_FALSE(readEnum.has_value());
        CHECK(readEnum.get_error().generic_code() == error_code::INVALID_STATE);
        const result<void> readStruct = from_json(type, &target, parse(R"({"nested": {}})").root());
        REQUIRE_FALSE(readStruct.has_value());
        CHECK(readStruct.get_error().generic_code() == error_code::INVALID_STATE);
        CHECK(readStruct.get_error().message().find("field 'nested'") != std::string::npos);
    }

    TEST_CASE("json_conversion - the typed wrappers fail with INVALID_STATE for unreflected types") {
        const reflect_test::not_reflected object = {};
        json::writer out = {};
        const result<void> written = to_json(object, out);
        REQUIRE_FALSE(written.has_value());
        CHECK(written.get_error().generic_code() == error_code::INVALID_STATE);

        reflect_test::not_reflected target = {};
        const result<void> read = from_json(target, parse("{}").root());
        REQUIRE_FALSE(read.has_value());
        CHECK(read.get_error().generic_code() == error_code::INVALID_STATE);

        const result<json::document> document = to_json_document(object);
        REQUIRE_FALSE(document.has_value());
        CHECK(document.get_error().generic_code() == error_code::INVALID_STATE);
    }

    TEST_CASE("json_conversion - resolve_path walks nested structs and array elements") {
        reflect_test::everything object = make_populated();
        const reflect::type_info *type = type_info_of<reflect_test::everything>();
        REQUIRE(type != nullptr);

        const result<field_ref> single = resolve_path(*type, &object, "single");
        REQUIRE(single.has_value());
        CHECK(single->data == &object.single);
        CHECK(single->field == type->find_field("single"));
        CHECK_FALSE(single->isElement);
        CHECK(single->kind() == field_kind::F32);
        CHECK(single->size() == 4);
        CHECK(single->is_valid());

        const result<field_ref> nested = resolve_path(*type, &object, "nested.offset");
        REQUIRE(nested.has_value());
        CHECK(nested->data == &object.nested.offset);
        CHECK(nested->field->name == "offset");
        CHECK(nested->kind() == field_kind::VEC3);

        const result<field_ref> element = resolve_path(*type, &object, "counts[2]");
        REQUIRE(element.has_value());
        CHECK(element->data == &object.counts[2]);
        CHECK(element->isElement);
        CHECK(element->kind() == field_kind::I32);
        CHECK(element->size() == 4);

        const result<field_ref> wholeArray = resolve_path(*type, &object, "counts");
        REQUIRE(wholeArray.has_value());
        CHECK(wholeArray->kind() == field_kind::ARRAY);
        CHECK(wholeArray->size() == 12);

        const result<field_ref> deep = resolve_path(*type, &object, "parts[1].weight");
        REQUIRE(deep.has_value());
        CHECK(deep->data == &object.parts[1].weight);
        CHECK(deep->field == type_info_of<reflect_test::inner>()->find_field("weight"));

        const result<field_ref> part = resolve_path(*type, &object, "parts[0]");
        REQUIRE(part.has_value());
        CHECK(part->data == &object.parts[0]);
        CHECK(part->isElement);
        CHECK(part->kind() == field_kind::STRUCT);
        CHECK(part->size() == sizeof(reflect_test::inner));

        const result<field_ref> pose = resolve_path(*type, &object, "pose");
        REQUIRE(pose.has_value());
        CHECK(pose->kind() == field_kind::TRANSFORM);
    }

    TEST_CASE("json_conversion - resolve_path reports every malformed or unresolvable path") {
        reflect_test::everything object = {};
        const reflect::type_info *type = type_info_of<reflect_test::everything>();
        REQUIRE(type != nullptr);

        const result<field_ref> unknown = resolve_path(*type, &object, "missing");
        REQUIRE_FALSE(unknown.has_value());
        CHECK(unknown.get_error().generic_code() == error_code::NOT_FOUND);
        CHECK(unknown.get_error().message().find("test.everything") != std::string::npos);

        const result<field_ref> unknownNested = resolve_path(*type, &object, "nested.missing");
        REQUIRE_FALSE(unknownNested.has_value());
        CHECK(unknownNested.get_error().generic_code() == error_code::NOT_FOUND);
        CHECK(unknownNested.get_error().message().find("test.inner") != std::string::npos);

        const result<field_ref> notArray = resolve_path(*type, &object, "single[0]");
        REQUIRE_FALSE(notArray.has_value());
        CHECK(notArray.get_error().generic_code() == error_code::INVALID_ARGUMENT);
        CHECK(notArray.get_error().message().find("not an array") != std::string::npos);

        const result<field_ref> outOfRange = resolve_path(*type, &object, "counts[3]");
        REQUIRE_FALSE(outOfRange.has_value());
        CHECK(outOfRange.get_error().generic_code() == error_code::OUT_OF_RANGE);

        const result<field_ref> badIndex = resolve_path(*type, &object, "counts[x]");
        REQUIRE_FALSE(badIndex.has_value());
        CHECK(badIndex.get_error().generic_code() == error_code::INVALID_ARGUMENT);
        CHECK(badIndex.get_error().message().find("not an array index") != std::string::npos);
        CHECK_FALSE(resolve_path(*type, &object, "counts[-1]").has_value());
        CHECK_FALSE(resolve_path(*type, &object, "counts[]").has_value());

        // Indices are plain digits: a sign or whitespace is malformed even though parse_uint accepts '+'.
        const result<field_ref> signedIndex = resolve_path(*type, &object, "counts[+1]");
        REQUIRE_FALSE(signedIndex.has_value());
        CHECK(signedIndex.get_error().generic_code() == error_code::INVALID_ARGUMENT);
        CHECK(signedIndex.get_error().message().find("'+1' is not an array index") != std::string::npos);
        CHECK_FALSE(resolve_path(*type, &object, "counts[ 1]").has_value());
        CHECK_FALSE(resolve_path(*type, &object, "counts[0x1]").has_value());
        const result<field_ref> leadingZero = resolve_path(*type, &object, "counts[01]");
        REQUIRE(leadingZero.has_value());
        CHECK(leadingZero->data == &object.counts[1]);
        CHECK(leadingZero->path == "counts[1]");

        const result<field_ref> unclosed = resolve_path(*type, &object, "counts[1");
        REQUIRE_FALSE(unclosed.has_value());
        CHECK(unclosed.get_error().message().find("missing ']'") != std::string::npos);

        const result<field_ref> notStruct = resolve_path(*type, &object, "single.x");
        REQUIRE_FALSE(notStruct.has_value());
        CHECK(notStruct.get_error().generic_code() == error_code::INVALID_ARGUMENT);
        CHECK(notStruct.get_error().message().find("not a struct") != std::string::npos);
        CHECK_FALSE(resolve_path(*type, &object, "counts[0].x").has_value());
        CHECK_FALSE(resolve_path(*type, &object, "pose.position").has_value());

        const result<field_ref> empty = resolve_path(*type, &object, "");
        REQUIRE_FALSE(empty.has_value());
        CHECK(empty.get_error().generic_code() == error_code::INVALID_ARGUMENT);
        CHECK_FALSE(resolve_path(*type, &object, ".single").has_value());
        CHECK_FALSE(resolve_path(*type, &object, "nested.").has_value());
        CHECK_FALSE(resolve_path(*type, &object, "nested..weight").has_value());

        const result<field_ref> trailing = resolve_path(*type, &object, "counts[1]x");
        REQUIRE_FALSE(trailing.has_value());
        CHECK(trailing.get_error().message().find("unexpected 'x'") != std::string::npos);

        const result<field_ref> nullObject = resolve_path(*type, nullptr, "single");
        REQUIRE_FALSE(nullObject.has_value());
        CHECK(nullObject.get_error().generic_code() == error_code::INVALID_ARGUMENT);
    }

    TEST_CASE("json_conversion - set_from_json and get_as_json operate on resolved fields and elements") {
        reflect_test::everything object = {};
        const reflect::type_info *type = type_info_of<reflect_test::everything>();
        REQUIRE(type != nullptr);

        const field_ref weight = resolve_path(*type, &object, "parts[1].weight").value();
        REQUIRE(set_from_json(weight, parse("3.5").root()).has_value());
        CHECK(object.parts[1].weight == 3.5f);
        CHECK(get_as_json(weight).value().to_string() == "3.5");

        const field_ref element = resolve_path(*type, &object, "counts[1]").value();
        REQUIRE(set_from_json(element, parse("77").root()).has_value());
        CHECK(object.counts == std::array<i32, 3>{1, 77, 3});
        CHECK(get_as_json(element).value().to_string() == "77");

        const field_ref wholeArray = resolve_path(*type, &object, "counts").value();
        CHECK(get_as_json(wholeArray).value().to_string() == "[1,77,3]");
        REQUIRE(set_from_json(wholeArray, parse("[5, 6, 7]").root()).has_value());
        CHECK(object.counts == std::array<i32, 3>{5, 6, 7});

        const field_ref part = resolve_path(*type, &object, "parts[0]").value();
        REQUIRE(set_from_json(part, parse(R"({"weight": 9})").root()).has_value());
        CHECK(object.parts[0].weight == 9.0f);
        CHECK(get_as_json(part).value() == parse(R"({"offset": [1.0, 2.0, 3.0], "weight": 9.0})"));

        const field_ref pose = resolve_path(*type, &object, "pose").value();
        REQUIRE(set_from_json(pose, parse(R"({"position": [4, 5, 6]})").root()).has_value());
        CHECK(object.pose.position == math::vec3(4.0f, 5.0f, 6.0f));
        CHECK(object.pose.scale == math::vec3(1.0f));

        const field_ref weapon = resolve_path(*type, &object, "weapon").value();
        REQUIRE(set_from_json(weapon, parse("\"SWORD\"").root()).has_value());
        CHECK(object.weapon == reflect_test::weapon_kind::SWORD);
        CHECK(get_as_json(weapon).value().to_string() == "\"SWORD\"");

        // Errors name the resolved path, not just the field name.
        CHECK(element.path == "counts[1]");
        CHECK(weight.path == "parts[1].weight");
        CHECK(wholeArray.path == "counts");
        const result<void> mismatch = set_from_json(element, parse("\"x\"").root());
        REQUIRE_FALSE(mismatch.has_value());
        CHECK(mismatch.get_error().generic_code() == error_code::INVALID_ARGUMENT);
        CHECK(mismatch.get_error().message().find("field 'counts[1]'") != std::string::npos);
        CHECK(object.counts[1] == 6);

        const result<void> deepMismatch = set_from_json(weight, parse("\"x\"").root());
        REQUIRE_FALSE(deepMismatch.has_value());
        CHECK(deepMismatch.get_error().message().find("field 'parts[1].weight'") != std::string::npos);

        const result<void> badElement = set_from_json(wholeArray, parse("[1, \"x\"]").root());
        REQUIRE_FALSE(badElement.has_value());
        CHECK(badElement.get_error().message().find("field 'counts[1]'") != std::string::npos);

        // A reference built by hand without a path falls back to the field name.
        const field_ref bare = {&object.single, type->find_field("single"), false};
        const result<void> bareMismatch = set_from_json(bare, parse("\"x\"").root());
        REQUIRE_FALSE(bareMismatch.has_value());
        CHECK(bareMismatch.get_error().message().find("field 'single'") != std::string::npos);

        const result<void> invalidRef = set_from_json(field_ref{}, parse("1").root());
        REQUIRE_FALSE(invalidRef.has_value());
        CHECK(invalidRef.get_error().generic_code() == error_code::INVALID_ARGUMENT);
        CHECK_FALSE(get_as_json(field_ref{}).has_value());
        json::writer out = {};
        CHECK_FALSE(write_json(field_ref{}, out).has_value());
    }

    TEST_CASE("json_conversion - strings keep every byte including quotes, escapes and UTF-8") {
        reflect_test::everything object = {};
        object.text = std::string("line\nbreak \"quoted\" \\ slash \xE2\x9C\x93 nul") + '\0' + "after";
        reflect_test::everything restored = {};
        REQUIRE(from_json(restored, parse(serialise(object)).root()).has_value());
        CHECK(restored.text == object.text);
        CHECK(restored.text.size() == object.text.size());

        REQUIRE(from_json(restored, parse(R"({"text": ""})").root()).has_value());
        CHECK(restored.text.empty());
    }
}
