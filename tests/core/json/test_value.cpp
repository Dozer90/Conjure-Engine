#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"

#include <array>
#include <cstdint>
#include <format>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::json;

namespace {
    document parse_or_fail(std::string_view text) {
        result<document> parsed = document::parse(text);
        const std::string failure = parsed.has_value() ? std::string() : parsed.get_error().to_string();
        INFO("parse failure: ", failure);
        REQUIRE(parsed.has_value());
        return std::move(parsed).value();
    }

    constexpr std::string_view EVERY_KIND =
      R"({"n":null,"b":true,"f":false,"i":1,"neg":-2,"r":1.5,"s":"x","a":[1,2,3],"o":{"k":"v"}})";

    static_assert(json_convertible<bool>);
    static_assert(json_convertible<i32>);
    static_assert(json_convertible<u64>);
    static_assert(json_convertible<f32>);
    static_assert(json_convertible<std::string>);
    static_assert(json_convertible<uuid>);
    static_assert(json_convertible<std::vector<i32>>);
    static_assert(json_convertible<std::vector<std::vector<uuid>>>);
    static_assert(!json_convertible<value>);
    static_assert(!json_convertible<document>);
}

TEST_SUITE("core/value") {

    TEST_CASE("value - a default value is invalid and every accessor returns its fallback") {
        const value invalid = {};

        CHECK_FALSE(invalid.is_valid());
        CHECK(invalid.owner() == nullptr);
        CHECK(invalid.type() == json_type::NULL_VALUE);
        CHECK(invalid.is_null());
        CHECK_FALSE(invalid.is_bool());
        CHECK_FALSE(invalid.is_number());
        CHECK_FALSE(invalid.is_string());
        CHECK_FALSE(invalid.is_array());
        CHECK_FALSE(invalid.is_object());
        CHECK_FALSE(invalid.is_integer());

        CHECK(invalid.as_bool(true));
        CHECK(invalid.as_i64(-7) == -7);
        CHECK(invalid.as_u64(7) == 7);
        CHECK(invalid.as_f64(2.5) == 2.5);
        CHECK(invalid.as_f32(1.5f) == 1.5f);
        CHECK(invalid.as_string("fallback") == "fallback");
        CHECK(invalid.size() == 0);

        CHECK_FALSE(invalid[0].is_valid());
        CHECK_FALSE(invalid["key"].is_valid());
        CHECK_FALSE(invalid.contains("key"));
        CHECK_FALSE(invalid.get("key").has_value());
        CHECK(invalid.begin() == invalid.end());

        usize visited = 0;
        for ([[maybe_unused]] const auto [key, item] : invalid.members()) {
            ++visited;
        }
        CHECK(visited == 0);
    }

    TEST_CASE("value - type and predicates identify every kind") {
        const document doc = parse_or_fail(EVERY_KIND);
        const value root = doc.root();

        CHECK(root.is_valid());
        CHECK(root.owner() == &doc);
        CHECK(root.type() == json_type::OBJECT);
        CHECK(root.is_object());

        CHECK(root["n"].type() == json_type::NULL_VALUE);
        CHECK(root["n"].is_null());
        CHECK(root["n"].is_valid());

        CHECK(root["b"].type() == json_type::BOOL);
        CHECK(root["b"].is_bool());
        CHECK(root["f"].is_bool());

        CHECK(root["i"].type() == json_type::NUMBER);
        CHECK(root["i"].is_number());
        CHECK(root["i"].is_integer());
        CHECK(root["neg"].is_integer());
        CHECK(root["r"].is_number());
        CHECK_FALSE(root["r"].is_integer());

        CHECK(root["s"].type() == json_type::STRING);
        CHECK(root["s"].is_string());
        CHECK(root["a"].type() == json_type::ARRAY);
        CHECK(root["a"].is_array());
        CHECK(root["o"].type() == json_type::OBJECT);

        CHECK_FALSE(root["b"].is_null());
        CHECK_FALSE(root["s"].is_number());
        CHECK_FALSE(root["a"].is_object());
    }

    TEST_CASE("value - json_type names are stable and formattable") {
        CHECK(to_string(json_type::NULL_VALUE) == "null");
        CHECK(to_string(json_type::BOOL) == "bool");
        CHECK(to_string(json_type::NUMBER) == "number");
        CHECK(to_string(json_type::STRING) == "string");
        CHECK(to_string(json_type::ARRAY) == "array");
        CHECK(to_string(json_type::OBJECT) == "object");
        CHECK(to_string(static_cast<json_type>(99)) == "unknown");
        CHECK(std::format("{}", json_type::ARRAY) == "array");
    }

    TEST_CASE("value - scalar accessors return the value or the fallback") {
        const document doc = parse_or_fail(EVERY_KIND);
        const value root = doc.root();

        CHECK(root["b"].as_bool() == true);
        CHECK(root["f"].as_bool(true) == false);
        CHECK(root["i"].as_bool(true) == true);
        CHECK(root["s"].as_bool() == false);

        CHECK(root["i"].as_i64() == 1);
        CHECK(root["neg"].as_i64() == -2);
        CHECK(root["s"].as_i64(42) == 42);
        CHECK(root["b"].as_i64(42) == 42);

        CHECK(root["i"].as_u64() == 1);
        CHECK(root["s"].as_u64(42) == 42);

        CHECK(root["r"].as_f64() == 1.5);
        CHECK(root["i"].as_f64() == 1.0);
        CHECK(root["neg"].as_f64() == -2.0);
        CHECK(root["s"].as_f64(9.5) == 9.5);

        CHECK(root["r"].as_f32() == 1.5f);
        CHECK(root["neg"].as_f32() == -2.0f);
        CHECK(root["a"].as_f32(3.0f) == 3.0f);

        CHECK(root["s"].as_string() == "x");
        CHECK(root["i"].as_string("none") == "none");
        CHECK(root["n"].as_string("none") == "none");
    }

    TEST_CASE("value - as_i64 and as_u64 respect sign and range") {
        const document doc = parse_or_fail(
          R"({"big":18446744073709551615,"neg":-1,"whole":3.0,"frac":2.5,"huge":1e19,"negzero":-0.0,)"
          R"("min":-9223372036854775808,"toobig":1e300})"
        );
        const value root = doc.root();

        CHECK(root["big"].as_u64() == std::numeric_limits<u64>::max());
        CHECK(root["big"].as_i64(-1) == -1);

        CHECK(root["neg"].as_i64() == -1);
        CHECK(root["neg"].as_u64(99) == 99);

        CHECK(root["whole"].as_i64() == 3);
        CHECK(root["whole"].as_u64() == 3);
        CHECK(root["frac"].as_i64(-5) == -5);
        CHECK(root["frac"].as_u64(5) == 5);

        CHECK(root["huge"].as_u64() == 10000000000000000000ull);
        CHECK(root["huge"].as_i64(-1) == -1);

        CHECK(root["negzero"].as_u64(7) == 0);
        CHECK(root["negzero"].as_i64(7) == 0);

        CHECK(root["min"].as_i64() == std::numeric_limits<i64>::min());
        CHECK(root["min"].as_u64(1) == 1);

        CHECK(root["toobig"].as_i64(-3) == -3);
        CHECK(root["toobig"].as_u64(3) == 3);
    }

    TEST_CASE("value - as_f32 rounds in range and falls back beyond the float range") {
        const document doc = parse_or_fail(
          R"({"huge":1e300,"neg":-1e300,"max":3.4028234663852886e38,"over":3.5e38,"tiny":1e-50,"int":16777217,)"
          R"("big":18446744073709551615})"
        );
        const value root = doc.root();

        CHECK(root["huge"].as_f32(1.5f) == 1.5f);
        CHECK(root["neg"].as_f32(-1.5f) == -1.5f);
        CHECK(root["max"].as_f32() == std::numeric_limits<f32>::max());
        CHECK(root["over"].as_f32(2.0f) == 2.0f);
        CHECK(root["tiny"].as_f32(9.0f) < 1e-40f);
        CHECK(root["int"].as_f32() == 16777216.0f);
        CHECK(root["big"].as_f32() == 18446744073709551615.0f);

        // The checked getter and the converter agree on where the range ends.
        CHECK_FALSE(root["huge"].to<f32>().has_value());
        CHECK_FALSE(root["over"].to<f32>().has_value());
        CHECK(root["max"].to<f32>().value() == std::numeric_limits<f32>::max());
    }

    TEST_CASE("value - as_string returns the decoded bytes including embedded zeros") {
        const document doc = parse_or_fail(R"({"zero":"a\u0000b","accent":"é","escapes":"\n\t\"\\/"})");
        const value root = doc.root();

        const std::string_view withZero = root["zero"].as_string();
        REQUIRE(withZero.size() == 3);
        CHECK(withZero[0] == 'a');
        CHECK(withZero[1] == '\0');
        CHECK(withZero[2] == 'b');

        CHECK(root["accent"].as_string() == "\xC3\xA9");
        CHECK(root["escapes"].as_string() == "\n\t\"\\/");
    }

    TEST_CASE("value - indexing never asserts and yields invalid values for misses") {
        testing::expect_assert asserts = {};
        const document doc = parse_or_fail(EVERY_KIND);
        const value root = doc.root();

        CHECK(root["a"][0].as_i64() == 1);
        CHECK(root["a"][2].as_i64() == 3);
        CHECK_FALSE(root["a"][3].is_valid());
        CHECK_FALSE(root["a"]["key"].is_valid());
        CHECK_FALSE(root["o"][0].is_valid());
        CHECK_FALSE(root["missing"].is_valid());
        CHECK_FALSE(root["missing"]["deeper"][4].is_valid());
        CHECK_FALSE(root["i"][0].is_valid());
        CHECK_FALSE(root["s"]["x"].is_valid());
        CHECK(root["missing"]["width"].as_i64(1280) == 1280);

        CHECK(asserts.count() == 0);
    }

    TEST_CASE("value - contains and get look up object members") {
        const document doc = parse_or_fail(EVERY_KIND);
        const value root = doc.root();

        CHECK(root.contains("o"));
        CHECK(root.contains("n"));
        CHECK_FALSE(root.contains("missing"));
        CHECK_FALSE(root["a"].contains("0"));
        CHECK_FALSE(root["s"].contains("s"));

        const std::optional<value> found = root.get("o");
        REQUIRE(found.has_value());
        CHECK(found->is_object());
        CHECK((*found)["k"].as_string() == "v");
        CHECK_FALSE(root.get("missing").has_value());
        CHECK_FALSE(root["a"].get("k").has_value());
    }

    TEST_CASE("value - lookups return the first of duplicate keys") {
        const document doc = parse_or_fail(R"({"a":1,"b":"x","a":2,"":3,"":4})");
        const value root = doc.root();

        CHECK(root.size() == 5);
        CHECK(root["a"].as_i64() == 1);
        CHECK(root.get("a")->as_i64() == 1);
        CHECK(root.contains("a"));
        CHECK(root[""].as_i64() == 3);
        CHECK(root[std::string_view{}].as_i64() == 3);
        CHECK(root.contains(std::string_view{}));
        CHECK(root.get(std::string_view{})->as_i64() == 3);

        usize duplicates = 0;
        for (const auto [key, item] : root.members()) {
            if (key == "a") {
                ++duplicates;
            }
        }
        CHECK(duplicates == 2);
    }

    TEST_CASE("value - size counts elements and members") {
        const document doc = parse_or_fail(EVERY_KIND);
        const value root = doc.root();

        CHECK(root.size() == 9);
        CHECK(root["a"].size() == 3);
        CHECK(root["o"].size() == 1);
        CHECK(root["s"].size() == 0);
        CHECK(root["i"].size() == 0);
        CHECK(parse_or_fail("[]").root().size() == 0);
        CHECK(parse_or_fail("{}").root().size() == 0);
    }

    TEST_CASE("value - array iteration visits every element in order") {
        const document doc = parse_or_fail(R"([10, "two", [3], {"four": 4}, null])");
        const value root = doc.root();

        std::vector<json_type> types = {};
        for (const value item : root) {
            types.push_back(item.type());
        }
        REQUIRE(types.size() == 5);
        CHECK(types[0] == json_type::NUMBER);
        CHECK(types[1] == json_type::STRING);
        CHECK(types[2] == json_type::ARRAY);
        CHECK(types[3] == json_type::OBJECT);
        CHECK(types[4] == json_type::NULL_VALUE);

        value::array_iterator iterator = root.begin();
        CHECK((*iterator).as_i64() == 10);
        const value::array_iterator before = iterator++;
        CHECK((*before).as_i64() == 10);
        CHECK((*iterator).as_string() == "two");
        CHECK(before != iterator);
        ++iterator;
        ++iterator;
        ++iterator;
        ++iterator;
        CHECK(iterator == root.end());
        ++iterator;
        CHECK(iterator == root.end());

        const value::array_iterator again = root.begin();
        CHECK(again == root.begin());
    }

    TEST_CASE("value - iterating a non-array or an empty array visits nothing") {
        const document doc = parse_or_fail(R"({"empty":[],"object":{"a":1},"scalar":5})");
        const value root = doc.root();

        usize visited = 0;
        for ([[maybe_unused]] const value item : root["empty"]) {
            ++visited;
        }
        for ([[maybe_unused]] const value item : root["object"]) {
            ++visited;
        }
        for ([[maybe_unused]] const value item : root["scalar"]) {
            ++visited;
        }
        CHECK(visited == 0);
        CHECK(root["object"].begin() == root["object"].end());
    }

    TEST_CASE("value - object iteration yields keys and values in document order") {
        const document doc = parse_or_fail(R"({"zeta":1,"alpha":"a","mid":[true],"":null})");
        const value root = doc.root();

        std::vector<std::string> keys = {};
        std::vector<json_type> types = {};
        for (const auto [key, item] : root.members()) {
            keys.emplace_back(key);
            types.push_back(item.type());
        }
        REQUIRE(keys.size() == 4);
        CHECK(keys[0] == "zeta");
        CHECK(keys[1] == "alpha");
        CHECK(keys[2] == "mid");
        CHECK(keys[3] == "");
        CHECK(types[0] == json_type::NUMBER);
        CHECK(types[1] == json_type::STRING);
        CHECK(types[2] == json_type::ARRAY);
        CHECK(types[3] == json_type::NULL_VALUE);

        value::object_iterator iterator = root.members().begin();
        const member first = *iterator;
        CHECK(first.key == "zeta");
        CHECK(first.item.as_i64() == 1);
        CHECK(first.item.owner() == &doc);
        const value::object_iterator before = iterator++;
        CHECK((*before).key == "zeta");
        CHECK((*iterator).key == "alpha");
        ++iterator;
        ++iterator;
        ++iterator;
        CHECK(iterator == root.members().end());
    }

    TEST_CASE("value - iterating members of a non-object or an empty object visits nothing") {
        const document doc = parse_or_fail(R"({"empty":{},"array":[1,2],"scalar":"s"})");
        const value root = doc.root();

        usize visited = 0;
        for ([[maybe_unused]] const auto [key, item] : root["empty"].members()) {
            ++visited;
        }
        for ([[maybe_unused]] const auto [key, item] : root["array"].members()) {
            ++visited;
        }
        for ([[maybe_unused]] const auto [key, item] : root["scalar"].members()) {
            ++visited;
        }
        CHECK(visited == 0);
    }

    TEST_CASE("value - to<T> converts bool, numbers, strings and uuids") {
        const document doc = parse_or_fail(
          R"({"b":true,"i":-42,"u":42,"r":2.5,"s":"text","id":"0123abcd-4567-4000-8000-000000000001"})"
        );
        const value root = doc.root();

        CHECK(root["b"].to<bool>().value() == true);
        CHECK(root["i"].to<i8>().value() == -42);
        CHECK(root["i"].to<i16>().value() == -42);
        CHECK(root["i"].to<i32>().value() == -42);
        CHECK(root["i"].to<i64>().value() == -42);
        CHECK(root["u"].to<u8>().value() == 42);
        CHECK(root["u"].to<u16>().value() == 42);
        CHECK(root["u"].to<u32>().value() == 42);
        CHECK(root["u"].to<u64>().value() == 42);
        CHECK(root["u"].to<i32>().value() == 42);
        CHECK(root["r"].to<f32>().value() == 2.5f);
        CHECK(root["r"].to<f64>().value() == 2.5);
        CHECK(root["i"].to<f64>().value() == -42.0);
        CHECK(root["u"].to<f32>().value() == 42.0f);
        CHECK(root["s"].to<std::string>().value() == "text");

        const result<uuid> id = root["id"].to<uuid>();
        REQUIRE(id.has_value());
        CHECK(id->to_string() == "0123abcd-4567-4000-8000-000000000001");
    }

    TEST_CASE("value - to<T> range-checks integers") {
        const document doc = parse_or_fail(
          R"({"u8max":255,"u8over":256,"i8min":-128,"i8under":-129,"i32over":2147483648,"neg":-1,)"
          R"("u64max":18446744073709551615,"i64min":-9223372036854775808,"whole":3.0,"frac":2.5,)"
          R"("huge":1e19,"negwhole":-4.0,"realover":1e300})"
        );
        const value root = doc.root();

        CHECK(root["u8max"].to<u8>().value() == 255);
        REQUIRE_FALSE(root["u8over"].to<u8>().has_value());
        CHECK(root["u8over"].to<u8>().get_error().generic_code() == error_code::OUT_OF_RANGE);
        CHECK(root["u8over"].to<u8>().get_error().message().find("256") != std::string::npos);
        CHECK(root["u8over"].to<u8>().get_error().message().find("u8") != std::string::npos);
        CHECK(root["u8over"].to<u16>().value() == 256);

        CHECK(root["i8min"].to<i8>().value() == -128);
        CHECK(root["i8under"].to<i8>().get_error().generic_code() == error_code::OUT_OF_RANGE);
        CHECK(root["i8under"].to<i16>().value() == -129);

        CHECK(root["i32over"].to<i32>().get_error().generic_code() == error_code::OUT_OF_RANGE);
        CHECK(root["i32over"].to<u32>().value() == 2147483648u);
        CHECK(root["i32over"].to<i64>().value() == 2147483648ll);

        CHECK(root["neg"].to<u8>().get_error().generic_code() == error_code::OUT_OF_RANGE);
        CHECK(root["neg"].to<u64>().get_error().generic_code() == error_code::OUT_OF_RANGE);
        CHECK(root["neg"].to<i64>().value() == -1);

        CHECK(root["u64max"].to<u64>().value() == std::numeric_limits<u64>::max());
        CHECK(root["u64max"].to<i64>().get_error().generic_code() == error_code::OUT_OF_RANGE);
        CHECK(root["i64min"].to<i64>().value() == std::numeric_limits<i64>::min());
        CHECK(root["i64min"].to<u64>().get_error().generic_code() == error_code::OUT_OF_RANGE);

        CHECK(root["whole"].to<i32>().value() == 3);
        CHECK(root["whole"].to<u8>().value() == 3);
        CHECK(root["negwhole"].to<i16>().value() == -4);
        CHECK(root["negwhole"].to<u16>().get_error().generic_code() == error_code::OUT_OF_RANGE);

        REQUIRE_FALSE(root["frac"].to<i32>().has_value());
        CHECK(root["frac"].to<i32>().get_error().generic_code() == error_code::INVALID_ARGUMENT);
        CHECK(root["frac"].to<i32>().get_error().message().find("2.5") != std::string::npos);

        CHECK(root["huge"].to<u64>().value() == 10000000000000000000ull);
        CHECK(root["huge"].to<i64>().get_error().generic_code() == error_code::OUT_OF_RANGE);
        CHECK(root["realover"].to<u64>().get_error().generic_code() == error_code::OUT_OF_RANGE);
        CHECK(root["realover"].to<i64>().get_error().generic_code() == error_code::OUT_OF_RANGE);
    }

    TEST_CASE("value - to<T> reports type mismatches, missing values and bad strings") {
        const document doc = parse_or_fail(R"({"n":5,"s":"text","bad":"not-a-uuid","huge":1e300,"neg":-1e300})");
        const value root = doc.root();

        const result<bool> notBool = root["n"].to<bool>();
        REQUIRE_FALSE(notBool.has_value());
        CHECK(notBool.get_error().generic_code() == error_code::INVALID_ARGUMENT);
        CHECK(notBool.get_error().message() == "expected bool, found number");

        CHECK(root["s"].to<i32>().get_error().message() == "expected integer, found string");
        CHECK(root["s"].to<f64>().get_error().message() == "expected number, found string");
        CHECK(root["n"].to<std::string>().get_error().message() == "expected string, found number");
        CHECK(root["n"].to<uuid>().get_error().generic_code() == error_code::INVALID_ARGUMENT);

        const result<i32> missing = root["missing"].to<i32>();
        REQUIRE_FALSE(missing.has_value());
        CHECK(missing.get_error().generic_code() == error_code::NOT_FOUND);
        CHECK(value().to<std::string>().get_error().generic_code() == error_code::NOT_FOUND);
        CHECK(value().to<bool>().get_error().generic_code() == error_code::NOT_FOUND);
        CHECK(value().to<f32>().get_error().generic_code() == error_code::NOT_FOUND);
        CHECK(value().to<uuid>().get_error().generic_code() == error_code::NOT_FOUND);

        const result<uuid> badId = root["bad"].to<uuid>();
        REQUIRE_FALSE(badId.has_value());
        CHECK(badId.get_error().generic_code() == error_code::PARSE_FAILURE);
        CHECK(badId.get_error().message().find("not-a-uuid") != std::string::npos);

        CHECK(root["huge"].to<f32>().get_error().generic_code() == error_code::OUT_OF_RANGE);
        CHECK(root["neg"].to<f32>().get_error().generic_code() == error_code::OUT_OF_RANGE);
        CHECK(root["huge"].to<f64>().value() == 1e300);
    }

    TEST_CASE("value - to<std::vector<T>> converts arrays element by element") {
        const document doc = parse_or_fail(
          R"({"ints":[1,2,3],"nested":[[1],[2,3],[]],"names":["a","b"],"bad":[1,"two",3],"notArray":{}})"
        );
        const value root = doc.root();

        const result<std::vector<i32>> ints = root["ints"].to<std::vector<i32>>();
        REQUIRE(ints.has_value());
        CHECK(*ints == std::vector<i32>{1, 2, 3});

        const result<std::vector<std::vector<u8>>> nested = root["nested"].to<std::vector<std::vector<u8>>>();
        REQUIRE(nested.has_value());
        REQUIRE(nested->size() == 3);
        CHECK((*nested)[1] == std::vector<u8>{2, 3});
        CHECK((*nested)[2].empty());

        const result<std::vector<std::string>> names = root["names"].to<std::vector<std::string>>();
        REQUIRE(names.has_value());
        CHECK(*names == std::vector<std::string>{"a", "b"});

        const result<std::vector<i32>> bad = root["bad"].to<std::vector<i32>>();
        REQUIRE_FALSE(bad.has_value());
        CHECK(bad.get_error().generic_code() == error_code::INVALID_ARGUMENT);
        CHECK(bad.get_error().message() == "element 1: expected integer, found string");

        const result<std::vector<i32>> notArray = root["notArray"].to<std::vector<i32>>();
        REQUIRE_FALSE(notArray.has_value());
        CHECK(notArray.get_error().message() == "expected array, found object");

        CHECK(root["missing"].to<std::vector<i32>>().get_error().generic_code() == error_code::NOT_FOUND);

        const result<std::vector<u8>> overflow = parse_or_fail("[1, 300]").root().to<std::vector<u8>>();
        REQUIRE_FALSE(overflow.has_value());
        CHECK(overflow.get_error().generic_code() == error_code::OUT_OF_RANGE);
        CHECK(overflow.get_error().message().find("element 1") != std::string::npos);

        CHECK(parse_or_fail("[]").root().to<std::vector<f64>>().value().empty());
    }

    TEST_CASE("value - structural equality ignores member order but not content") {
        const document left = parse_or_fail(R"({"a":1,"b":[1,2,{"c":"d"}],"e":null})");
        const document right = parse_or_fail(R"({"e":null,"b":[1,2,{"c":"d"}],"a":1})");
        const document different = parse_or_fail(R"({"a":1,"b":[1,2,{"c":"x"}],"e":null})");
        const document shorter = parse_or_fail(R"({"a":1,"b":[1,2,{"c":"d"}]})");
        const document longerArray = parse_or_fail(R"({"a":1,"b":[1,2,{"c":"d"},3],"e":null})");

        CHECK(left.root() == right.root());
        CHECK(right.root() == left.root());
        CHECK(left.root() != different.root());
        CHECK(left.root() != shorter.root());
        CHECK(left.root() != longerArray.root());
        CHECK(left.root()["b"] == right.root()["b"]);
        CHECK(left.root()["b"][2] == right.root()["b"][2]);
        CHECK(left.root()["a"] != left.root()["e"]);
    }

    TEST_CASE("value - structural equality of scalars follows the canonical text") {
        const document doc = parse_or_fail(R"([1, 1.0, -0.0, 0.0, "1", true, null, 18446744073709551615, -1])");
        const value root = doc.root();

        CHECK(root[0] == parse_or_fail("1").root());
        CHECK(root[0] != root[1]);
        CHECK(root[1] == parse_or_fail("1.0").root());
        CHECK(root[2] != root[3]);
        CHECK(root[2] == parse_or_fail("-0.0").root());
        CHECK(root[0] != root[4]);
        CHECK(root[5] == parse_or_fail("true").root());
        CHECK(root[5] != parse_or_fail("false").root());
        CHECK(root[6] == parse_or_fail("null").root());
        CHECK(root[7] == parse_or_fail("18446744073709551615").root());
        CHECK(root[8] == parse_or_fail("-1").root());
        CHECK(root[8] != root[0]);

        CHECK(value() == value());
        CHECK(value() != root[6]);
        CHECK(root[6] != value());
    }

    TEST_CASE("value - structural equality pairs duplicate keys in order and never over-matches") {
        CHECK(parse_or_fail(R"({"a":1,"a":2})").root() == parse_or_fail(R"({"a":1,"a":2})").root());
        CHECK(parse_or_fail(R"({"a":1,"a":2})").root() != parse_or_fail(R"({"a":2,"a":1})").root());
        CHECK(parse_or_fail(R"({"a":1,"b":2,"a":3})").root() == parse_or_fail(R"({"b":2,"a":1,"a":3})").root());
        CHECK(parse_or_fail(R"({"a":1,"b":2,"a":3})").root() != parse_or_fail(R"({"b":2,"a":3,"a":1})").root());
        CHECK(parse_or_fail(R"({"a":1,"a":2})").root() != parse_or_fail(R"({"a":1,"b":2})").root());
        CHECK(parse_or_fail(R"({"a":1,"b":2})").root() != parse_or_fail(R"({"a":1,"a":2})").root());
        CHECK(parse_or_fail(R"({"a":1,"b":2})").root() != parse_or_fail(R"({"a":1,"c":2})").root());
        CHECK(parse_or_fail(R"({"a":1,"a":1})").root() != parse_or_fail(R"({"a":1,"a":2})").root());
        CHECK(parse_or_fail(R"({"a":{"x":1},"a":{"x":2}})").root() ==
          parse_or_fail(R"({"a":{"x":1},"a":{"x":2}})").root());
        CHECK(parse_or_fail(R"({"a":{"x":1},"a":{"x":2}})").root() !=
          parse_or_fail(R"({"a":{"x":2},"a":{"x":1}})").root());

        // Equality tracks the canonical text exactly, including for duplicates.
        const std::array<std::string_view, 4> texts = {
            R"({"b":1,"a":2,"b":3,"a":4})",
            R"({"a":2,"a":4,"b":1,"b":3})",
            R"({"a":4,"a":2,"b":1,"b":3})",
            R"({"a":2,"b":3,"a":4,"b":1})",
        };
        for (const std::string_view left : texts) {
            for (const std::string_view right : texts) {
                CAPTURE(left);
                CAPTURE(right);
                const document leftDoc = parse_or_fail(left);
                const document rightDoc = parse_or_fail(right);
                CHECK((leftDoc == rightDoc) == (leftDoc.to_canonical_string() == rightDoc.to_canonical_string()));
            }
        }
    }
}
