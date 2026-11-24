#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"

#include "conjure/core/json/json.h"

#include <array>
#include <string>
#include <string_view>
#include <utility>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::json;

namespace {
    // True while a json::to_string overload accepts a second, bool argument.
    template <typename T>
    concept callable_with_flag = requires(const T &item) { conjure::core::json::to_string(item, true); };

    document parse_or_fail(std::string_view text) {
        result<document> parsed = parse(text);
        const std::string failure = parsed.has_value() ? std::string() : parsed.get_error().to_string();
        INFO("parse failure: ", failure);
        REQUIRE(parsed.has_value());
        return std::move(parsed).value();
    }
}

TEST_SUITE("core/json") {

    TEST_CASE("json - the umbrella header exposes every type of the area") {
        const json_type kind = json_type::STRING;
        const value view = {};
        const document doc = {};
        const writer out = {};

        CHECK(to_string(kind) == "string");
        CHECK_FALSE(view.is_valid());
        CHECK(doc.root().is_null());
        CHECK(out.depth() == 0);
    }

    TEST_CASE("json - parse matches document::parse for success and failure") {
        const result<document> viaFree = parse(R"({"a":[1,2,3]})");
        const result<document> viaStatic = document::parse(R"({"a":[1,2,3]})");
        REQUIRE(viaFree.has_value());
        REQUIRE(viaStatic.has_value());
        CHECK(*viaFree == *viaStatic);

        const result<document> badFree = parse("{");
        const result<document> badStatic = document::parse("{");
        REQUIRE_FALSE(badFree.has_value());
        REQUIRE_FALSE(badStatic.has_value());
        CHECK(badFree.get_error() == badStatic.get_error());
        CHECK(badFree.get_error().generic_code() == error_code::PARSE_FAILURE);
    }

    TEST_CASE("json - to_string serialises any node of a document in member order") {
        const document doc = parse_or_fail(R"({"outer":{"y":1,"x":[true,null,"s"]},"n":-2.5})");
        const value root = doc.root();

        CHECK(to_string(root) == R"({"outer":{"y":1,"x":[true,null,"s"]},"n":-2.5})");
        CHECK(to_string(root) == doc.to_string());
        CHECK(to_string(root["outer"]) == R"({"y":1,"x":[true,null,"s"]})");
        CHECK(to_string(root["outer"]["x"]) == R"([true,null,"s"])");
        CHECK(to_string(root["outer"]["x"][2]) == R"("s")");
        CHECK(to_string(root["n"]) == "-2.5");
        CHECK(to_string(root["outer"]["y"]) == "1");
    }

    TEST_CASE("json - to_canonical_string sorts the members of any node at every depth") {
        const document doc = parse_or_fail(R"({"outer":{"y":1,"x":[{"b":2,"a":1},null,"s"]},"n":-2.5})");
        const value root = doc.root();

        CHECK(to_canonical_string(root) == R"({"n":-2.5,"outer":{"x":[{"a":1,"b":2},null,"s"],"y":1}})");
        CHECK(to_canonical_string(root["outer"]) == R"({"x":[{"a":1,"b":2},null,"s"],"y":1})");
        CHECK(to_canonical_string(root["outer"]["x"]) == R"([{"a":1,"b":2},null,"s"])");
        CHECK(to_canonical_string(root["outer"]["x"][1]) == "null");
        CHECK(to_canonical_string(root["outer"]["y"]) == "1");
        CHECK(to_canonical_string(root["n"]) == "-2.5");
    }

    TEST_CASE("json - to_canonical_string of a root matches document::to_canonical_string") {
        const std::array<std::string_view, 6> corpus = {
            "null",
            "[]",
            "{}",
            R"({"b":{"z":0.1,"a":[3,2,1]},"a":"x","é":true,"A":null})",
            R"({"a":1,"a":2})",
            "[1e21,-0.0,18446744073709551615,-9223372036854775808]",
        };

        for (const std::string_view text : corpus) {
            CAPTURE(text);
            const document doc = parse_or_fail(text);
            CHECK(to_canonical_string(doc.root()) == doc.to_canonical_string());
            CHECK(to_string(doc.root()) == doc.to_string());
        }
    }

    TEST_CASE("json - to_string and to_canonical_string of an invalid value are empty") {
        const document doc = parse_or_fail("{}");

        SUBCASE("a default value") {
            CHECK(to_string(value()).empty());
            CHECK(to_canonical_string(value()).empty());
        }
        SUBCASE("a missing member") {
            CHECK(to_string(doc.root()["missing"]).empty());
            CHECK(to_canonical_string(doc.root()["missing"]).empty());
        }
        SUBCASE("an index into an object") {
            CHECK(to_string(doc.root()[0]).empty());
            CHECK(to_canonical_string(doc.root()[0]).empty());
        }
    }

    TEST_CASE("json - to_string and to_canonical_string of a node holding invalid UTF-8 are empty") {
        document doc = document::object();
        doc.set("inner", object({{"bad", "\xFF\xFE"}}));
        const std::array<std::pair<std::string_view, std::string (*)(const value &)>, 2> writers = {{
          {"to_string", &json::to_string},
          {"to_canonical_string", &json::to_canonical_string},
        }};
        const std::array<std::pair<std::string_view, value>, 2> nodes = {{
          {"the object", doc.root()["inner"]},
          {"the string", doc.root()["inner"]["bad"]},
        }};
        testing::log_capture logs = {};

        // Each refusal logs exactly one warning, from the serialiser.
        for (const auto &writer : writers) {
            for (const auto &node : nodes) {
                const std::string_view writerName = writer.first;
                const std::string_view nodeName = node.first;
                CAPTURE(writerName);
                CAPTURE(nodeName);
                logs.clear();
                CHECK(writer.second(node.second).empty());
                CHECK(logs.count(log_level::WARN) == 1);
                CHECK(logs.contains(log_level::WARN, "serialisation failed"));
            }
        }
    }

    TEST_CASE("regression - code sweep F619 json::to_string takes no canonical flag") {
        // The flag meant "canonical" here and "pretty" on document::to_string; a leftover
        // to_string(x, true) must stop compiling instead of silently changing its output.
        CHECK_FALSE(callable_with_flag<value>);

        const document doc = parse_or_fail(R"({"b":1,"a":2})");
        CHECK(to_string(doc.root()) == R"({"b":1,"a":2})");
        CHECK(to_canonical_string(doc.root()) == R"({"a":2,"b":1})");
    }

    TEST_CASE("json - documents round trip through to_string and parse") {
        const std::array<std::string_view, 8> corpus = {
            "null",
            "true",
            R"("a string with \"quotes\" and \\ and \u0001 and é")",
            "[1,-2,3.5,1e21,-0.0,18446744073709551615,-9223372036854775808]",
            R"({"nested":{"deeper":{"deepest":[[[{}]]]}}})",
            R"({"a":1,"a":2})",
            R"([{"id":"0123abcd-4567-4000-8000-000000000001","tags":["x","y"],"ratio":0.1}])",
            "[]",
        };

        for (const std::string_view text : corpus) {
            CAPTURE(text);
            const document first = parse_or_fail(text);
            const std::string written = first.to_string();
            const document second = parse_or_fail(written);
            CHECK(first == second);
            CHECK(second.to_string() == written);
            CHECK(first.to_canonical_string() == second.to_canonical_string());

            const document third = parse_or_fail(first.to_string(true));
            CHECK(third == first);
        }
    }

    TEST_CASE("json - canonical text is identical however equal content was built") {
        const document parsedOne = parse_or_fail(R"({"b":[1,2.5,"x"],"a":{"d":null,"c":true}})");
        const document parsedTwo = parse_or_fail(R"({"a":{"c":true,"d":null},"b":[1,2.5,"x"]})");

        document built = document::object();
        built.set("b", array({1, 2.5, "x"}));
        built.set("a", object({{"d", nullptr}, {"c", true}}));

        document pointed = {};
        pointed.set_pointer("/a/d", nullptr);
        pointed.set_pointer("/a/c", true);
        pointed.set_pointer("/b", array({1, 2.5, "x"}));

        writer out = {};
        out.begin_object();
        out.key("a");
        out.begin_object();
        out.key("c");
        out.value(true);
        out.key("d");
        out.value(nullptr);
        out.end_object();
        out.key("b");
        out.begin_array();
        out.value(1);
        out.value(2.5);
        out.value("x");
        out.end_array();
        out.end_object();

        const std::string expected = R"({"a":{"c":true,"d":null},"b":[1,2.5,"x"]})";
        CHECK(parsedOne.to_canonical_string() == expected);
        CHECK(parsedTwo.to_canonical_string() == expected);
        CHECK(built.to_canonical_string() == expected);
        CHECK(pointed.to_canonical_string() == expected);
        CHECK(out.finish() == expected);
        CHECK(parsedOne == built);
        CHECK(pointed == parsedTwo);
    }

    TEST_CASE("json - parse errors report the position of several malformed inputs") {
        struct sample {
            std::string_view text = {};
            std::string_view position = {};
        };
        const std::array<sample, 6> samples = {{
            {"{", "line 1, column 2"},
            {R"({"a": 1,)" "\n" R"("b": })", "line 2, column 6"},
            {"[1, 2,]", "line 1, column 6"},
            {"\n\n   tru", "line 3, column 7"},
            {R"({"a": 1} trailing)", "line 1, column 10"},
            {"[1 2]", "line 1, column 4"},
        }};

        for (const sample &entry : samples) {
            CAPTURE(entry.text);
            const result<document> parsed = parse(entry.text);
            REQUIRE_FALSE(parsed.has_value());
            CAPTURE(parsed.get_error().message());
            CHECK(parsed.get_error().generic_code() == error_code::PARSE_FAILURE);
            CHECK(parsed.get_error().message().find("parse error at ") == 0);
            CHECK(parsed.get_error().message().find(entry.position) != std::string::npos);
        }
    }

    TEST_CASE("json - a large array round trips and keeps its order") {
        document doc = document::array();
        for (i32 index = 0; index < 10000; ++index) {
            doc.push_back(index);
        }

        const std::string text = doc.to_string();
        CHECK(text.front() == '[');
        CHECK(text.back() == ']');
        CHECK(text == doc.to_canonical_string());

        const document reparsed = parse_or_fail(text);
        REQUIRE(reparsed.root().size() == 10000);
        CHECK(reparsed.root()[0].as_i64() == 0);
        CHECK(reparsed.root()[9999].as_i64() == 9999);
        CHECK(reparsed == doc);

        i64 sum = 0;
        for (const value item : reparsed.root()) {
            sum += item.as_i64();
        }
        CHECK(sum == 49995000);
    }
}
