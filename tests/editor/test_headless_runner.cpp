#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/string_utils.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/headless_runner.h"
#include "conjure/editor/protocol/protocol_message.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// headless_runner.h (docs/design/04_editor.md 3.11, 08_testing.md 6.3 item 5).

using namespace conjure;
using namespace conjure::editor;

namespace {
    std::vector<std::string> lines_of(const std::string &text) {
        std::vector<std::string> lines = {};
        for (const std::string_view line : core::str::split_lines(text)) {
            if (!line.empty()) {
                lines.emplace_back(line);
            }
        }
        return lines;
    }
}

TEST_SUITE("editor/headless_runner") {

    TEST_CASE("headless_runner::parse_command_file - accepts a JSON array and NDJSON, numbering missing ids") {
        const core::result<std::vector<protocol_request>> array = headless_runner::parse_command_file(
          R"([{"cmd":"a"},{"id":"x","cmd":"b","args":{"k":1}}])"
        );
        CONJURE_REQUIRE_OK(array);
        REQUIRE(array.value().size() == 2);
        CHECK(array.value()[0].id.root().as_i64() == 0);
        CHECK(array.value()[1].id.root().as_string() == "x");
        CHECK(array.value()[1].args.root()["k"].as_i64() == 1);

        const core::result<std::vector<protocol_request>> ndjson = headless_runner::parse_command_file(
          "{\"cmd\":\"a\"}\n\n{\"id\":7,\"cmd\":\"b\"}\r\n"
        );
        CONJURE_REQUIRE_OK(ndjson);
        REQUIRE(ndjson.value().size() == 2);
        CHECK(ndjson.value()[0].id.root().as_i64() == 0);
        CHECK(ndjson.value()[1].id.root().as_i64() == 7);

        const core::result<std::vector<protocol_request>> empty = headless_runner::parse_command_file("  \n");
        CONJURE_REQUIRE_OK(empty);
        CHECK(empty.value().empty());
        CONJURE_CHECK_ERROR(headless_runner::parse_command_file("[{\"nope\":1}]"), protocol_error_code::INVALID_ARGS);
        CONJURE_CHECK_ERROR(
          headless_runner::parse_command_file("{\"cmd\":\"a\"}\nnot json\n"),
          protocol_error_code::INVALID_ARGS
        );
        CONJURE_CHECK_ERROR(headless_runner::parse_command_file("[1, 2"), protocol_error_code::INVALID_ARGS);
    }

    TEST_CASE("headless_runner::expand_placeholders - replaces ${project} in every string argument") {
        protocol_request request = {};
        request.command = "asset.import";
        request.args = core::json::object({
          {"source", "${project}/incoming/a.png"},
          {"nested", core::json::object({{"list", core::json::array({"${project}/x", 3, "plain"})}})},
        });
        headless_runner::expand_placeholders(request, std::filesystem::path("C:/proj"));
        CHECK(request.args.root()["source"].as_string() == "C:/proj/incoming/a.png");
        CHECK(request.args.root()["nested"]["list"][0].as_string() == "C:/proj/x");
        CHECK(request.args.root()["nested"]["list"][1].as_i64() == 3);
        CHECK(request.args.root()["nested"]["list"][2].as_string() == "plain");
    }

    TEST_CASE("headless_runner::run_requests - stops at the first failure with exit code 1 and identifies it") {
        editor_test::headless_editor editor = {};
        const core::result<std::vector<protocol_request>> requests = headless_runner::parse_command_file(
          R"([{"cmd":"entity.create","args":{"name":"a"}},{"cmd":"entity.destroy","args":{"entity":"Nobody"}},)"
          R"({"cmd":"entity.create","args":{"name":"c"}}])"
        );
        CONJURE_REQUIRE_OK(requests);
        std::ostringstream out = {};
        headless_runner runner = {};
        const exec_summary summary = runner.run_requests(editor.application(), requests.value(), out, false);
        CHECK(summary.executed == 2);
        CHECK(summary.succeeded == 1);
        CHECK(summary.failed == 1);
        CHECK(summary.firstFailedIndex == 1);
        CHECK(summary.exitCode == 1);
        CHECK(editor.scene().world().entity_count() == 1);
        const std::vector<std::string> lines = lines_of(out.str());
        bool sawFailure = false;
        for (const std::string &line : lines) {
            const core::result<protocol_response> response = parse_response(line);
            if (response.has_value() && !response.value().ok) {
                sawFailure = true;
                CHECK(response.value().id.root().as_i64() == 1);
                CHECK(response.value().errorCode == protocol_error_code::ENTITY_NOT_FOUND);
            }
        }
        CHECK(sawFailure);
        const core::json::document json = summary.to_json();
        CHECK(json.root()["firstFailedIndex"].as_i64() == 1);
        CHECK(json.root()["exitCode"].as_i64() == 1);
    }

    TEST_CASE("headless_runner::run_requests - keep going runs every request") {
        editor_test::headless_editor editor = {};
        const core::result<std::vector<protocol_request>> requests = headless_runner::parse_command_file(
          R"([{"cmd":"entity.create","args":{"name":"a"}},{"cmd":"entity.destroy","args":{"entity":"Nobody"}},)"
          R"({"cmd":"entity.create","args":{"name":"c"}}])"
        );
        CONJURE_REQUIRE_OK(requests);
        std::ostringstream out = {};
        headless_runner runner = {};
        const exec_summary summary = runner.run_requests(editor.application(), requests.value(), out, true);
        CHECK(summary.executed == 3);
        CHECK(summary.succeeded == 2);
        CHECK(summary.failed == 1);
        CHECK(summary.exitCode == 1);
        CHECK(editor.scene().world().entity_count() == 2);
    }

    TEST_CASE("headless_runner::run_requests - the output carries events and asynchronous replies are pumped") {
        editor_test::headless_editor editor = {};
        const core::result<std::vector<protocol_request>> requests = headless_runner::parse_command_file(
          R"([{"id":"w","cmd":"editor.wait_frames","args":{"frames":3}},{"id":"e","cmd":"entity.create",)"
          R"("args":{"name":"after"}}])"
        );
        CONJURE_REQUIRE_OK(requests);
        std::ostringstream out = {};
        headless_runner runner = {};
        const exec_summary summary = runner.run_requests(editor.application(), requests.value(), out, false);
        CHECK(summary.failed == 0);
        CHECK(summary.exitCode == 0);
        CHECK(runner.frames_pumped() >= 3);
        bool sawEvent = false;
        bool sawWait = false;
        for (const std::string &line : lines_of(out.str())) {
            if (parse_event(line).has_value()) {
                sawEvent = true;
            }
            const core::result<protocol_response> response = parse_response(line);
            if (response.has_value() && response.value().id.root().as_string() == "w") {
                sawWait = true;
                CHECK(response.value().ok);
            }
        }
        CHECK(sawEvent);
        CHECK(sawWait);
    }

    TEST_CASE(
      "headless_runner::run - a missing or malformed file is INVALID_ARGS (exit 3) and output goes to the file"
    ) {
        editor_test::headless_editor editor = {};
        headless_runner runner = {};
        editor_options missing = editor.application().options();
        missing.execFile = editor.project_directory() / "missing.json";
        CONJURE_CHECK_ERROR(runner.run(editor.application(), missing), protocol_error_code::INVALID_ARGS);
        editor_options none = editor.application().options();
        none.execFile.clear();
        CONJURE_CHECK_ERROR(runner.run(editor.application(), none), protocol_error_code::INVALID_ARGS);

        const std::filesystem::path malformed = editor.project_directory() / "bad.json";
        {
            std::ofstream stream(malformed, std::ios::binary | std::ios::trunc);
            stream << "[{\"cmd\":";
        }
        editor_options badFile = editor.application().options();
        badFile.execFile = malformed;
        CONJURE_CHECK_ERROR(runner.run(editor.application(), badFile), protocol_error_code::INVALID_ARGS);

        const std::filesystem::path good = editor.project_directory() / "good.json";
        {
            std::ofstream stream(good, std::ios::binary | std::ios::trunc);
            stream << R"({"cmd":"entity.create","args":{"name":"from ${project}"}})";
        }
        editor_options withOutput = editor.application().options();
        withOutput.execFile = good;
        withOutput.outputFile = editor.project_directory() / "out" / "responses.ndjson";
        const core::result<exec_summary> summary = runner.run(editor.application(), withOutput);
        CONJURE_REQUIRE_OK(summary);
        CHECK(summary.value().exitCode == 0);
        const std::string output = editor_test::read_text_file(withOutput.outputFile);
        CHECK(output.find("\"ok\":true") != std::string::npos);
        const core::json::document found = editor.ok(
          "entity.find",
          core::json::object({{"name", "from " + editor.project_directory().generic_string()}})
        );
        CHECK(found.root()["total"].as_i64() == 1);
    }
}
