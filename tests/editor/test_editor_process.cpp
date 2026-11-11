#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/process.h"
#include "conjure/core/string_utils.h"
#include "conjure/editor/headless_runner.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/engine/exit_code.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// The real executable (docs/design/08_testing.md 6.3 item 8, label process): conjure_editor --headless --exec runs
// the Grid Sweeper file into a scratch project and saves the same scene the in-process runner produces.

#ifndef CONJURE_EDITOR_EXECUTABLE
    #define CONJURE_EDITOR_EXECUTABLE ""
#endif

using namespace conjure;
using namespace conjure::editor;

namespace {
    void write_project(const std::filesystem::path &directory) {
        std::error_code ignored = {};
        std::filesystem::create_directories(directory / "assets", ignored);
        std::ofstream stream(directory / "project.json", std::ios::binary | std::ios::trunc);
        REQUIRE(stream.is_open());
        stream << "{\n  \"schema_version\": 1,\n  \"name\": \"process_test\",\n  \"assets_root\": \"assets\"\n}\n";
    }

    core::process_result run_editor(const std::vector<std::string> &arguments) {
        core::process_desc desc = {};
        desc.executable = CONJURE_EDITOR_EXECUTABLE;
        desc.arguments = arguments;
        desc.timeoutSeconds = 600.0;
        core::result<core::process_result> result = core::process::run(desc);
        REQUIRE_MESSAGE(result.has_value(), testing::describe_error(result));
        return std::move(result).value();
    }
}

TEST_SUITE("editor/editor_process") {

    TEST_CASE("editor_process - conjure_editor builds Grid Sweeper headless and saves the same scene as the in-process "
      "run [process]") {
        const std::string executable = CONJURE_EDITOR_EXECUTABLE;
        REQUIRE_MESSAGE(!executable.empty(), "the conjure_editor executable path was not configured");
        REQUIRE_MESSAGE(std::filesystem::is_regular_file(executable), executable);
        testing::log_capture logs = {};
        testing::temp_directory directory = {};
        const std::filesystem::path project = directory.path() / "project";
        write_project(project);
        const std::filesystem::path commands = editor_test::fixture_directory() / "build_grid_sweeper.json";
        const std::filesystem::path output = directory.path() / "responses.ndjson";
        const core::process_result result = run_editor({
          "--headless", "--project", project.generic_string(), "--exec", commands.generic_string(), "--out",
          output.generic_string(),
          "--ai-port", "0", "--log-level", "warn",
        });
        CHECK_MESSAGE(result.exitCode == 0, result.standardOutput << result.standardError);
        CHECK_FALSE(result.timedOut);
        REQUIRE(std::filesystem::is_regular_file(output));
        usize responses = 0;
        const std::string outputText = editor_test::read_text_file(output);
        for (const std::string_view line : core::str::split_lines(outputText)) {
            const core::result<protocol_response> response = parse_response(line);
            if (response.has_value()) {
                ++responses;
                CHECK_MESSAGE(response.value().ok, line);
            }
        }
        CHECK(responses > 10);
        const std::filesystem::path saved = project / "assets" / "scenes" / "grid_sweeper.scene.json";
        REQUIRE(std::filesystem::is_regular_file(saved));

        // The same file in process: the fixed guids make the saved scenes identical.
        editor_test::headless_editor editor = {};
        const core::result<std::vector<protocol_request>> requests =
          headless_runner::parse_command_file(editor_test::read_text_file(commands));
        CONJURE_REQUIRE_OK(requests);
        std::ostringstream out = {};
        headless_runner runner = {};
        REQUIRE(runner.run_requests(editor.application(), requests.value(), out, false).failed == 0);
        const std::string inProcess = editor_test::read_text_file(
          editor.project_directory() / "assets" / "scenes" / "grid_sweeper.scene.json"
        );
        CHECK(editor_test::read_text_file(saved) == inProcess);
    }

    TEST_CASE("editor_process - a bad flag exits 3 with the usage and a missing project exits 2 [process]") {
        const std::string executable = CONJURE_EDITOR_EXECUTABLE;
        REQUIRE_MESSAGE(!executable.empty(), "the conjure_editor executable path was not configured");
        testing::temp_directory directory = {};
        const core::process_result badFlag = run_editor({"--headless", "--out", "x.ndjson"});
        CHECK(badFlag.exitCode == static_cast<int>(engine::exit_code::BAD_ARGUMENTS));
        CHECK(badFlag.standardError.find("--exec") != std::string::npos);
        const core::process_result missingProject = run_editor({
          "--headless", "--project", (
            directory.path() / "nowhere"
          ).generic_string(), "--ai-port", "0", "--exec", "none.json",
        });
        CHECK(missingProject.exitCode == static_cast<int>(engine::exit_code::INIT_FAILURE));
        const core::process_result help = run_editor({"--help"});
        CHECK(help.exitCode == 0);
        CHECK(help.standardOutput.find("--ai-port") != std::string::npos);
    }

    TEST_CASE("editor_process - a failing command file exits 1 and names the failing index [process]") {
        const std::string executable = CONJURE_EDITOR_EXECUTABLE;
        REQUIRE_MESSAGE(!executable.empty(), "the conjure_editor executable path was not configured");
        testing::temp_directory directory = {};
        const std::filesystem::path project = directory.path() / "project";
        write_project(project);
        const std::filesystem::path commands = directory.path() / "failing.json";
        {
            std::ofstream stream(commands, std::ios::binary | std::ios::trunc);
            stream << R"([{"cmd":"scene.new","args":{"name":"s"}},{"cmd":"entity.destroy","args":{"entity":"Nobody"}},)"
            R"({"cmd":"entity.create","args":{"name":"never"}}])";
        }
        const core::process_result result = run_editor({
          "--headless", "--project", project.generic_string(), "--exec", commands.generic_string(), "--ai-port", "0",
          "--log-level", "warn",
        });
        CHECK(result.exitCode == 1);
        CHECK(result.standardOutput.find("ENTITY_NOT_FOUND") != std::string::npos);
        CHECK(result.standardOutput.find("\"never\"") == std::string::npos);
        const core::process_result keepGoing = run_editor({
          "--headless", "--project", project.generic_string(), "--exec", commands.generic_string(), "--ai-port", "0",
          "--keep-going",
          "--log-level", "warn",
        });
        CHECK(keepGoing.exitCode == 1);
        CHECK(keepGoing.standardOutput.find("\"id\":2,\"ok\":true") != std::string::npos);
    }
}
