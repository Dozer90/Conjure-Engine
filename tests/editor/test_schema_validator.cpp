#include "conjure/testing/doctest.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/schema_validator.h"

#include <string>
#include <string_view>

// schema_validator.h: the JSON Schema subset every command's arguments are checked against.

using namespace conjure;
using namespace conjure::editor;

namespace {
    core::json::document parse(std::string_view text) {
        core::result<core::json::document> parsed = core::json::document::parse(text);
        REQUIRE_MESSAGE(parsed.has_value(), testing::describe_error(parsed));
        return std::move(parsed).value();
    }

    std::string message_of(const core::result<void> &result) {
        return result.has_value() ? std::string("ok") : result.get_error().message();
    }
}

TEST_SUITE("editor/schema_validator") {

    TEST_CASE("compiled_schema - required keys are enforced and named") {
        const core::result<compiled_schema> schema = compiled_schema::compile(
          R"({"type":"object","properties":{"name":{"type":"string"},"count":{"type":"integer"}},"required":["name"]})"
        );
        CONJURE_REQUIRE_OK(schema);
        CONJURE_CHECK_OK(schema.value().validate(parse(R"({"name":"x"})").root()));
        const core::result<void> missing = schema.value().validate(parse(R"({"count":1})").root());
        CONJURE_CHECK_ERROR(missing, protocol_error_code::INVALID_ARGS);
        CHECK(message_of(missing).find("name") != std::string::npos);
    }

    TEST_CASE("compiled_schema - a type mismatch names the path") {
        const core::result<compiled_schema> schema = compiled_schema::compile(
          R"({"type":"object","properties":{"position":{"type":"object","properties":{"x":{"type":"number"}}}}})"
        );
        CONJURE_REQUIRE_OK(schema);
        const core::result<void> bad = schema.value().validate(parse(R"({"position":{"x":"one"}})").root());
        CONJURE_CHECK_ERROR(bad, protocol_error_code::INVALID_ARGS);
        CHECK(message_of(bad).find("args.position.x") != std::string::npos);
        CHECK(message_of(bad).find("number") != std::string::npos);
    }

    TEST_CASE("compiled_schema - integer accepts whole numbers only and type lists accept alternatives") {
        const core::result<compiled_schema> schema = compiled_schema::compile(
          R"({"type":"object","properties":{"n":{"type":"integer"},"p":{"type":["string","null"]}}})"
        );
        CONJURE_REQUIRE_OK(schema);
        CONJURE_CHECK_OK(schema.value().validate(parse(R"({"n":3,"p":null})").root()));
        CONJURE_CHECK_OK(schema.value().validate(parse(R"({"p":"root"})").root()));
        CONJURE_CHECK_ERROR(schema.value().validate(parse(R"({"n":3.5})").root()), protocol_error_code::INVALID_ARGS);
        CONJURE_CHECK_ERROR(schema.value().validate(parse(R"({"p":7})").root()), protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("compiled_schema - enum restricts the values") {
        const core::result<compiled_schema> schema = compiled_schema::compile(
          R"({"type":"object","properties":{"mode":{"type":"string","enum":["mesh","bounds"]}}})"
        );
        CONJURE_REQUIRE_OK(schema);
        CONJURE_CHECK_OK(schema.value().validate(parse(R"({"mode":"mesh"})").root()));
        const core::result<void> bad = schema.value().validate(parse(R"({"mode":"physics"})").root());
        CONJURE_CHECK_ERROR(bad, protocol_error_code::INVALID_ARGS);
        CHECK(message_of(bad).find("physics") != std::string::npos);
    }

    TEST_CASE("compiled_schema - numeric ranges and array lengths") {
        const core::result<compiled_schema> schema = compiled_schema::compile(
          R"({"type":"object","properties":{"count":{"type":"integer","minimum":1,"maximum":10},)"
          R"("vec":{"type":"array","items":{"type":"number"},"minItems":3,"maxItems":3}}})"
        );
        CONJURE_REQUIRE_OK(schema);
        CONJURE_CHECK_OK(schema.value().validate(parse(R"({"count":10,"vec":[1,2,3]})").root()));
        CONJURE_CHECK_ERROR(schema.value().validate(parse(R"({"count":0})").root()), protocol_error_code::INVALID_ARGS);
        CONJURE_CHECK_ERROR(
          schema.value().validate(parse(R"({"count":11})").root()),
          protocol_error_code::INVALID_ARGS
        );
        CONJURE_CHECK_ERROR(
          schema.value().validate(parse(R"({"vec":[1,2]})").root()),
          protocol_error_code::INVALID_ARGS
        );
        const core::result<void> badItem = schema.value().validate(parse(R"({"vec":[1,"two",3]})").root());
        CONJURE_CHECK_ERROR(badItem, protocol_error_code::INVALID_ARGS);
        CHECK(message_of(badItem).find("vec[1]") != std::string::npos);
    }

    TEST_CASE("compiled_schema - nested objects validate recursively") {
        const core::result<compiled_schema> schema = compiled_schema::compile(
          R"({"type":"object","properties":{"transform":{"type":"object","properties":{)"
          R"("position":{"type":"array","items":{"type":"number"}},"scale":{"type":"array",)"
          R"("items":{"type":"number"}}},)"
          R"("required":["position"]}}})"
        );
        CONJURE_REQUIRE_OK(schema);
        CONJURE_CHECK_OK(schema.value().validate(parse(R"({"transform":{"position":[0,0,0]}})").root()));
        const core::result<void> missing = schema.value().validate(parse(R"({"transform":{"scale":[1,1,1]}})").root());
        CONJURE_CHECK_ERROR(missing, protocol_error_code::INVALID_ARGS);
        CHECK(message_of(missing).find("transform") != std::string::npos);
    }

    TEST_CASE("compiled_schema - additional properties are rejected at the top level unless allowed") {
        const core::result<compiled_schema> strict = compiled_schema::compile(R"({"type":"object",)"
        R"("properties":{"a":{"type":"integer"}}})");
        CONJURE_REQUIRE_OK(strict);
        const core::result<void> extra = strict.value().validate(parse(R"({"a":1,"b":2})").root());
        CONJURE_CHECK_ERROR(extra, protocol_error_code::INVALID_ARGS);
        CHECK(message_of(extra).find("b") != std::string::npos);

        const core::result<compiled_schema> open = compiled_schema::compile(R"({"type":"object",)"
        R"("additionalProperties":true})");
        CONJURE_REQUIRE_OK(open);
        CONJURE_CHECK_OK(open.value().validate(parse(R"({"anything":[1,2,3]})").root()));

        // Nested objects default to open so component values pass through.
        const core::result<compiled_schema> nested = compiled_schema::compile(
          R"({"type":"object","properties":{"value":{"type":"object","properties":{"x":{"type":"number"}}}}})"
        );
        CONJURE_REQUIRE_OK(nested);
        CONJURE_CHECK_OK(nested.value().validate(parse(R"({"value":{"x":1,"y":2}})").root()));
    }

    TEST_CASE("compiled_schema - an unsupported keyword fails to compile") {
        const core::result<compiled_schema> bad = compiled_schema::compile(R"({"type":"object",)"
        R"("patternProperties":{}})");
        CONJURE_CHECK_ERROR(bad, protocol_error_code::INVALID_ARGS);
        CONJURE_CHECK_ERROR(compiled_schema::compile("not json"), protocol_error_code::INVALID_ARGS);
        CONJURE_CHECK_ERROR(compiled_schema::compile(R"({"type":"wibble"})"), protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("validate_args - compiles and validates in one call, the root name is configurable") {
        CONJURE_CHECK_OK(validate_args(R"({"type":"object"})", parse("{}").root()));
        CONJURE_CHECK_ERROR(
          validate_args(R"({"type":"object"})", parse("[]").root()),
          protocol_error_code::INVALID_ARGS
        );
        const core::result<compiled_schema> schema = compile_schema(R"({"type":"object",)"
        R"("properties":{"k":{"type":"boolean"}}})");
        CONJURE_REQUIRE_OK(schema);
        const core::result<void> named = schema.value().validate(parse(R"({"k":1})").root(), "result");
        CONJURE_CHECK_ERROR(named, protocol_error_code::INVALID_ARGS);
        CHECK(message_of(named).find("result.k") != std::string::npos);
        CHECK(schema.value().schema().root()["type"].as_string() == "object");
    }
}
