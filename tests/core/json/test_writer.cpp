#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "conjure/core/json/document.h"
#include "conjure/core/json/json.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/platform_detect.h"

#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::json;

namespace {
    std::string write_scalar_array(std::string_view text) {
        writer out = {};
        out.begin_array();
        out.value(text);
        out.end_array();
        return out.finish();
    }

    std::string write_scalar_array(f64 number) {
        writer out = {};
        out.begin_array();
        out.value(number);
        out.end_array();
        return out.finish();
    }

    std::string write_single_member(std::string_view key) {
        writer out = {};
        out.begin_object();
        out.key(key);
        out.value(1);
        out.end_object();
        return out.finish();
    }

    constexpr bool DEBUG_BUILD = CONJURE_DEBUG != 0;
}

TEST_SUITE("core/writer") {

    TEST_CASE("writer - writes every scalar kind as the root and is reusable after finish") {
        writer out = {};

        out.value(true);
        CHECK(out.finish() == "true");
        out.value(false);
        CHECK(out.finish() == "false");
        out.value(nullptr);
        CHECK(out.finish() == "null");
        out.value(i32(-12));
        CHECK(out.finish() == "-12");
        out.value(u32(4000000000u));
        CHECK(out.finish() == "4000000000");
        out.value(std::numeric_limits<i64>::min());
        CHECK(out.finish() == "-9223372036854775808");
        out.value(std::numeric_limits<u64>::max());
        CHECK(out.finish() == "18446744073709551615");
        out.value(1.5);
        CHECK(out.finish() == "1.5");
        out.value(0.25f);
        CHECK(out.finish() == "0.25");
        out.value(std::string_view("view"));
        CHECK(out.finish() == "\"view\"");
        out.value("literal");
        CHECK(out.finish() == "\"literal\"");

        const std::string text = "owned";
        out.value(text);
        CHECK(out.finish() == "\"owned\"");
    }

    TEST_CASE("writer - writes nested objects and arrays with commas and colons in the right places") {
        writer out = {};
        out.begin_object();
        out.key("name");
        out.value("player");
        out.key("tags");
        out.begin_array();
        out.value(1);
        out.value(2);
        out.begin_object();
        out.end_object();
        out.begin_array();
        out.end_array();
        out.end_array();
        out.key("empty");
        out.begin_object();
        out.end_object();
        out.key("flag");
        out.value(false);
        out.end_object();

        CHECK(out.is_complete());
        CHECK(out.finish() == R"({"name":"player","tags":[1,2,{},[]],"empty":{},"flag":false})");
    }

    TEST_CASE("writer - escapes strings exactly like the document writer") {
        const std::array<std::string_view, 9> samples = {
            "plain",
            "quote\" backslash\\ slash/",
            "controls \b\t\n\f\r",
            std::string_view("other controls \x01\x1F\x00\x7F", 19),
            "unicode \xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80",
            "",
            "   spaces   ",
            "ends with escape\n",
            "\"",
        };

        for (const std::string_view sample : samples) {
            CAPTURE(sample);
            const document expected = array({sample});
            CHECK(write_scalar_array(sample) == expected.to_string());
            CHECK(write_scalar_array(sample) == expected.to_canonical_string());
        }

        CHECK(write_scalar_array(std::string_view("\x01\x1F", 2)) == R"(["\u0001\u001F"])");
        CHECK(write_scalar_array("\"\\/") == R"(["\"\\/"])");
        CHECK(write_scalar_array(std::string_view{}) == R"([""])");
    }

    TEST_CASE("writer - escapes keys exactly like the document writer") {
        const std::array<std::string_view, 8> keys = {
            "plain",
            "tab\t",
            "\"q\"",
            "back\\slash/",
            std::string_view("\x01\x1F\x00", 3),
            "\xC3\xA9 \xF0\x9F\x98\x80",
            "",
            "\n",
        };

        for (const std::string_view key : keys) {
            CAPTURE(key);
            const document expected = object({{key, 1}});
            CHECK(write_single_member(key) == expected.to_canonical_string());
            CHECK(write_single_member(key) == expected.to_string());
            const result<document> reparsed = parse(write_single_member(key));
            REQUIRE(reparsed.has_value());
            CHECK(*reparsed == expected);
        }

        CHECK(write_single_member("tab\t") == R"({"tab\t":1})");
        CHECK(write_single_member("\"q\"") == R"({"\"q\"":1})");
        CHECK(write_single_member(std::string_view{}) == R"({"":1})");
    }

    TEST_CASE("writer - invalid UTF-8 asserts in Debug and is written as-is") {
        testing::expect_assert asserts = {};
        writer out = {};

        out.begin_array();
        out.value("\xFF\xFE");
        CHECK(asserts.count() == (DEBUG_BUILD ? 1 : 0));
        out.value("\xC3\xA9");
        CHECK(asserts.count() == (DEBUG_BUILD ? 1 : 0));
        out.value(std::string_view("\xED\xA0\x80", 3));
        CHECK(asserts.count() == (DEBUG_BUILD ? 2 : 0));
        out.value(std::string_view("\xC0\x80", 2));
        CHECK(asserts.count() == (DEBUG_BUILD ? 3 : 0));
        out.value(std::string_view("\xF4\x90\x80\x80", 4));
        CHECK(asserts.count() == (DEBUG_BUILD ? 4 : 0));
        out.value(std::string_view("\xE2\x82", 2));
        CHECK(asserts.count() == (DEBUG_BUILD ? 5 : 0));
        out.value(std::string_view("\xF0\x9F\x98\x80", 4));
        CHECK(asserts.count() == (DEBUG_BUILD ? 5 : 0));
        out.end_array();

        const std::string text = out.finish();
        CHECK(text ==
          "[\"\xFF\xFE\",\"\xC3\xA9\",\"\xED\xA0\x80\",\"\xC0\x80\","
          "\"\xF4\x90\x80\x80\",\"\xE2\x82\",\"\xF0\x9F\x98\x80\"]");
        CHECK_FALSE(parse(text).has_value());

        out.begin_object();
        out.key("\xFF");
        CHECK(asserts.count() == (DEBUG_BUILD ? 6 : 0));
        out.value(1);
        out.end_object();
        CHECK(out.finish() == "{\"\xFF\":1}");
    }

    TEST_CASE("writer - integral values of every width are written as plain digits") {
        writer out = {};
        out.begin_array();
        out.value(5L);
        out.value(6UL);
        out.value(sizeof(u64));
        out.value(u8(200));
        out.value(i8(-5));
        out.value(static_cast<short>(-3));
        out.value(static_cast<unsigned short>(7));
        out.value(-9LL);
        out.value(9ULL);
        out.end_array();
        CHECK(out.finish() == "[5,6,8,200,-5,-3,7,-9,9]");
    }

    TEST_CASE("writer - formats reals exactly like the document writer") {
        const std::array<f64, 20> samples = {
            0.0, -0.0, 1.0, -1.0, 0.1, 0.5, 1.5, 3.14159, 1e21, 1e22, 1e-7, 0.000001, 123456789.125,
            1.7976931348623157e308, 5e-324, 2.2250738585072014e-308, 1e300, -1e-300, 4294967296.0, 0.30000000000000004,
        };

        for (const f64 sample : samples) {
            CAPTURE(sample);
            const document expected = array({sample});
            CHECK(write_scalar_array(sample) == expected.to_string());
        }

        CHECK(write_scalar_array(-0.0) == "[-0.0]");
        CHECK(write_scalar_array(1.0) == "[1.0]");
        CHECK(write_scalar_array(1e21) == "[1e21]");
        CHECK(write_scalar_array(1e-7) == "[1e-7]");
        CHECK(write_scalar_array(0.1) == "[0.1]");
    }

    TEST_CASE("writer - non-finite reals assert and are written as null") {
        testing::expect_assert asserts = {};
        writer out = {};

        out.begin_array();
        out.value(std::numeric_limits<f64>::quiet_NaN());
        out.value(std::numeric_limits<f64>::infinity());
        out.value(-std::numeric_limits<f64>::infinity());
        out.end_array();

        CHECK(asserts.count() == 3);
        CHECK(out.finish() == "[null,null,null]");
    }

    TEST_CASE("writer - output is byte-identical to the canonical document text when keys are sorted") {
        document doc = document::object();
        doc.set("zulu", "last");
        doc.set("alpha", -5);
        doc.set("mike", 2.5);
        document inner = document::object();
        inner.set("y", true);
        inner.set("x", nullptr);
        inner.set("list", array({1, "two", 3.0, false}));
        doc.set("inner", inner);
        doc.set("big", std::numeric_limits<u64>::max());
        doc.set("text", "tab\tquote\"\xC3\xA9");

        writer out = {};
        out.begin_object();
        out.key("alpha");
        out.value(-5);
        out.key("big");
        out.value(std::numeric_limits<u64>::max());
        out.key("inner");
        out.begin_object();
        out.key("list");
        out.begin_array();
        out.value(1);
        out.value("two");
        out.value(3.0);
        out.value(false);
        out.end_array();
        out.key("x");
        out.value(nullptr);
        out.key("y");
        out.value(true);
        out.end_object();
        out.key("mike");
        out.value(2.5);
        out.key("text");
        out.value("tab\tquote\"\xC3\xA9");
        out.key("zulu");
        out.value("last");
        out.end_object();

        const std::string written = out.finish();
        CHECK(written == doc.to_canonical_string());
        CHECK(written == to_canonical_string(doc.root()));
        CHECK(written != doc.to_string());

        const result<document> reparsed = parse(written);
        REQUIRE(reparsed.has_value());
        CHECK(*reparsed == doc);
    }

    TEST_CASE("writer - write_raw_json inserts pre-serialised text verbatim") {
        writer out = {};
        out.begin_object();
        out.key("raw");
        out.write_raw_json(R"({"x":[1,2],"y":"z"})");
        out.key("after");
        out.write_raw_json("7");
        out.end_object();

        const std::string text = out.finish();
        CHECK(text == R"({"raw":{"x":[1,2],"y":"z"},"after":7})");
        CHECK(parse(text).has_value());

        out.begin_array();
        out.write_raw_json("1");
        out.write_raw_json("[2]");
        out.end_array();
        CHECK(out.finish() == "[1,[2]]");

        out.write_raw_json("\"root\"");
        CHECK(out.finish() == "\"root\"");
    }

#if CONJURE_DEBUG
    TEST_CASE("writer - write_raw_json validates its input in Debug") {
        testing::expect_assert asserts = {};
        writer out = {};

        out.begin_array();
        out.write_raw_json("{not json");
        CHECK(asserts.count() == 1);
        out.write_raw_json("");
        CHECK(asserts.count() == 2);
        out.write_raw_json("[1,2]");
        CHECK(asserts.count() == 2);
        out.end_array();
        CHECK(out.finish() == "[{not json,,[1,2]]");
    }
#endif

    TEST_CASE("writer - a key outside an object asserts and is ignored") {
        testing::expect_assert asserts = {};
        writer out = {};

        out.key("root");
        CHECK(asserts.count() == 1);
        out.begin_array();
        out.key("inArray");
        CHECK(asserts.count() == 2);
        out.value(1);
        out.end_array();

        CHECK(out.finish() == "[1]");
    }

    TEST_CASE("writer - a value inside an object without a key asserts and is ignored") {
        testing::expect_assert asserts = {};
        writer out = {};

        out.begin_object();
        out.value(1);
        CHECK(asserts.count() == 1);
        out.begin_array();
        CHECK(asserts.count() == 2);
        out.write_raw_json("3");
        CHECK(asserts.count() == 3);
        out.key("ok");
        out.value(2);
        out.end_object();

        CHECK(out.finish() == R"({"ok":2})");
    }

    TEST_CASE("writer - two keys in a row assert and the second is ignored") {
        testing::expect_assert asserts = {};
        writer out = {};

        out.begin_object();
        out.key("first");
        out.key("second");
        CHECK(asserts.count() == 1);
        out.value(1);
        out.end_object();

        CHECK(out.finish() == R"({"first":1})");
    }

    TEST_CASE("writer - unbalanced ends assert and are ignored") {
        testing::expect_assert asserts = {};
        writer out = {};

        out.end_object();
        CHECK(asserts.count() == 1);
        out.end_array();
        CHECK(asserts.count() == 2);

        out.begin_object();
        out.end_array();
        CHECK(asserts.count() == 3);
        out.end_object();
        CHECK(asserts.count() == 3);

        CHECK(out.finish() == "{}");

        out.begin_array();
        out.end_object();
        CHECK(asserts.count() == 4);
        out.end_array();
        CHECK(out.finish() == "[]");
    }

    TEST_CASE("writer - ending an object with a pending key asserts and writes null for it") {
        testing::expect_assert asserts = {};
        writer out = {};

        out.begin_object();
        out.key("dangling");
        out.end_object();

        CHECK(asserts.count() == 1);
        CHECK(out.finish() == R"({"dangling":null})");
    }

    TEST_CASE("writer - a second root value asserts and is ignored") {
        testing::expect_assert asserts = {};
        writer out = {};

        out.value(1);
        out.value(2);
        CHECK(asserts.count() == 1);
        out.begin_object();
        CHECK(asserts.count() == 2);
        out.write_raw_json("3");
        CHECK(asserts.count() == 3);

        CHECK(out.finish() == "1");
    }

    TEST_CASE("writer - finish asserts on open containers or a missing root but still returns the text") {
        testing::expect_assert asserts = {};
        writer out = {};

        CHECK(out.finish() == "");
        CHECK(asserts.count() == 1);

        out.begin_object();
        out.key("a");
        out.begin_array();
        CHECK_FALSE(out.is_complete());
        CHECK(out.depth() == 2);
        CHECK(out.finish() == R"({"a":[)");
        CHECK(asserts.count() == 2);

        CHECK(out.depth() == 0);
        CHECK_FALSE(out.is_complete());
        out.value(true);
        CHECK(out.is_complete());
        CHECK(out.finish() == "true");
        CHECK(asserts.count() == 2);
    }

    TEST_CASE("writer - reset discards partial output") {
        writer out = {};

        out.begin_object();
        out.key("a");
        CHECK(out.depth() == 1);
        CHECK(out.text() == R"({"a":)");

        out.reset();
        CHECK(out.depth() == 0);
        CHECK(out.text().empty());
        CHECK_FALSE(out.is_complete());

        out.begin_array();
        out.value(1);
        out.end_array();
        CHECK(out.finish() == "[1]");
    }

    TEST_CASE("writer - depth, is_complete and text report progress") {
        writer out = {};

        CHECK(out.depth() == 0);
        CHECK_FALSE(out.is_complete());
        CHECK(out.text().empty());

        out.begin_object();
        CHECK(out.depth() == 1);
        out.key("list");
        out.begin_array();
        CHECK(out.depth() == 2);
        CHECK(out.text() == R"({"list":[)");
        out.end_array();
        CHECK(out.depth() == 1);
        CHECK_FALSE(out.is_complete());
        out.end_object();
        CHECK(out.depth() == 0);
        CHECK(out.is_complete());
        CHECK(out.text() == R"({"list":[]})");
    }

    TEST_CASE("writer - a null C string writes null like the document API") {
        testing::expect_assert asserts = {};
        writer out = {};

        out.begin_array();
        out.value(static_cast<const char *>(nullptr));
        out.end_array();

        CHECK(asserts.count() == 0);
        CHECK(out.finish() == "[null]");
        CHECK(array({static_cast<const char *>(nullptr)}).to_canonical_string() == "[null]");
    }

    TEST_CASE("writer - writers can be copied and moved with their state") {
        writer original = {};
        original.begin_array();
        original.value(1);

        writer copy = original;
        copy.value(2);
        copy.end_array();
        CHECK(copy.finish() == "[1,2]");

        original.value(3);
        original.end_array();
        writer moved = std::move(original);
        CHECK(moved.finish() == "[1,3]");
    }
}
