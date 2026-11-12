#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"

#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"

#include <array>
#include <atomic>
#include <filesystem>
#include <format>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
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

    std::string read_file(const std::filesystem::path &path) {
        std::ifstream stream(path, std::ios::binary);
        std::stringstream contents = {};
        contents << stream.rdbuf();
        return contents.str();
    }

    std::string nested_arrays(usize depth) {
        std::string text = {};
        text.append(depth, '[');
        text.append(depth, ']');
        return text;
    }
}

TEST_SUITE("core/document") {

    TEST_CASE("document - a default document has a null root") {
        const document doc = {};

        CHECK(doc.root().is_valid());
        CHECK(doc.root().is_null());
        CHECK(doc.root().type() == json_type::NULL_VALUE);
        CHECK(doc.root().owner() == &doc);
        CHECK(doc.to_string() == "null");
        CHECK(doc.to_string(true) == "null");
        CHECK(doc.to_canonical_string() == "null");
    }

    TEST_CASE("document - object() and array() create empty containers") {
        const document object = document::object();
        const document array = document::array();

        CHECK(object.root().is_object());
        CHECK(object.root().size() == 0);
        CHECK(object.to_string() == "{}");
        CHECK(array.root().is_array());
        CHECK(array.root().size() == 0);
        CHECK(array.to_string() == "[]");
        CHECK(object != array);
    }

    TEST_CASE("document - from_value copies a subtree into its own document") {
        const document source = parse_or_fail(R"({"inner":{"a":[1,2]},"n":5})");

        const document inner = document::from_value(source.root()["inner"]);
        CHECK(inner.to_string() == R"({"a":[1,2]})");
        CHECK(inner.root().owner() == &inner);

        const document scalar = document::from_value(source.root()["n"]);
        CHECK(scalar.to_string() == "5");

        const document fromInvalid = document::from_value(value());
        CHECK(fromInvalid.root().is_null());
        CHECK(fromInvalid.to_string() == "null");
    }

    TEST_CASE("document - parse accepts strict JSON of every root type") {
        CHECK(parse_or_fail("null").root().is_null());
        CHECK(parse_or_fail("true").root().as_bool());
        CHECK(parse_or_fail("  42 ").root().as_i64() == 42);
        CHECK(parse_or_fail("-1.5e3").root().as_f64() == -1500.0);
        CHECK(parse_or_fail(R"("text")").root().as_string() == "text");
        CHECK(parse_or_fail("[1]").root().is_array());
        CHECK(parse_or_fail("{}").root().is_object());
        CHECK(parse_or_fail("\t\r\n {\"a\" : 1 }\n").root()["a"].as_i64() == 1);
    }

    TEST_CASE("document - parse rejects empty input and non-standard extensions") {
        const std::array<std::string_view, 10> rejected = {
            "",
            "   \n ",
            "// comment\n{}",
            "{\"a\":1,}",
            "[1,2,]",
            "NaN",
            "[Infinity]",
            "{'a':1}",
            "\xEF\xBB\xBF{}",
            "{\"a\":1} {\"b\":2}",
        };

        for (const std::string_view text : rejected) {
            CAPTURE(text);
            const result<document> parsed = document::parse(text);
            REQUIRE_FALSE(parsed.has_value());
            CHECK(parsed.get_error().generic_code() == error_code::PARSE_FAILURE);
            CHECK(parsed.get_error().message().find("parse error at line") == 0);
        }
    }

    TEST_CASE("document - parse errors report the 1-based line and column") {
        const result<document> firstLine = document::parse("{\"a\": }");
        REQUIRE_FALSE(firstLine.has_value());
        CHECK(firstLine.get_error().message().find("line 1, column 7") != std::string::npos);

        const result<document> secondLine = document::parse("{\"a\": 1,\n  \"b\": }");
        REQUIRE_FALSE(secondLine.has_value());
        CHECK(secondLine.get_error().message().find("line 2, column 8") != std::string::npos);

        const result<document> truncated = document::parse("[1, 2");
        REQUIRE_FALSE(truncated.has_value());
        CHECK(truncated.get_error().message().find("line 1, column 6") != std::string::npos);

        const result<document> empty = document::parse("");
        REQUIRE_FALSE(empty.has_value());
        CHECK(empty.get_error().message() == "parse error at line 1, column 1: empty input");
    }

    TEST_CASE("document - parse rejects invalid UTF-8 and invalid escapes") {
        CHECK_FALSE(document::parse("[\"\xFF\"]").has_value());
        CHECK_FALSE(document::parse(R"(["\x41"])").has_value());
        CHECK_FALSE(document::parse(R"(["\ud800"])").has_value());
        CHECK(document::parse(R"(["😀"])").has_value());
    }

    TEST_CASE("document - parse_file reads a file and reports io and parse failures") {
        const testing::temp_directory directory("core_json");
        const std::filesystem::path good = directory.write_file("nested/dir/config.json", R"({"width": 1920})");
        const std::filesystem::path bad = directory.write_file("broken.json", "{\"width\": }");

        const result<document> parsed = document::parse_file(good);
        REQUIRE(parsed.has_value());
        CHECK(parsed->root()["width"].as_i64() == 1920);

        const result<document> missing = document::parse_file(directory.path() / "missing.json");
        REQUIRE_FALSE(missing.has_value());
        CHECK(missing.get_error().generic_code() == error_code::IO_FAILURE);
        CHECK(missing.get_error().message().find("missing.json") != std::string::npos);

        const result<document> broken = document::parse_file(bad);
        REQUIRE_FALSE(broken.has_value());
        CHECK(broken.get_error().generic_code() == error_code::PARSE_FAILURE);
        CHECK(broken.get_error().message().find("broken.json") != std::string::npos);
        CHECK(broken.get_error().message().find("line 1, column 11") != std::string::npos);

        const std::filesystem::path empty = directory.write_file("empty.json", "");
        const result<document> emptyParsed = document::parse_file(empty);
        REQUIRE_FALSE(emptyParsed.has_value());
        CHECK(emptyParsed.get_error().generic_code() == error_code::PARSE_FAILURE);

        // POSIX would happily open the directory as an empty stream; it must be an I/O failure.
        const result<document> folder = document::parse_file(directory.path() / "nested");
        REQUIRE_FALSE(folder.has_value());
        CHECK(folder.get_error().generic_code() == error_code::IO_FAILURE);
        CHECK(folder.get_error().message().find("not a regular file") != std::string::npos);
    }

    TEST_CASE("document - parse_file skips a leading UTF-8 byte order mark") {
        const testing::temp_directory directory("core_json_bom");
        const std::string text = "\xEF\xBB\xBF{\"a\": 1}";
        const std::filesystem::path marked = directory.write_file("marked.json", text);

        const result<document> parsed = document::parse_file(marked);
        REQUIRE(parsed.has_value());
        CHECK(parsed->root()["a"].as_i64() == 1);

        // parse() itself stays strict.
        const result<document> direct = document::parse(text);
        REQUIRE_FALSE(direct.has_value());
        CHECK(direct.get_error().generic_code() == error_code::PARSE_FAILURE);

        // Only one leading mark is dropped: a second one is still content.
        const std::filesystem::path twice = directory.write_file("twice.json", "\xEF\xBB\xBF" + text);
        const result<document> twiceParsed = document::parse_file(twice);
        REQUIRE_FALSE(twiceParsed.has_value());
        CHECK(twiceParsed.get_error().generic_code() == error_code::PARSE_FAILURE);
    }

    TEST_CASE("document - copies are deep and moves transfer ownership") {
        document original = parse_or_fail(R"({"a":[1,2],"b":"text"})");

        document copy = original;
        copy.set("b", "changed");
        copy.set_pointer("/a/0", 99);
        CHECK(original.to_string() == R"({"a":[1,2],"b":"text"})");
        CHECK(copy.to_string() == R"({"a":[99,2],"b":"changed"})");
        CHECK(copy.root().owner() == &copy);

        document assigned = document::array();
        assigned = original;
        CHECK(assigned == original);
        assigned.erase("a");
        CHECK(original.root().contains("a"));

        const value rootBefore = original.root();
        document moved = std::move(original);
        CHECK(moved.to_string() == R"({"a":[1,2],"b":"text"})");
        CHECK(moved.root() == rootBefore);

        document moveAssigned = document::object();
        moveAssigned.set("x", 1);
        moveAssigned = std::move(moved);
        CHECK(moveAssigned.to_string() == R"({"a":[1,2],"b":"text"})");

        document &self = moveAssigned;
        moveAssigned = self;
        CHECK(moveAssigned.to_string() == R"({"a":[1,2],"b":"text"})");
    }

    TEST_CASE("document - a moved-from document is empty until it is assigned to") {
        document source = parse_or_fail(R"({"a":1})");
        document taken = std::move(source);
        REQUIRE(taken.to_string() == R"({"a":1})");

        CHECK_FALSE(source.root().is_valid());
        CHECK(source.root().is_null());
        CHECK(source.to_string().empty());
        CHECK(source.to_canonical_string().empty());
        CHECK_FALSE(source.at_pointer("/a").is_valid());
        CHECK_FALSE(source.at_pointer("").is_valid());
        CHECK(source != document());
        CHECK(source != taken);
        CHECK_FALSE(source.erase("a"));
        CHECK_FALSE(source.erase(usize(0)));
        CHECK_FALSE(source.set_pointer("/x", 1));
        CHECK_FALSE(source.set_pointer("", 1));
        CHECK_FALSE(source.merge_patch(taken));

        testing::expect_assert asserts = {};
        source.set("x", 1);
        CHECK(asserts.count() == 1);
        source.push_back(1);
        CHECK(asserts.count() == 2);
        source.clear();
        CHECK_FALSE(source.root().is_valid());

        const document copied = source;
        CHECK_FALSE(copied.root().is_valid());
        CHECK(copied.to_string().empty());

        const testing::temp_directory directory("core_json_moved");
        const result<void> written = source.write_file(directory.path() / "moved.json");
        REQUIRE_FALSE(written.has_value());
        CHECK(written.get_error().generic_code() == error_code::INVALID_STATE);

        source = taken;
        CHECK(source.root().is_valid());
        CHECK(source.to_string() == R"({"a":1})");
        CHECK(source == taken);

        document movedAgain = std::move(source);
        source = document::array();
        source.push_back(2);
        CHECK(source.to_string() == "[2]");
        CHECK(movedAgain.to_string() == R"({"a":1})");
    }

    TEST_CASE("document - set adds members in order and replaces in place") {
        document doc = document::object();
        doc.set("a", 1);
        doc.set("b", 2);
        doc.set("c", 3);
        CHECK(doc.to_string() == R"({"a":1,"b":2,"c":3})");

        doc.set("b", "replaced");
        CHECK(doc.to_string() == R"({"a":1,"b":"replaced","c":3})");

        doc.set("b", array({1, 2}));
        CHECK(doc.to_string() == R"({"a":1,"b":[1,2],"c":3})");

        doc.set("", "empty key");
        CHECK(doc.root()[""].as_string() == "empty key");
        CHECK(doc.root().size() == 4);
    }

    TEST_CASE("document - a default-constructed string_view key is the empty key") {
        document doc = document::object();
        doc.set("", 1);
        doc.set(std::string_view{}, 2);
        CHECK(doc.root().size() == 1);
        CHECK(doc.to_string() == R"({"":2})");

        CHECK(doc.root()[std::string_view{}].as_i64() == 2);
        CHECK(doc.root()[""].as_i64() == 2);
        CHECK(doc.root().contains(std::string_view{}));
        CHECK(doc.root().get(std::string_view{}).has_value());
        CHECK(doc.root()[std::string_view{}] == doc.root()[""]);

        doc.set_null(std::string_view{});
        CHECK(doc.to_string() == R"({"":null})");
        CHECK(doc.erase(std::string_view{}));
        CHECK(doc.root().size() == 0);
        CHECK_FALSE(doc.erase(std::string_view{}));

        document fresh = {};
        fresh.set(std::string_view{}, std::string_view{});
        CHECK(fresh.to_string() == R"({"":""})");
        CHECK(fresh.root()[std::string_view{}].as_string("fallback") == "");

        const document built = object({{std::string_view{}, 1}, {"", 2}});
        CHECK(built.to_string() == R"({"":2})");
        CHECK(built.root()[""].as_i64() == 2);

        CHECK(fresh.set_pointer("/", 7));
        CHECK(fresh.to_string() == R"({"":7})");
    }

    TEST_CASE("document - set accepts every scalar type, documents and values") {
        const document other = parse_or_fail(R"({"deep":{"list":[true]}})");

        document doc = document::object();
        doc.set("bool", true);
        doc.set("i32", i32(-7));
        doc.set("u32", u32(4000000000u));
        doc.set("i64", std::numeric_limits<i64>::min());
        doc.set("u64", std::numeric_limits<u64>::max());
        doc.set("f64", 0.25);
        doc.set("f32", 1.5f);
        doc.set("view", std::string_view("view"));
        doc.set("literal", "literal");
        doc.set("owned", std::string("owned"));
        doc.set("nullptr", nullptr);
        doc.set("nullCString", static_cast<const char *>(nullptr));
        doc.set("document", other);
        doc.set("value", other.root()["deep"]["list"]);

        const value root = doc.root();
        CHECK(root["bool"].as_bool() == true);
        CHECK(root["i32"].as_i64() == -7);
        CHECK(root["i32"].is_integer());
        CHECK(root["u32"].as_u64() == 4000000000u);
        CHECK(root["i64"].as_i64() == std::numeric_limits<i64>::min());
        CHECK(root["u64"].as_u64() == std::numeric_limits<u64>::max());
        CHECK(root["f64"].as_f64() == 0.25);
        CHECK_FALSE(root["f64"].is_integer());
        CHECK(root["f32"].as_f64() == 1.5);
        CHECK(root["view"].as_string() == "view");
        CHECK(root["literal"].as_string() == "literal");
        CHECK(root["owned"].as_string() == "owned");
        CHECK(root["nullptr"].is_null());
        CHECK(root["nullptr"].is_valid());
        CHECK(root["nullCString"].is_null());
        CHECK(root["document"] == other.root());
        CHECK(root["value"] == other.root()["deep"]["list"]);
        CHECK(root["value"].to<std::vector<bool>>().value() == std::vector<bool>{true});
        CHECK(root["value"].owner() == &doc);
    }

    TEST_CASE("document - set on a null root promotes it to an object and asserts on other roots") {
        document fresh = {};
        fresh.set("a", 1);
        CHECK(fresh.root().is_object());
        CHECK(fresh.to_string() == R"({"a":1})");

        testing::expect_assert asserts = {};
        document array = document::array();
        array.push_back(1);
        array.set("a", 1);
        CHECK(asserts.count() == 1);
        CHECK(array.to_string() == "[1]");

        document scalar = parse_or_fail("5");
        scalar.set_null("a");
        CHECK(asserts.count() == 2);
        CHECK(scalar.to_string() == "5");
    }

    TEST_CASE("document - set_null keeps the member and erase removes it") {
        document doc = parse_or_fail(R"({"a":1,"b":2,"a":3})");

        doc.set_null("b");
        CHECK(doc.root()["b"].is_valid());
        CHECK(doc.root()["b"].is_null());
        CHECK(doc.root().size() == 3);

        CHECK(doc.erase("a"));
        CHECK(doc.root().size() == 1);
        CHECK_FALSE(doc.root().contains("a"));
        CHECK_FALSE(doc.erase("a"));
        CHECK_FALSE(doc.erase("missing"));
        CHECK(doc.to_string() == R"({"b":null})");

        document array = document::array();
        CHECK_FALSE(array.erase("a"));
        document fresh = {};
        CHECK_FALSE(fresh.erase("a"));
        CHECK(fresh.root().is_null());
    }

    TEST_CASE("document - push_back appends every scalar type, documents and values") {
        const document other = parse_or_fail(R"({"k":[1]})");

        document doc = document::array();
        doc.push_back(true);
        doc.push_back(i32(-1));
        doc.push_back(u32(2));
        doc.push_back(i64(-3));
        doc.push_back(u64(4));
        doc.push_back(5.5);
        doc.push_back(0.5f);
        doc.push_back(std::string_view("view"));
        doc.push_back("literal");
        doc.push_back(nullptr);
        doc.push_back(static_cast<const char *>(nullptr));
        doc.push_back(other);
        doc.push_back(other.root()["k"]);

        CHECK(doc.to_string() == R"([true,-1,2,-3,4,5.5,0.5,"view","literal",null,null,{"k":[1]},[1]])");
        CHECK(doc.root().size() == 13);
    }

    TEST_CASE("document - push_back on a null root promotes it to an array and asserts on other roots") {
        document fresh = {};
        fresh.push_back(1);
        CHECK(fresh.root().is_array());
        CHECK(fresh.to_string() == "[1]");

        testing::expect_assert asserts = {};
        document object = document::object();
        object.push_back(1);
        CHECK(asserts.count() == 1);
        CHECK(object.to_string() == "{}");
    }

    TEST_CASE("document - erase by index and clear") {
        document doc = parse_or_fail("[1,2,3]");

        CHECK(doc.erase(usize(1)));
        CHECK(doc.to_string() == "[1,3]");
        CHECK_FALSE(doc.erase(usize(2)));
        CHECK(doc.erase(usize(0)));
        CHECK(doc.erase(usize(0)));
        CHECK_FALSE(doc.erase(usize(0)));
        CHECK(doc.to_string() == "[]");

        document object = parse_or_fail(R"({"a":1})");
        CHECK_FALSE(object.erase(usize(0)));
        object.clear();
        CHECK(object.to_string() == "{}");
        CHECK(object.root().is_object());

        document array = parse_or_fail("[1,2]");
        array.clear();
        CHECK(array.to_string() == "[]");
        CHECK(array.root().is_array());

        document scalar = parse_or_fail("\"text\"");
        scalar.clear();
        CHECK(scalar.root().is_null());
        CHECK(scalar.to_string() == "null");
    }

    TEST_CASE("document - non-finite reals assert and are stored as null") {
        testing::expect_assert asserts = {};
        document doc = document::object();

        doc.set("nan", std::numeric_limits<f64>::quiet_NaN());
        CHECK(asserts.count() == 1);
        doc.set("inf", std::numeric_limits<f64>::infinity());
        CHECK(asserts.count() == 2);
        CHECK(doc.root()["nan"].is_null());
        CHECK(doc.root()["inf"].is_null());
        CHECK(doc.to_string() == R"({"nan":null,"inf":null})");

        document array = document::array();
        array.push_back(-std::numeric_limits<f64>::infinity());
        CHECK(asserts.count() == 3);
        CHECK(array.to_string() == "[null]");

        document pointed = {};
        CHECK(pointed.set_pointer("/x", std::numeric_limits<f64>::quiet_NaN()));
        CHECK(asserts.count() == 4);
        CHECK(pointed.to_string() == R"({"x":null})");
    }

    TEST_CASE("document - at_pointer resolves RFC 6901 pointers") {
        const document doc = parse_or_fail(R"({"a":{"b":[10,{"c":"found"}]},"x/y":1,"m~n":2,"":3,"arr":[]})");

        CHECK(doc.at_pointer("") == doc.root());
        CHECK(doc.at_pointer("/a/b/0").as_i64() == 10);
        CHECK(doc.at_pointer("/a/b/1/c").as_string() == "found");
        CHECK(doc.at_pointer("/x~1y").as_i64() == 1);
        CHECK(doc.at_pointer("/m~0n").as_i64() == 2);
        CHECK(doc.at_pointer("/").as_i64() == 3);
        CHECK(doc.at_pointer("/a/b").is_array());

        CHECK_FALSE(doc.at_pointer("/missing").is_valid());
        CHECK_FALSE(doc.at_pointer("/a/b/2").is_valid());
        CHECK_FALSE(doc.at_pointer("/a/b/-").is_valid());
        CHECK_FALSE(doc.at_pointer("/a/b/0/c").is_valid());
        CHECK_FALSE(doc.at_pointer("a/b").is_valid());
        CHECK_FALSE(doc.at_pointer("/a/b/x").is_valid());
        CHECK_FALSE(doc.at_pointer("/arr/0").is_valid());
        CHECK_FALSE(doc.at_pointer("/x/y").is_valid());
    }

    TEST_CASE("document - set_pointer creates intermediate objects and replaces existing values") {
        document doc = {};

        CHECK(doc.set_pointer("/a/b/c", 1));
        CHECK(doc.to_string() == R"({"a":{"b":{"c":1}}})");
        CHECK(doc.set_pointer("/a/b/c", "replaced"));
        CHECK(doc.set_pointer("/a/d", true));
        CHECK(doc.to_string() == R"({"a":{"b":{"c":"replaced"},"d":true}})");

        CHECK(doc.set_pointer("/list", array({1, 2})));
        CHECK(doc.set_pointer("/list/0", 10));
        CHECK(doc.set_pointer("/list/-", 3));
        CHECK(doc.at_pointer("/list").size() == 3);
        CHECK(doc.at_pointer("/list").to<std::vector<i32>>().value() == std::vector<i32>{10, 2, 3});
    }

    TEST_CASE("document - set_pointer accepts every value type and reports failures") {
        const document other = parse_or_fail(R"({"k":"v"})");
        document doc = document::object();

        CHECK(doc.set_pointer("/b", true));
        CHECK(doc.set_pointer("/i32", i32(-1)));
        CHECK(doc.set_pointer("/u32", u32(1)));
        CHECK(doc.set_pointer("/i64", i64(-2)));
        CHECK(doc.set_pointer("/u64", u64(2)));
        CHECK(doc.set_pointer("/f64", 0.5));
        CHECK(doc.set_pointer("/f32", 0.25f));
        CHECK(doc.set_pointer("/view", std::string_view("view")));
        CHECK(doc.set_pointer("/literal", "literal"));
        CHECK(doc.set_pointer("/null", nullptr));
        CHECK(doc.set_pointer("/nullCString", static_cast<const char *>(nullptr)));
        CHECK(doc.set_pointer("/doc", other));
        CHECK(doc.set_pointer("/val", other.root()["k"]));
        CHECK(doc.to_canonical_string() ==
          R"({"b":true,"doc":{"k":"v"},"f32":0.25,"f64":0.5,"i32":-1,"i64":-2,"literal":"literal","null":null,)"
          R"("nullCString":null,"u32":1,"u64":2,"val":"v","view":"view"})");

        CHECK_FALSE(doc.set_pointer("no-slash", 1));
        CHECK_FALSE(doc.set_pointer("/i32/child", 1));
        CHECK_FALSE(doc.set_pointer("/doc/k/deeper", 1));
        CHECK(doc.set_pointer("/arr", array({1})));
        CHECK_FALSE(doc.set_pointer("/arr/5", 1));
        CHECK_FALSE(doc.set_pointer("/arr/x", 1));
        CHECK_FALSE(doc.set_pointer("/arr/-1", 1));
        CHECK_FALSE(doc.set_pointer("/arr/01", 1));
        CHECK(doc.at_pointer("/arr").size() == 1);

        // RFC 6902 "add": an index equal to the size appends, anything past it is out of range.
        CHECK(doc.set_pointer("/arr/1", 2));
        CHECK(doc.at_pointer("/arr").to<std::vector<i32>>().value() == std::vector<i32>{1, 2});
        CHECK_FALSE(doc.set_pointer("/arr/3", 4));
        CHECK(doc.set_pointer("/arr/2/k", "nested"));
        CHECK(doc.at_pointer("/arr").size() == 3);
        CHECK(doc.at_pointer("/arr/2/k").as_string() == "nested");

        testing::expect_assert asserts = {};
        CHECK_FALSE(doc.set_pointer("/invalid", value()));
        CHECK(asserts.count() == 1);

        CHECK(doc.set_pointer("", 42));
        CHECK(doc.to_string() == "42");
        CHECK(doc.set_pointer("", other));
        CHECK(doc == other);
    }

    TEST_CASE("document - merge_patch follows the RFC 7386 examples") {
        struct sample {
            std::string_view original = {};
            std::string_view patch = {};
            std::string_view expected = {};
        };
        const std::array<sample, 15> samples = {{
            {R"({"a":"b"})", R"({"a":"c"})", R"({"a":"c"})"},
            {R"({"a":"b"})", R"({"b":"c"})", R"({"a":"b","b":"c"})"},
            {R"({"a":"b"})", R"({"a":null})", R"({})"},
            {R"({"a":"b","b":"c"})", R"({"a":null})", R"({"b":"c"})"},
            {R"({"a":["b"]})", R"({"a":"c"})", R"({"a":"c"})"},
            {R"({"a":"c"})", R"({"a":["b"]})", R"({"a":["b"]})"},
            {R"({"a":{"b":"c"}})", R"({"a":{"b":"d","c":null}})", R"({"a":{"b":"d"}})"},
            {R"({"a":[{"b":"c"}]})", R"({"a":[1]})", R"({"a":[1]})"},
            {R"(["a","b"])", R"(["c","d"])", R"(["c","d"])"},
            {R"({"a":"b"})", R"(["c"])", R"(["c"])"},
            {R"({"a":"foo"})", "null", "null"},
            {R"({"a":"foo"})", R"("bar")", R"("bar")"},
            {R"({"e":null})", R"({"a":1})", R"({"a":1,"e":null})"},
            {"[1,2]", R"({"a":"b","c":null})", R"({"a":"b"})"},
            {"{}", R"({"a":{"bb":{"ccc":null}}})", R"({"a":{"bb":{}}})"},
        }};

        for (const sample &entry : samples) {
            CAPTURE(entry.original);
            CAPTURE(entry.patch);
            document target = parse_or_fail(entry.original);
            const document patch = parse_or_fail(entry.patch);
            REQUIRE(target.merge_patch(patch));
            CHECK(target.to_canonical_string() == entry.expected);
            CHECK(target == parse_or_fail(entry.expected));
        }
    }

    TEST_CASE("document - merge_patch accepts a value from another document and rejects an invalid one") {
        document target = parse_or_fail(R"({"settings":{"volume":5,"muted":false}})");
        const document source = parse_or_fail(R"({"patch":{"volume":7,"muted":null,"new":"x"}})");

        document settings = document::from_value(target.root()["settings"]);
        REQUIRE(settings.merge_patch(source.root()["patch"]));
        CHECK(settings.to_canonical_string() == R"({"new":"x","volume":7})");

        testing::expect_assert asserts = {};
        CHECK_FALSE(target.merge_patch(value()));
        CHECK(asserts.count() == 1);
        CHECK(target.to_string() == R"({"settings":{"volume":5,"muted":false}})");
    }

    TEST_CASE("document - to_string keeps member order and pretty prints with four spaces") {
        document doc = document::object();
        doc.set("b", array({1, 2}));
        doc.set("a", object({{"z", true}, {"y", nullptr}}));
        doc.set("empty", document::object());

        CHECK(doc.to_string() == R"({"b":[1,2],"a":{"z":true,"y":null},"empty":{}})");
        CHECK(doc.to_string(false) == doc.to_string());

        const std::string pretty = doc.to_string(true);
        const std::string expected =
          "{\n"
          "    \"b\": [\n"
          "        1,\n"
          "        2\n"
          "    ],\n"
          "    \"a\": {\n"
          "        \"z\": true,\n"
          "        \"y\": null\n"
          "    },\n"
          "    \"empty\": {}\n"
          "}";
        CHECK(pretty == expected);
    }

    TEST_CASE("document - to_canonical_string sorts keys recursively and minifies") {
        const document doc = parse_or_fail(R"({ "b" : 1 , "a" : { "d" : [ { "z" : 0 , "y" : 1 } ] , "c" : 3 } })");

        CHECK(doc.to_canonical_string() == R"({"a":{"c":3,"d":[{"y":1,"z":0}]},"b":1})");
        CHECK(doc.to_string() == R"({"b":1,"a":{"d":[{"z":0,"y":1}],"c":3}})");

        const document duplicates = parse_or_fail(R"({"b":1,"a":2,"b":3,"a":4})");
        CHECK(duplicates.to_canonical_string() == R"({"a":2,"a":4,"b":1,"b":3})");

        const document byteOrder = parse_or_fail(R"({"b":0,"B":0,"a":0,"A":0,"_":0,"10":0,"9":0,"é":0,"z":0})");
        CHECK(byteOrder.to_canonical_string() == R"({"10":0,"9":0,"A":0,"B":0,"_":0,"a":0,"b":0,"z":0,"é":0})");
    }

    TEST_CASE("document - canonical text is stable for a table of numbers and round trips") {
        struct sample {
            std::string_view input = {};
            std::string_view canonical = {};
        };
        const std::array<sample, 22> samples = {{
            {"0", "0"},
            {"-0", "0"},
            {"0.0", "0.0"},
            {"-0.0", "-0.0"},
            {"1.0", "1.0"},
            {"0.1", "0.1"},
            {"0.30000000000000004", "0.30000000000000004"},
            {"1e21", "1e21"},
            {"1E+21", "1e21"},
            {"1e22", "1e22"},
            {"100000000000000000000", "100000000000000000000.0"},
            {"1e-7", "1e-7"},
            {"0.000001", "0.000001"},
            {"123456789.125", "123456789.125"},
            {"18446744073709551615", "18446744073709551615"},
            {"18446744073709551616", "18446744073709552000.0"},
            {"-9223372036854775808", "-9223372036854775808"},
            {"-9223372036854775809", "-9223372036854776000.0"},
            {"1.7976931348623157e308", "1.7976931348623157e308"},
            {"5e-324", "5e-324"},
            {"2.5E3", "2500.0"},
            {"-1.5e-3", "-0.0015"},
        }};

        for (const sample &entry : samples) {
            CAPTURE(entry.input);
            const document doc = parse_or_fail(entry.input);
            const std::string canonical = doc.to_canonical_string();
            CHECK(canonical == entry.canonical);
            CHECK(doc.to_string() == entry.canonical);

            const document reparsed = parse_or_fail(canonical);
            CHECK(reparsed.to_canonical_string() == canonical);
            CHECK(reparsed == doc);
        }

        document built = document::array();
        built.push_back(0.1);
        built.push_back(1e21);
        built.push_back(-0.0);
        built.push_back(std::numeric_limits<u64>::max());
        built.push_back(std::numeric_limits<i64>::min());
        built.push_back(100.0);
        CHECK(built.to_canonical_string() == "[0.1,1e21,-0.0,18446744073709551615,-9223372036854775808,100.0]");
    }

    TEST_CASE("document - strings with escapes and UTF-8 round trip and serialise like yyjson") {
        const document doc = parse_or_fail(R"({"s":"é\n\t\"\\\u0001\u001f/😀 \u007f"})");

        const std::string_view decoded = doc.root()["s"].as_string();
        CHECK(decoded == "\xC3\xA9\n\t\"\\\x01\x1F/\xF0\x9F\x98\x80 \x7F");
        CHECK(doc.to_string() == "{\"s\":\"\xC3\xA9\\n\\t\\\"\\\\\\u0001\\u001F/\xF0\x9F\x98\x80 \x7F\"}");
        CHECK(doc.to_canonical_string() == doc.to_string());

        const document reparsed = parse_or_fail(doc.to_string());
        CHECK(reparsed == doc);

        document built = document::object();
        built.set("s", std::string_view("a\0b", 3));
        CHECK(built.to_string() == R"({"s":"a\u0000b"})");
        CHECK(parse_or_fail(built.to_string()).root()["s"].as_string().size() == 3);
    }

    TEST_CASE("document - a string holding invalid UTF-8 cannot be serialised") {
        testing::log_capture logs = {};
        document doc = document::object();
        doc.set("bad", "\xFF\xFE");

        CHECK(doc.to_string().empty());
        CHECK(doc.to_canonical_string().empty());
        CHECK(logs.count(log_level::WARN) >= 1);

        const testing::temp_directory directory("core_json_bad_utf8");
        const result<void> written = doc.write_file(directory.path() / "bad.json");
        REQUIRE_FALSE(written.has_value());
        CHECK(written.get_error().generic_code() == error_code::INVALID_STATE);
        CHECK_FALSE(std::filesystem::exists(directory.path() / "bad.json"));
    }

    TEST_CASE("document - write_file writes minified or pretty text and creates parent directories") {
        const testing::temp_directory directory("core_json_write");
        const document doc = parse_or_fail(R"({"a":[1,2],"b":"x"})");

        const std::filesystem::path minified = directory.path() / "out" / "deeper" / "min.json";
        REQUIRE(doc.write_file(minified).has_value());
        CHECK(read_file(minified) == R"({"a":[1,2],"b":"x"})");

        const std::filesystem::path pretty = directory.path() / "pretty.json";
        REQUIRE(doc.write_file(pretty, true).has_value());
        CHECK(read_file(pretty) == doc.to_string(true));

        const result<document> reloaded = document::parse_file(pretty);
        REQUIRE(reloaded.has_value());
        CHECK(*reloaded == doc);

        REQUIRE(document::object().write_file(minified).has_value());
        CHECK(read_file(minified) == "{}");
    }

    TEST_CASE("document - write_file fails on an unwritable path") {
        const testing::temp_directory directory("core_json_write_bad");
        const std::filesystem::path blocker = directory.write_file("not_a_directory", "x");
        const document doc = document::object();

        const result<void> written = doc.write_file(blocker / "out.json");
        REQUIRE_FALSE(written.has_value());
        CHECK(written.get_error().generic_code() == error_code::IO_FAILURE);
        CHECK(written.get_error().message().find("out.json") != std::string::npos);
    }

    TEST_CASE("document - object() and array() builders accept every argument kind") {
        const document other = parse_or_fail(R"({"k":[7]})");
        const std::string owned = "owned";

        const document built = object({
          {"bool", true},
          {"i32", -1},
          {"u32", 2u},
          {"i64", std::numeric_limits<i64>::min()},
          {"u64", std::numeric_limits<u64>::max()},
          {"f64", 2.5},
          {"f32", 0.5f},
          {"literal", "text"},
          {"view", std::string_view("view")},
          {"owned", owned},
          {"temporary", std::string("temporary")},
          {"null", nullptr},
          {"array", array({1, "two", 3.0, false, nullptr, array({}), object({})})},
          {"document", other},
          {"value", other.root()["k"]},
        });

        CHECK(built.to_string() ==
          R"({"bool":true,"i32":-1,"u32":2,"i64":-9223372036854775808,"u64":18446744073709551615,"f64":2.5,)"
          R"("f32":0.5,"literal":"text","view":"view","owned":"owned","temporary":"temporary","null":null,)"
          R"("array":[1,"two",3.0,false,null,[],{}],"document":{"k":[7]},"value":[7]})");

        CHECK(object({}).to_string() == "{}");
        CHECK(array({}).to_string() == "[]");
        CHECK(array({nullptr}).to_string() == "[null]");
        CHECK(object({{"a", 1}, {"a", 2}}).to_string() == R"({"a":2})");
    }

    TEST_CASE("document - a null C string is JSON null in the builders, like in the mutation API") {
        const char *nullText = nullptr;

        CHECK(array({nullText}).to_string() == "[null]");
        CHECK(array({nullText, "text"}).to_string() == R"([null,"text"])");
        CHECK(object({{"k", nullText}}).to_string() == R"({"k":null})");
        CHECK(object({{"k", nullText}}).root()["k"].is_null());
        CHECK(object({{"k", nullText}}).root()["k"].is_valid());

        document viaSet = document::object();
        viaSet.set("k", nullText);
        CHECK(viaSet == object({{"k", nullText}}));

        document moved = parse_or_fail("[1]");
        document fromMoved = std::move(moved);
        CHECK(array({std::move(moved)}).to_string() == "[null]");
        CHECK(array({value()}).to_string() == "[null]");
        CHECK(fromMoved.to_string() == "[1]");
    }

    TEST_CASE("document - integral arguments of every width are accepted") {
        document doc = document::object();
        doc.set("long", 5L);
        doc.set("ulong", 6UL);
        doc.set("size", sizeof(u64));
        doc.set("u8", u8(200));
        doc.set("i8", i8(-5));
        doc.set("short", static_cast<short>(-3));
        doc.set("ushort", static_cast<unsigned short>(7));
        doc.set("longlong", -9LL);
        doc.set("ulonglong", 9ULL);
        CHECK(doc.to_string() ==
          R"({"long":5,"ulong":6,"size":8,"u8":200,"i8":-5,"short":-3,"ushort":7,"longlong":-9,"ulonglong":9})");
        CHECK(doc.root()["i8"].is_integer());
        CHECK(doc.root()["u8"].to<u8>().value() == 200);

        document list = document::array();
        list.push_back(5L);
        list.push_back(sizeof(u32));
        list.push_back(i8(-1));
        list.push_back(static_cast<unsigned short>(65535));
        CHECK(list.to_string() == "[5,4,-1,65535]");

        document pointed = {};
        CHECK(pointed.set_pointer("/a", 5L));
        CHECK(pointed.set_pointer("/b", sizeof(u16)));
        CHECK(pointed.set_pointer("/c", i8(-2)));
        CHECK(pointed.to_string() == R"({"a":5,"b":2,"c":-2})");

        const document built = array({5L, sizeof(u8), i8(-7), static_cast<unsigned short>(3), 1, 2u, -3LL, 4ULL});
        CHECK(built.to_string() == "[5,1,-7,3,1,2,-3,4]");
        CHECK(object({{"n", static_cast<short>(-1)}}).to_string() == R"({"n":-1})");
    }

    TEST_CASE("document - documents compare structurally") {
        const document left = parse_or_fail(R"({"a":1,"b":[true,{"c":null}]})");
        const document right = parse_or_fail(R"({"b":[true,{"c":null}],"a":1})");
        const document different = parse_or_fail(R"({"a":1,"b":[true,{"c":0}]})");

        CHECK(left == right);
        CHECK(left != different);
        CHECK(document() == document());
        CHECK(document::object() != document::array());
        CHECK(document::object() == parse_or_fail("{}"));
    }

    TEST_CASE("document - deep nesting of 1000 levels round trips") {
        const std::string arrays = nested_arrays(1000);
        const document doc = parse_or_fail(arrays);
        CHECK(doc.to_string() == arrays);
        CHECK(doc.to_canonical_string() == arrays);

        const document copy = doc;
        CHECK(copy == doc);
        CHECK(copy.to_string() == arrays);

        value cursor = doc.root();
        for (usize depth = 0; depth < 999; ++depth) {
            cursor = cursor[0];
        }
        REQUIRE(cursor.is_valid());
        CHECK(cursor.is_array());
        CHECK(cursor.size() == 0);

        std::string objects = {};
        std::string closing = {};
        for (usize depth = 0; depth < 1000; ++depth) {
            objects.append("{\"a\":");
            closing.push_back('}');
        }
        objects.append("1");
        objects.append(closing);
        const document nestedObjects = parse_or_fail(objects);
        CHECK(nestedObjects.to_string() == objects);
        CHECK(nestedObjects.to_canonical_string() == objects);
        CHECK(document(nestedObjects) == nestedObjects);

        std::string pointer = {};
        for (usize depth = 0; depth < 1000; ++depth) {
            pointer.append("/a");
        }
        CHECK(nestedObjects.at_pointer(pointer).as_i64() == 1);

        document patched = nestedObjects;
        REQUIRE(patched.merge_patch(nestedObjects));
        CHECK(patched == nestedObjects);
    }

    TEST_CASE("document - parse accepts MAX_NESTING_DEPTH levels and rejects one more") {
        static_assert(MAX_NESTING_DEPTH >= 1000, "the design requires 1000 levels");

        const std::string arrays = nested_arrays(MAX_NESTING_DEPTH);
        const document deepest = parse_or_fail(arrays);
        CHECK(deepest.to_string() == arrays);
        CHECK(deepest.to_canonical_string() == arrays);
        const document copy = deepest;
        CHECK(copy == deepest);
        CHECK(copy.to_string() == arrays);
        document patched = deepest;
        REQUIRE(patched.merge_patch(deepest));
        CHECK(patched == deepest);

        std::string objects = {};
        std::string closing = {};
        for (usize depth = 0; depth < MAX_NESTING_DEPTH; ++depth) {
            objects.append("{\"a\":");
            closing.push_back('}');
        }
        objects.append("1");
        objects.append(closing);
        const document deepestObjects = parse_or_fail(objects);
        CHECK(deepestObjects.to_canonical_string() == objects);
        CHECK(document(deepestObjects) == deepestObjects);

        const result<document> tooDeep = document::parse(nested_arrays(MAX_NESTING_DEPTH + 1));
        REQUIRE_FALSE(tooDeep.has_value());
        CHECK(tooDeep.get_error().generic_code() == error_code::PARSE_FAILURE);
        CHECK(tooDeep.get_error().message().find("nesting deeper than") != std::string::npos);
        CHECK(tooDeep.get_error().message().find(std::format("{}", MAX_NESTING_DEPTH)) != std::string::npos);

        // The limit counts containers on any path, scalars and siblings do not matter.
        const std::string mixed = "[1,{\"a\":[2," + nested_arrays(MAX_NESTING_DEPTH - 2) + "]},3]";
        REQUIRE_FALSE(document::parse(mixed).has_value());
        const std::string fits = "[1,{\"a\":[2," + nested_arrays(MAX_NESTING_DEPTH - 3) + "]},3]";
        CHECK(document::parse(fits).has_value());

        // Far deeper input must be rejected without touching the recursion at all.
        const result<document> extreme = document::parse(nested_arrays(100000));
        REQUIRE_FALSE(extreme.has_value());
        CHECK(extreme.get_error().generic_code() == error_code::PARSE_FAILURE);

        const testing::temp_directory directory("core_json_depth");
        const std::filesystem::path file = directory.write_file("deep.json", nested_arrays(MAX_NESTING_DEPTH + 1));
        const result<document> fromFile = document::parse_file(file);
        REQUIRE_FALSE(fromFile.has_value());
        CHECK(fromFile.get_error().generic_code() == error_code::PARSE_FAILURE);
        CHECK(fromFile.get_error().message().find("deep.json") != std::string::npos);
    }

    TEST_CASE("document - serialisation and equality are iterative and handle trees past MAX_NESTING_DEPTH") {
        document deep = document::array();
        for (usize depth = 0; depth < MAX_NESTING_DEPTH; ++depth) {
            deep = array({std::move(deep)});
        }
        const std::string arrays = nested_arrays(MAX_NESTING_DEPTH + 1);
        CHECK(deep.to_string() == arrays);
        CHECK(deep.to_canonical_string() == arrays);
        CHECK(deep == deep);

        document other = document::array();
        for (usize depth = 0; depth < MAX_NESTING_DEPTH; ++depth) {
            other = array({std::move(other)});
        }
        CHECK(other == deep);
        other.push_back(1);
        CHECK(other != deep);

        document objects = document::object();
        objects.set("a", 1);
        for (usize depth = 0; depth < MAX_NESTING_DEPTH; ++depth) {
            objects = object({{"z", 2}, {"a", std::move(objects)}});
        }
        const std::string canonical = objects.to_canonical_string();
        CHECK(canonical.size() == 7 + 12 * MAX_NESTING_DEPTH);
        CHECK(canonical.find(R"({"a":{"a":)") == 0);
        CHECK(canonical.find(R"({"a":{"a":1},"z":2})") != std::string::npos);
        CHECK(objects == objects);
    }

    TEST_CASE("document - documents built on different threads do not interfere") {
        constexpr int THREAD_COUNT = 8;
        constexpr int ITERATIONS = 200;
        std::atomic<int> failures = 0;

        std::vector<std::thread> threads = {};
        for (int threadIndex = 0; threadIndex < THREAD_COUNT; ++threadIndex) {
            threads.emplace_back([threadIndex, &failures] {
                for (int iteration = 0; iteration < ITERATIONS; ++iteration) {
                    result<document> parsed = document::parse(R"({"z":[1,2,3],"a":{"k":"v"}})");
                    if (!parsed.has_value()) {
                        ++failures;
                        continue;
                    }
                    document doc = std::move(parsed).value();
                    doc.set("thread", threadIndex);
                    doc.set_pointer("/a/iteration", iteration);
                    doc.erase("z");

                    const std::string expected = std::format(
                      R"({{"a":{{"iteration":{},"k":"v"}},"thread":{}}})", iteration, threadIndex
                    );
                    if (doc.to_canonical_string() != expected) {
                        ++failures;
                    }
                    const document copy = doc;
                    if (copy != doc) {
                        ++failures;
                    }
                }
            });
        }
        for (std::thread &thread : threads) {
            thread.join();
        }

        CHECK(failures.load() == 0);
    }

    TEST_CASE("document - one document can be read from many threads at once") {
        document shared = document::object();
        for (i32 index = 0; index < 100; ++index) {
            shared.set(std::format("key{}", index), index);
        }
        shared.set("list", array({1, 2, 3}));
        const std::string canonical = shared.to_canonical_string();

        constexpr int THREAD_COUNT = 8;
        std::atomic<int> failures = 0;
        std::vector<std::thread> threads = {};
        for (int threadIndex = 0; threadIndex < THREAD_COUNT; ++threadIndex) {
            threads.emplace_back([&shared, &canonical, &failures] {
                for (int iteration = 0; iteration < 50; ++iteration) {
                    i64 sum = 0;
                    for (const auto [key, item] : shared.root().members()) {
                        sum += item.as_i64();
                    }
                    if (sum != 4950) {
                        ++failures;
                    }
                    if (shared.root()["key42"].to<i32>().value_or(-1) != 42) {
                        ++failures;
                    }
                    if (shared.at_pointer("/list/2").as_i64() != 3) {
                        ++failures;
                    }
                    if (shared.to_canonical_string() != canonical) {
                        ++failures;
                    }
                }
            });
        }
        for (std::thread &thread : threads) {
            thread.join();
        }

        CHECK(failures.load() == 0);
    }
}
