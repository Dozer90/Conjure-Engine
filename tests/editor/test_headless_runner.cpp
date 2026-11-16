#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"

#include "editor_test_support.h"

#include "conjure/core/clock.h"
#include "conjure/core/json/document.h"
#include "conjure/core/log.h"
#include "conjure/editor/command_dispatcher.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/headless_runner.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/engine/project.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

// headless_runner.h (docs/design/04_editor.md 3.11, 08_testing.md 6.3 item 5).

using namespace conjure;
using namespace conjure::editor;

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

    TEST_CASE(
      "headless_runner::parse_command_file - a leading UTF-8 byte order mark is ignored for arrays and NDJSON"
    ) {
        // What Windows PowerShell 5.1 writes for Set-Content -Encoding utf8 and Out-File.
        const core::result<std::vector<protocol_request>> array = headless_runner::parse_command_file(
          "\xEF\xBB\xBF[{\"cmd\":\"a\"},{\"id\":\"x\",\"cmd\":\"b\"}]"
        );
        CONJURE_REQUIRE_OK(array);
        REQUIRE(array.value().size() == 2);
        CHECK(array.value()[0].command == "a");
        CHECK(array.value()[1].id.root().as_string() == "x");

        const core::result<std::vector<protocol_request>> ndjson = headless_runner::parse_command_file(
          "\xEF\xBB\xBF{\"cmd\":\"a\"}\r\n{\"id\":7,\"cmd\":\"b\"}\r\n"
        );
        CONJURE_REQUIRE_OK(ndjson);
        REQUIRE(ndjson.value().size() == 2);
        CHECK(ndjson.value()[0].command == "a");
        CHECK(ndjson.value()[0].id.root().as_i64() == 0);
        CHECK(ndjson.value()[1].id.root().as_i64() == 7);

        const core::result<std::vector<protocol_request>> onlyMark = headless_runner::parse_command_file(
          "\xEF\xBB\xBF\n"
        );
        CONJURE_REQUIRE_OK(onlyMark);
        CHECK(onlyMark.value().empty());
        // Only a leading mark is skipped: one in the middle of a line is still malformed JSON.
        CONJURE_CHECK_ERROR(
          headless_runner::parse_command_file("{\"cmd\":\"a\"}\n\xEF\xBB\xBF{\"cmd\":\"b\"}\n"),
          protocol_error_code::INVALID_ARGS
        );
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
        const std::vector<std::string> lines = editor_test::non_empty_lines(out.str());
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

    TEST_CASE("regression - F505 run_requests ends with RUNTIME_FAILURE when the output stream fails") {
        editor_test::headless_editor editor = {};
        editor.log().allow_errors();
        const core::result<std::vector<protocol_request>> requests = headless_runner::parse_command_file(
          R"([{"cmd":"entity.create","args":{"name":"a"}},{"cmd":"entity.create","args":{"name":"b"}}])"
        );
        CONJURE_REQUIRE_OK(requests);
        std::ostringstream out = {};
        out.setstate(std::ios::badbit);
        headless_runner runner = {};
        const exec_summary summary = runner.run_requests(editor.application(), requests.value(), out, true);
        // The first request ran and is counted; nothing after it could report its answer, so the run stopped.
        CHECK(summary.executed == 1);
        CHECK(summary.succeeded == 1);
        CHECK(summary.exitCode == 2);
        CHECK(editor.scene().world().entity_count() == 1);
        CHECK(editor.log().contains(core::log_level::ERR, "could not be written"));
    }

    TEST_CASE("exec_summary - record counts results and keeps the first failed index") {
        exec_summary summary = {};
        summary.record(true, 0);
        CHECK(summary.executed == 1);
        CHECK(summary.succeeded == 1);
        CHECK(summary.failed == 0);
        CHECK(summary.firstFailedIndex == -1);
        summary.record(false, 1);
        summary.record(true, 2);
        summary.record(false, 3);
        CHECK(summary.executed == 4);
        CHECK(summary.succeeded == 2);
        CHECK(summary.failed == 2);
        CHECK(summary.firstFailedIndex == 1);
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
        for (const std::string &line : editor_test::non_empty_lines(out.str())) {
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

    TEST_CASE("headless_runner::run_requests - editor.quit stops the run and its exit code becomes the summary's") {
        {
            editor_test::headless_editor editor = {};
            const core::result<std::vector<protocol_request>> requests = headless_runner::parse_command_file(
              R"([{"cmd":"entity.create","args":{"name":"a"}},{"cmd":"editor.quit","args":{"exitCode":7}},)"
              R"({"cmd":"entity.create","args":{"name":"c"}}])"
            );
            CONJURE_REQUIRE_OK(requests);
            std::ostringstream out = {};
            headless_runner runner = {};
            const exec_summary summary = runner.run_requests(editor.application(), requests.value(), out, false);
            CHECK(summary.executed == 2);
            CHECK(summary.failed == 0);
            CHECK(summary.exitCode == 7);
            CHECK(editor.scene().world().entity_count() == 1);
        }
        // The default exit code 0 does not hide an earlier failure under --keep-going.
        editor_test::headless_editor editor = {};
        const core::result<std::vector<protocol_request>> requests = headless_runner::parse_command_file(
          R"([{"cmd":"entity.destroy","args":{"entity":"Nobody"}},{"cmd":"editor.quit"},)"
          R"({"cmd":"entity.create","args":{"name":"c"}}])"
        );
        CONJURE_REQUIRE_OK(requests);
        std::ostringstream out = {};
        headless_runner runner = {};
        const exec_summary summary = runner.run_requests(editor.application(), requests.value(), out, true);
        CHECK(summary.executed == 2);
        CHECK(summary.failed == 1);
        CHECK(summary.exitCode == 1);
        CHECK(editor.scene().world().entity_count() == 0);
    }

    TEST_CASE(
      "headless_runner::run_requests - an asynchronous request is bounded by the dispatcher timeout in seconds"
    ) {
        editor_test::headless_editor editor = {};
        editor.context().dispatcher().set_async_timeout_seconds(0.2);
        // Two million frames would outlast a frame-count bound; the dispatcher answers after 0.2 s of wall time.
        // Its TIMEOUT frees the request, so --keep-going carries on with the next one.
        const core::result<std::vector<protocol_request>> requests = headless_runner::parse_command_file(
          R"([{"id":"w","cmd":"editor.wait_frames","args":{"frames":2000000}},)"
          R"({"id":"e","cmd":"entity.create","args":{"name":"after"}}])"
        );
        CONJURE_REQUIRE_OK(requests);
        std::ostringstream out = {};
        headless_runner runner = {};
        const exec_summary summary = runner.run_requests(editor.application(), requests.value(), out, true);
        editor.context().dispatcher().set_async_timeout_seconds(30.0);
        CHECK(summary.executed == 2);
        CHECK(summary.failed == 1);
        CHECK(summary.firstFailedIndex == 0);
        CHECK(editor.scene().world().entity_count() == 1);
        bool sawTimeout = false;
        for (const std::string &line : editor_test::non_empty_lines(out.str())) {
            const core::result<protocol_response> response = parse_response(line);
            if (response.has_value() && response.value().id.root().as_string() == "w") {
                sawTimeout = true;
                CHECK(response.value().errorCode == protocol_error_code::TIMEOUT);
                CHECK(response.value().errorMessage.find("did not complete within") != std::string::npos);
            }
        }
        CHECK(sawTimeout);
    }

    TEST_CASE(
      "headless_runner::run_requests - a request that is never answered ends the run after the runner's bound [slow]"
    ) {
        // Declared first so it outlives the editor whose dispatcher reads it.
        core::manual_clock frozen = {};
        editor_test::headless_editor editor = {};
        // A frozen dispatcher clock never reaches the deadline, so only the runner's own bound (twice the
        // timeout plus five seconds) can end the wait; a frame-count bound would run a million frames instead.
        editor.context().dispatcher().set_clock(&frozen);
        editor.context().dispatcher().set_async_timeout_seconds(0.001);
        const core::result<std::vector<protocol_request>> requests = headless_runner::parse_command_file(
          R"([{"id":"w","cmd":"editor.wait_frames","args":{"frames":2000000}},)"
          R"({"id":"e","cmd":"entity.create","args":{"name":"after"}}])"
        );
        CONJURE_REQUIRE_OK(requests);
        std::ostringstream out = {};
        headless_runner runner = {};
        const exec_summary summary = runner.run_requests(editor.application(), requests.value(), out, true);
        editor.context().dispatcher().set_clock(nullptr);
        editor.context().dispatcher().set_async_timeout_seconds(30.0);
        // The request is still pending, so a later request could take its late reply: the run ends even with
        // --keep-going.
        CHECK(summary.executed == 1);
        CHECK(summary.failed == 1);
        CHECK(summary.exitCode == 1);
        CHECK(editor.scene().world().entity_count() == 0);
        CHECK(runner.frames_pumped() < 2000000);
        bool sawTimeout = false;
        for (const std::string &line : editor_test::non_empty_lines(out.str())) {
            const core::result<protocol_response> response = parse_response(line);
            if (response.has_value() && response.value().id.root().as_string() == "w") {
                sawTimeout = true;
                CHECK(response.value().errorCode == protocol_error_code::TIMEOUT);
                CHECK(response.value().errorMessage.find("seconds") != std::string::npos);
            }
        }
        CHECK(sawTimeout);
    }

    TEST_CASE("headless_runner::run_requests - ${project} expands to the project open when the request runs") {
        editor_test::headless_editor editor = {};
        const core::result<std::vector<protocol_request>> requests = headless_runner::parse_command_file(
          R"([{"cmd":"project.create","args":{"path":"${project}/second","name":"Second"}},)"
          R"({"cmd":"project.open","args":{"path":"${project}/second"}},)"
          R"({"cmd":"scene.new","args":{"name":"s"}},)"
          R"({"cmd":"scene.set_settings","args":{"settings":{"root":"${project}"}}}])"
        );
        CONJURE_REQUIRE_OK(requests);
        std::ostringstream out = {};
        headless_runner runner = {};
        const exec_summary summary = runner.run_requests(editor.application(), requests.value(), out, false);
        CHECK_MESSAGE(summary.failed == 0, out.str());
        REQUIRE(editor.context().project() != nullptr);
        CHECK(editor.context().project()->description().name == "Second");
        // A string argument carries the expansion (an entity name may not hold the path's '/').
        const std::string secondRoot = editor.context().project()->root_directory().generic_string();
        const core::json::document info = editor.ok("scene.info");
        CHECK(info.root()["settings"]["root"].as_string() == secondRoot);
    }

    TEST_CASE("headless_runner::run_requests - the failure warning names the request id and its 1-based position") {
        testing::log_capture logs = {};
        editor_test::headless_editor editor = {};
        const core::result<std::vector<protocol_request>> requests = headless_runner::parse_command_file(
          R"([{"cmd":"entity.create","args":{"name":"a"}},{"id":12,"cmd":"entity.destroy","args":{"entity":"Nobody"}}])"
        );
        CONJURE_REQUIRE_OK(requests);
        std::ostringstream out = {};
        headless_runner runner = {};
        const exec_summary summary = runner.run_requests(editor.application(), requests.value(), out, false);
        CHECK(summary.failed == 1);
        CHECK(summary.firstFailedIndex == 1);
        CHECK(logs.contains(core::log_level::WARN, "exec: request #2 (id 12) 'entity.destroy' failed"));
    }

    TEST_CASE("headless_runner::load_command_file - reads a file without an engine and parses it") {
        // No editor exists here: the editor calls this before it starts an engine.
        testing::temp_directory directory = {};
        const std::filesystem::path array = directory.write_file("array.json", R"([{"cmd":"a"},{"id":"x","cmd":"b"}])");
        const core::result<std::vector<protocol_request>> fromArray = headless_runner::load_command_file(array);
        CONJURE_REQUIRE_OK(fromArray);
        REQUIRE(fromArray.value().size() == 2);
        CHECK(fromArray.value()[0].id.root().as_i64() == 0);
        CHECK(fromArray.value()[1].id.root().as_string() == "x");

        // What Windows PowerShell 5.1 writes: a byte order mark and CRLF line ends.
        const std::filesystem::path ndjson = directory.write_file(
          "lines.ndjson",
          "\xEF\xBB\xBF{\"cmd\":\"a\"}\r\n{\"id\":7,\"cmd\":\"b\"}\r\n"
        );
        const core::result<std::vector<protocol_request>> fromLines = headless_runner::load_command_file(ndjson);
        CONJURE_REQUIRE_OK(fromLines);
        REQUIRE(fromLines.value().size() == 2);
        CHECK(fromLines.value()[1].id.root().as_i64() == 7);

        // An empty file is a run of no requests, not an error.
        const core::result<std::vector<protocol_request>> fromEmpty = headless_runner::load_command_file(
          directory.write_file("empty.json", "")
        );
        CONJURE_REQUIRE_OK(fromEmpty);
        CHECK(fromEmpty.value().empty());
    }

    TEST_CASE("headless_runner::load_command_file - a file it cannot use is INVALID_ARGS naming --exec and the file") {
        testing::temp_directory directory = {};
        const core::result<std::vector<protocol_request>> none = headless_runner::load_command_file({});
        CONJURE_CHECK_ERROR(none, protocol_error_code::INVALID_ARGS);
        REQUIRE_FALSE(none.has_value());
        CHECK(none.get_error().message().find("--exec") != std::string::npos);

        const core::result<std::vector<protocol_request>> missing = headless_runner::load_command_file(
          directory.file("missing.json")
        );
        CONJURE_CHECK_ERROR(missing, protocol_error_code::INVALID_ARGS);
        REQUIRE_FALSE(missing.has_value());
        CHECK(missing.get_error().message().find("--exec: cannot read") != std::string::npos);
        CHECK(missing.get_error().message().find("missing.json") != std::string::npos);

        // A directory is not a command file on any platform.
        const core::result<std::vector<protocol_request>> folder = headless_runner::load_command_file(directory.path());
        CONJURE_CHECK_ERROR(folder, protocol_error_code::INVALID_ARGS);
        REQUIRE_FALSE(folder.has_value());
        CHECK(folder.get_error().message().find("--exec: cannot read") != std::string::npos);

        const core::result<std::vector<protocol_request>> truncated = headless_runner::load_command_file(
          directory.write_file("truncated.json", "[{\"cmd\":")
        );
        CONJURE_CHECK_ERROR(truncated, protocol_error_code::INVALID_ARGS);
        REQUIRE_FALSE(truncated.has_value());
        CHECK(truncated.get_error().message().find("--exec: cannot parse") != std::string::npos);
        CHECK(truncated.get_error().message().find("truncated.json") != std::string::npos);

        const core::result<std::vector<protocol_request>> badLine = headless_runner::load_command_file(
          directory.write_file("bad_line.ndjson", "{\"cmd\":\"a\"}\n\n{\"nope\":1}\n")
        );
        CONJURE_CHECK_ERROR(badLine, protocol_error_code::INVALID_ARGS);
        REQUIRE_FALSE(badLine.has_value());
        CHECK(badLine.get_error().message().find("bad_line.ndjson") != std::string::npos);
        CHECK(badLine.get_error().message().find("line 3") != std::string::npos);
    }

    TEST_CASE("headless_runner::run - the output goes to the file of --out, whose folder is created") {
        editor_test::headless_editor editor = {};
        headless_runner runner = {};
        const core::result<std::vector<protocol_request>> requests = headless_runner::parse_command_file(
          R"({"cmd":"scene.set_settings","args":{"settings":{"origin":"from ${project}"}}})"
        );
        CONJURE_REQUIRE_OK(requests);
        editor_options withOutput = editor.application().options();
        withOutput.outputFile = editor.project_directory() / "out" / "responses.ndjson";
        const core::result<exec_summary> summary = runner.run(editor.application(), withOutput, requests.value());
        CONJURE_REQUIRE_OK(summary);
        CHECK(summary.value().exitCode == 0);
        CHECK(summary.value().executed == 1);
        const std::string output = editor_test::read_text_file(withOutput.outputFile);
        CHECK(output.find("\"ok\":true") != std::string::npos);
        const core::json::document info = editor.ok("scene.info");
        CHECK(info.root()["settings"]["origin"].as_string() == "from " + editor.project_directory().generic_string());
    }

    TEST_CASE("headless_runner::run - an output file that cannot be opened is IO_ERROR and nothing runs") {
        editor_test::headless_editor editor = {};
        headless_runner runner = {};
        const core::result<std::vector<protocol_request>> requests = headless_runner::parse_command_file(
          R"([{"cmd":"entity.create","args":{"name":"never"}}])"
        );
        CONJURE_REQUIRE_OK(requests);
        editor_options blocked = editor.application().options();
        // A directory where the file should be.
        blocked.outputFile = editor.project_directory() / "assets";
        const core::result<exec_summary> summary = runner.run(editor.application(), blocked, requests.value());
        CONJURE_CHECK_ERROR(summary, protocol_error_code::IO_ERROR);
        REQUIRE_FALSE(summary.has_value());
        CHECK(summary.get_error().message().find("--out: cannot write") != std::string::npos);
        CHECK(editor.scene().world().entity_count() == 0);
    }

    TEST_CASE(
      "regression - code sweep F659 run_requests opens the output with the startup scene the engine could not load"
    ) {
        editor_test::headless_editor_options options = {};
        options.withScene = false;
        options.extraProjectJson = R"("startup_scene": "scenes/missing.scene.json")";
        editor_test::headless_editor editor(options);
        REQUIRE(editor.application().engine().startup_scene_failure().has_value());
        const core::result<std::vector<protocol_request>> requests = headless_runner::parse_command_file(
          R"([{"id":"new","cmd":"scene.new","args":{"name":"s"}},{"id":"a","cmd":"entity.create","args":{"name":"a"}}])"
        );
        CONJURE_REQUIRE_OK(requests);
        std::ostringstream out = {};
        headless_runner runner = {};
        const exec_summary summary = runner.run_requests(editor.application(), requests.value(), out, false);
        // The warning is information: neither the counts nor the exit code know about it.
        CHECK(summary.executed == 2);
        CHECK(summary.failed == 0);
        CHECK(summary.exitCode == 0);

        const std::vector<std::string> lines = editor_test::non_empty_lines(out.str());
        REQUIRE(lines.size() >= 3);
        const core::result<protocol_event> first = parse_event(lines.front());
        CONJURE_REQUIRE_OK(first);
        CHECK(first.value().name == "warning");
        CHECK(first.value().data.root()["code"].as_string() == "STARTUP_SCENE_FAILED");
        const std::string_view message = first.value().data.root()["message"].as_string();
        CHECK(message.find("scenes/missing.scene.json") != std::string_view::npos);
        CHECK(message == editor.application().engine().startup_scene_failure()->message());
        // Exactly once, and before the first response.
        usize warnings = 0;
        for (usize index = 1; index < lines.size(); ++index) {
            if (lines[index].find("STARTUP_SCENE_FAILED") != std::string::npos) {
                ++warnings;
            }
        }
        CHECK(warnings == 0);

        // A run of no requests still says it: the reader of an empty run learns why the world is empty.
        std::ostringstream empty = {};
        const exec_summary nothing = runner.run_requests(editor.application(), {}, empty, false);
        CHECK(nothing.executed == 0);
        CHECK(nothing.exitCode == 0);
        const std::vector<std::string> onlyLine = editor_test::non_empty_lines(empty.str());
        REQUIRE(onlyLine.size() == 1);
        CHECK(onlyLine.front().find("STARTUP_SCENE_FAILED") != std::string::npos);
    }

    TEST_CASE("headless_runner::run_requests - a project without a startup scene gets no startup warning") {
        editor_test::headless_editor editor = {};
        REQUIRE_FALSE(editor.application().engine().startup_scene_failure().has_value());
        const core::result<std::vector<protocol_request>> requests = headless_runner::parse_command_file(
          R"([{"cmd":"editor.state"}])"
        );
        CONJURE_REQUIRE_OK(requests);
        std::ostringstream out = {};
        headless_runner runner = {};
        const exec_summary summary = runner.run_requests(editor.application(), requests.value(), out, false);
        CHECK(summary.failed == 0);
        const std::vector<std::string> lines = editor_test::non_empty_lines(out.str());
        REQUIRE_FALSE(lines.empty());
        CHECK(parse_response(lines.front()).has_value());
        CHECK(out.str().find("STARTUP_SCENE_FAILED") == std::string::npos);
    }

    TEST_CASE(
      "regression - code sweep F659 run_requests carries the warning of a project.open into a broken startup scene"
    ) {
        editor_test::headless_editor editor = {};
        const std::filesystem::path broken = editor.project_directory() / "broken";
        std::error_code ignored = {};
        std::filesystem::create_directories(broken / "assets", ignored);
        {
            std::ofstream stream(broken / "project.json", std::ios::binary | std::ios::trunc);
            REQUIRE(stream.is_open());
            stream << "{\n  \"schema_version\": 1,\n  \"name\": \"broken\",\n  \"assets_root\": \"assets\",\n"
              "  \"startup_scene\": \"scenes/gone.scene.json\"\n}\n";
        }
        const core::result<std::vector<protocol_request>> requests = headless_runner::parse_command_file(
          R"([{"id":"open","cmd":"project.open","args":{"path":"${project}/broken"}},)"
          R"({"id":"info","cmd":"project.info"}])"
        );
        CONJURE_REQUIRE_OK(requests);
        std::ostringstream out = {};
        headless_runner runner = {};
        const exec_summary summary = runner.run_requests(editor.application(), requests.value(), out, false);
        CHECK_MESSAGE(summary.failed == 0, out.str());
        CHECK(summary.exitCode == 0);

        // The first start was clean, so nothing is said before the first response; the warning of the restarted
        // engine lies between the two responses.
        const std::vector<std::string> lines = editor_test::non_empty_lines(out.str());
        isize openLine = -1;
        isize warningLine = -1;
        isize infoLine = -1;
        for (usize index = 0; index < lines.size(); ++index) {
            CAPTURE(index);
            const core::result<protocol_response> response = parse_response(lines[index]);
            if (response.has_value() && response.value().id.root().as_string() == "open") {
                openLine = static_cast<isize>(index);
            }
            if (response.has_value() && response.value().id.root().as_string() == "info") {
                infoLine = static_cast<isize>(index);
            }
            if (lines[index].find("STARTUP_SCENE_FAILED") != std::string::npos) {
                CHECK(warningLine < 0);
                warningLine = static_cast<isize>(index);
                CHECK(lines[index].find("scenes/gone.scene.json") != std::string::npos);
            }
        }
        CHECK(openLine >= 0);
        CHECK(warningLine > openLine);
        CHECK(infoLine > warningLine);
    }
}
