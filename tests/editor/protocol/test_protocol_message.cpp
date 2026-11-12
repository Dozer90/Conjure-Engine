#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/protocol/protocol_message.h"

#include <string>
#include <string_view>
#include <utility>

// protocol_message.h: the newline-delimited JSON wire format (docs/design/04_editor.md 2.3).

using namespace conjure;
using namespace conjure::editor;

TEST_SUITE("editor/protocol/protocol_message") {

    TEST_CASE("parse_request - accepts integer, string and missing ids") {
        const core::result<protocol_request> integer = parse_request(R"({"id":7,"cmd":"editor.state"})");
        CONJURE_REQUIRE_OK(integer);
        CHECK(integer.value().id.root().as_i64() == 7);
        CHECK(integer.value().command == "editor.state");
        CHECK(integer.value().args.root().is_object());

        const core::result<protocol_request> text = parse_request(R"({"id":"abc","cmd":"x","args":{"a":1},)"
        R"("coalesceKey":9})");
        CONJURE_REQUIRE_OK(text);
        CHECK(text.value().id.root().as_string() == "abc");
        CHECK(text.value().args.root()["a"].as_i64() == 1);
        CHECK(text.value().coalesceKey == 9);

        const core::result<protocol_request> missing = parse_request(R"({"cmd":"x"})");
        CONJURE_REQUIRE_OK(missing);
        CHECK(missing.value().id.root().is_null());
    }

    TEST_CASE("parse_request - rejects a missing cmd, non-object args, bad ids and non-JSON") {
        CONJURE_CHECK_ERROR(parse_request(R"({"id":1})"), protocol_error_code::INVALID_REQUEST);
        CONJURE_CHECK_ERROR(parse_request(R"({"id":1,"cmd":""})"), protocol_error_code::INVALID_REQUEST);
        CONJURE_CHECK_ERROR(parse_request(R"({"id":1,"cmd":"x","args":[1]})"), protocol_error_code::INVALID_REQUEST);
        CONJURE_CHECK_ERROR(parse_request(R"({"id":{"k":1},"cmd":"x"})"), protocol_error_code::INVALID_REQUEST);
        CONJURE_CHECK_ERROR(
          parse_request(R"({"id":1,"cmd":"x","coalesceKey":-1})"),
          protocol_error_code::INVALID_REQUEST
        );
        CONJURE_CHECK_ERROR(parse_request("[1,2,3]"), protocol_error_code::INVALID_REQUEST);
        CONJURE_CHECK_ERROR(parse_request("{not json"), protocol_error_code::PARSE_ERROR);
        CONJURE_CHECK_ERROR(parse_request(""), protocol_error_code::PARSE_ERROR);
    }

    TEST_CASE("parse_request - a line above the limit is MESSAGE_TOO_LARGE") {
        const std::string huge(MAX_MESSAGE_BYTES + 1, 'x');
        CONJURE_CHECK_ERROR(parse_request(huge), protocol_error_code::MESSAGE_TOO_LARGE);
    }

    TEST_CASE("serialise - response golden text") {
        const protocol_response ok = make_ok_response(make_id(3).root(), core::json::object({{"guid", "g"}, {"n", 2}}));
        CHECK(serialise(ok) == R"({"id":3,"ok":true,"result":{"guid":"g","n":2}})");
        const protocol_response failed = make_error_response(
          make_id("req-1").root(),
          protocol_error_code::INVALID_ARGS,
          "args.name: expected string",
          core::json::object({{"path", "args.name"}})
        );
        CHECK(
          serialise(failed) ==
          R"({"id":"req-1","ok":false,"error":{"code":"INVALID_ARGS","message":"args.name: expected string",)"
          R"("details":{"path":"args.name"}}})"
        );
        const protocol_response nullId = make_error_response(null_id().root(), protocol_error_code::PARSE_ERROR, "bad");
        CHECK(serialise(nullId) == R"({"id":null,"ok":false,"error":{"code":"PARSE_ERROR","message":"bad",)"
        R"("details":{}}})");
    }

    TEST_CASE("serialise - event golden text") {
        protocol_event event = {};
        event.name = "scene.changed";
        event.seq = 12;
        event.frame = 4;
        event.data = core::json::object({{"revision", 5}, {"label", "Rename"}});
        CHECK(serialise(event) == R"({"event":"scene.changed","seq":12,"frame":4,"data":{"revision":5,)"
        R"("label":"Rename"}})");
    }

    TEST_CASE("serialise - request golden text omits a zero coalesce key") {
        protocol_request request = {};
        request.id = make_id(1);
        request.command = "entity.create";
        request.args = core::json::object({{"name", "Cell"}});
        CHECK(serialise(request) == R"({"id":1,"cmd":"entity.create","args":{"name":"Cell"}})");
        request.coalesceKey = 42;
        CHECK(serialise(request) == R"({"id":1,"cmd":"entity.create","args":{"name":"Cell"},"coalesceKey":42})");
    }

    TEST_CASE("parse_response and parse_event - round trip the serialised forms") {
        const protocol_response ok = make_ok_response(make_id(9).root(), core::json::object({{"x", 1}}));
        const core::result<protocol_response> parsedOk = parse_response(serialise(ok));
        CONJURE_REQUIRE_OK(parsedOk);
        CHECK(parsedOk.value().ok);
        CHECK(parsedOk.value().id.root().as_i64() == 9);
        CHECK(parsedOk.value().result.root()["x"].as_i64() == 1);

        const protocol_response failed = make_error_response(make_id(2).root(), protocol_error_code::BUSY, "too many");
        const core::result<protocol_response> parsedFailed = parse_response(serialise(failed));
        CONJURE_REQUIRE_OK(parsedFailed);
        CHECK_FALSE(parsedFailed.value().ok);
        CHECK(parsedFailed.value().errorCode == protocol_error_code::BUSY);
        CHECK(parsedFailed.value().errorMessage == "too many");
        CONJURE_CHECK_ERROR(parse_response(R"({"id":1})"), protocol_error_code::INVALID_REQUEST);
        CONJURE_CHECK_ERROR(parse_response(R"({"id":1,"ok":false})"), protocol_error_code::INVALID_REQUEST);

        protocol_event event = {};
        event.name = "log";
        event.seq = 3;
        event.frame = 1;
        event.data = core::json::object({{"level", "warn"}});
        const core::result<protocol_event> parsedEvent = parse_event(serialise(event));
        CONJURE_REQUIRE_OK(parsedEvent);
        CHECK(parsedEvent.value().name == "log");
        CHECK(parsedEvent.value().seq == 3);
        CHECK(parsedEvent.value().data.root()["level"].as_string() == "warn");
        CONJURE_CHECK_ERROR(parse_event(R"({"seq":1})"), protocol_error_code::INVALID_REQUEST);
    }

    TEST_CASE("regression - F188 strings that are not UTF-8 reach the wire as U+FFFD instead of asserting") {
        // The JSON serialiser refuses the bytes and logs a warning before the sanitised copy is written.
        testing::log_capture log = {};
        const std::string replaced = "Jos\xEF\xBF\xBD";
        core::json::document result = core::json::object({
          {"name", std::string_view("Jos\xE9")},
          {"list", core::json::array({std::string_view("Jos\xE9"), 3})},
          {"nested", core::json::object({{"deep", std::string_view("Jos\xE9")}, {"flag", true}})},
        });
        result.set(std::string_view("bad\xFFkey"), 1);
        const protocol_response ok = make_ok_response(make_id(4).root(), std::move(result));
        const core::result<protocol_response> parsedOk = parse_response(serialise(ok));
        CONJURE_REQUIRE_OK(parsedOk);
        const core::json::value root = parsedOk.value().result.root();
        CHECK(root["name"].as_string() == replaced);
        REQUIRE(root["list"].size() == 2);
        CHECK(root["list"][0].as_string() == replaced);
        CHECK(root["list"][1].as_i64() == 3);
        CHECK(root["nested"]["deep"].as_string() == replaced);
        CHECK(root["nested"]["flag"].as_bool());
        CHECK(root["bad\xEF\xBF\xBDkey"].as_i64() == 1);

        const protocol_response failed = make_error_response(
          make_id(5).root(),
          protocol_error_code::IO_ERROR,
          "cannot read 'Jos\xE9.cs'",
          core::json::object({{"path", std::string_view("Jos\xE9.cs")}})
        );
        const core::result<protocol_response> parsedFailed = parse_response(serialise(failed));
        CONJURE_REQUIRE_OK(parsedFailed);
        CHECK(parsedFailed.value().errorMessage == "cannot read '" + replaced + ".cs'");
        CHECK(parsedFailed.value().errorDetails.root()["path"].as_string() == replaced + ".cs");

        protocol_event event = {};
        event.name = "log";
        event.data = core::json::object({{"message", std::string_view("Jos\xE9")}});
        const core::result<protocol_event> parsedEvent = parse_event(serialise(event));
        CONJURE_REQUIRE_OK(parsedEvent);
        CHECK(parsedEvent.value().data.root()["message"].as_string() == replaced);

        protocol_request request = {};
        request.id = make_id(6);
        request.command = "script.read";
        request.args = core::json::object({{"path", std::string_view("Jos\xE9.cs")}});
        const core::result<protocol_request> parsedRequest = parse_request(serialise(request));
        CONJURE_REQUIRE_OK(parsedRequest);
        CHECK(parsedRequest.value().args.root()["path"].as_string() == replaced + ".cs");
    }

    TEST_CASE("protocol_error_code - every code round trips through the wire name") {
        for (const protocol_error_code code : all_error_codes()) {
            const protocol_response response = make_error_response(null_id().root(), code, "m");
            const core::result<protocol_response> parsed = parse_response(serialise(response));
            CONJURE_REQUIRE_OK(parsed);
            CHECK(parsed.value().errorCode == code);
        }
        CHECK(PROTOCOL_VERSION == 1);
    }
}
