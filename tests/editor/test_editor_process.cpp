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
#include <string_view>
#include <system_error>
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

    core::process_result run_editor(const std::vector<std::string> &arguments, f64 timeoutSeconds = 600.0) {
        core::process_desc desc = {};
        desc.executable = CONJURE_EDITOR_EXECUTABLE;
        desc.arguments = arguments;
        desc.timeoutSeconds = timeoutSeconds;
        core::result<core::process_result> result = core::process::run(desc);
        REQUIRE_MESSAGE(result.has_value(), testing::describe_error(result));
        return std::move(result).value();
    }

    void write_text(const std::filesystem::path &path, std::string_view text) {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        REQUIRE(stream.is_open());
        stream << text;
    }

    /// <summary>
    /// The path as UTF-8 bytes, the way the editor's command line expects it on every platform.
    /// </summary>
    std::string path_argument(const std::filesystem::path &path) {
        const std::u8string generic = path.generic_u8string();
        return std::string(generic.begin(), generic.end());
    }

    /// <summary>
    /// The lines of standard output that are neither a protocol response nor a protocol event (empty = clean).
    /// </summary>
    std::string non_protocol_lines(const std::string &output) {
        std::string stray = {};
        for (const std::string_view line : core::str::split_lines(output)) {
            if (line.empty() || parse_response(line).has_value() || parse_event(line).has_value()) {
                continue;
            }
            stray += line;
            stray += '\n';
        }
        return stray;
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
        // 04_editor.md 3.1, 3.11: a startup or project failure is 2 (engine::exit_code has no value of its own
        // for it; INIT_FAILURE is 1, which the editor uses for a failed command).
        CHECK_MESSAGE(missingProject.exitCode == 2, missingProject.standardError);
        CHECK(missingProject.standardError.find("PROJECT_NOT_FOUND") != std::string::npos);
        const core::process_result help = run_editor({"--help"});
        CHECK(help.exitCode == 0);
        CHECK(help.standardOutput.find("--ai-port") != std::string::npos);
    }

    TEST_CASE("regression - F079 editor_process bad arguments exit 3 and project settings that do not validate exit 2 "
      "[process]") {
        const std::string executable = CONJURE_EDITOR_EXECUTABLE;
        REQUIRE_MESSAGE(!executable.empty(), "the conjure_editor executable path was not configured");
        testing::temp_directory directory = {};
        const std::filesystem::path project = directory.path() / "project";
        write_project(project);
        constexpr int BAD_ARGUMENTS_EXIT = static_cast<int>(engine::exit_code::BAD_ARGUMENTS);
        constexpr std::string_view HELP_HINT = "'conjure_editor --help'";
        // Every run names a command file that does not exist: startup fails before it is read, and a regression
        // that let startup pass ends at the unreadable file instead of looping frames until the timeout.
        SUBCASE("an option neither the editor nor the engine knows") {
            const core::process_result result = run_editor({
              "--headless", "--bogus-flag", "--ai-port", "0", "--exec", "x.json",
            });
            CHECK_MESSAGE(result.exitCode == BAD_ARGUMENTS_EXIT, result.standardError);
            CHECK(result.standardError.find("--bogus-flag") != std::string::npos);
            CHECK(result.standardError.find(HELP_HINT) != std::string::npos);
        }
        SUBCASE("an unknown option is named before a missing project is noticed") {
            const core::process_result result = run_editor({
              "--headless", "--project", path_argument(directory.path() / "nowhere"), "--bogus-flag", "--ai-port", "0",
              "--exec", "x.json",
            });
            CHECK_MESSAGE(result.exitCode == BAD_ARGUMENTS_EXIT, result.standardError);
            CHECK(result.standardError.find("--bogus-flag") != std::string::npos);
            CHECK(result.standardError.find("PROJECT_NOT_FOUND") == std::string::npos);
        }
        SUBCASE("an engine flag value that does not parse") {
            const core::process_result result = run_editor({
              "--headless", "--frames", "abc", "--ai-port", "0", "--exec", "x.json",
            });
            CHECK_MESSAGE(result.exitCode == BAD_ARGUMENTS_EXIT, result.standardError);
            CHECK(result.standardError.find("--frames") != std::string::npos);
        }
        SUBCASE("an engine flag that makes the merged configuration invalid") {
            // Above core::jobs::MAX_WORKER_COUNT; the project alone validates.
            const core::process_result result = run_editor({
              "--headless", "--project", path_argument(project), "--jobs", "100000", "--ai-port", "0", "--exec",
              "x.json",
            });
            CHECK_MESSAGE(result.exitCode == BAD_ARGUMENTS_EXIT, result.standardError);
            CHECK(result.standardError.find("INVALID_CONFIG") != std::string::npos);
            CHECK(result.standardError.find(HELP_HINT) != std::string::npos);
        }
        SUBCASE("project settings that do not validate are the project's fault") {
            // A frame delta cap below the fixed step fails validate() with or without any flag.
            write_text(
              project / "project.json",
              "{\n  \"schema_version\": 1,\n  \"name\": \"process_test\",\n  \"assets_root\": \"assets\",\n"
              "  \"time\": { \"fixed_hz\": 60, \"max_frame_delta_seconds\": 0.001 }\n}\n"
            );
            const core::process_result result = run_editor({
              "--headless", "--project", path_argument(project), "--ai-port", "0", "--exec", "x.json",
            });
            CHECK_MESSAGE(result.exitCode == 2, result.standardError);
            CHECK(result.standardError.find("INVALID_CONFIG") != std::string::npos);
            CHECK(result.standardError.find("maxFrameDeltaSeconds") != std::string::npos);
            CHECK(result.standardError.find(HELP_HINT) == std::string::npos);
        }
    }

    TEST_CASE("regression - F037 editor_process a project path outside the active code page opens [process]") {
        const std::string executable = CONJURE_EDITOR_EXECUTABLE;
        REQUIRE_MESSAGE(!executable.empty(), "the conjure_editor executable path was not configured");
        testing::temp_directory directory = {};
        // Latin-1 (ü), Greek (λ) and CJK (日本) characters: no single legacy code page holds them all.
        const std::filesystem::path project =
          directory.path() / std::filesystem::path(std::u8string_view(u8"projekt_ü_λ_日本"));
        write_project(project);
        const std::filesystem::path commands = directory.path() / "commands.json";
        write_text(
          commands,
          R"([{"cmd":"scene.new","args":{"name":"s","path":"scenes/utf8"}},{"cmd":"scene.save"}])"
        );
        const core::process_result result = run_editor({
          "--headless", "--project", path_argument(project), "--exec", path_argument(commands), "--ai-port", "0",
          "--log-level", "warn",
        });
        CHECK_MESSAGE(result.exitCode == 0, result.standardOutput << result.standardError);
        CHECK(result.standardError.find("PROJECT_NOT_FOUND") == std::string::npos);
        CHECK(std::filesystem::is_regular_file(project / "assets" / "scenes" / "utf8.scene.json"));
    }

    TEST_CASE("regression - F083 editor_process a bare --exec writes only protocol lines to stdout [process]") {
        const std::string executable = CONJURE_EDITOR_EXECUTABLE;
        REQUIRE_MESSAGE(!executable.empty(), "the conjure_editor executable path was not configured");
        testing::temp_directory directory = {};
        const std::filesystem::path project = directory.path() / "project";
        write_project(project);
        const std::filesystem::path commands = directory.path() / "state.json";
        write_text(commands, R"([{"cmd":"editor.state"},{"cmd":"scene.new","args":{"name":"s"}}])");
        // No --log-level: the engine's INFO lines are produced and must all reach standard error.
        const core::process_result result = run_editor({
          "--headless", "--project", path_argument(project), "--exec", path_argument(commands), "--ai-port", "0",
        });
        CHECK_MESSAGE(result.exitCode == 0, result.standardOutput << result.standardError);
        CHECK(result.standardOutput.find("\"ok\":true") != std::string::npos);
        const std::string stray = non_protocol_lines(result.standardOutput);
        CHECK_MESSAGE(stray.empty(), stray);
        CHECK(result.standardError.find("[INFO ]") != std::string::npos);
    }

    TEST_CASE("regression - F158 editor_process a headless --ai-stdio session exits 0 once stdin ends [process]") {
        const std::string executable = CONJURE_EDITOR_EXECUTABLE;
        REQUIRE_MESSAGE(!executable.empty(), "the conjure_editor executable path was not configured");
        testing::temp_directory directory = {};
        const std::filesystem::path project = directory.path() / "project";
        write_project(project);
        // The child's standard input is the null device, so the input ends at once. Before the fix the editor
        // kept running until the timeout killed it.
        const core::process_result result = run_editor(
          {"--headless", "--project", path_argument(project), "--ai-stdio"},
          120.0
        );
        CHECK_FALSE(result.timedOut);
        CHECK_MESSAGE(result.exitCode == 0, result.standardOutput << result.standardError);
        const std::string stray = non_protocol_lines(result.standardOutput);
        CHECK_MESSAGE(stray.empty(), stray);
        CHECK(result.standardError.find("standard input closed") != std::string::npos);
    }

    TEST_CASE(
      "regression - code sweep F659 editor_process an unusable --exec file exits 3 before the engine starts [process]"
    ) {
        const std::string executable = CONJURE_EDITOR_EXECUTABLE;
        REQUIRE_MESSAGE(!executable.empty(), "the conjure_editor executable path was not configured");
        testing::temp_directory directory = {};
        const std::filesystem::path project = directory.path() / "project";
        write_project(project);
        constexpr int BAD_ARGUMENTS_EXIT = static_cast<int>(engine::exit_code::BAD_ARGUMENTS);
        // No --log-level: a started engine would announce itself on standard error, and it leaves its asset cache
        // under .conjure/ in the project.
        constexpr std::string_view ENGINE_RUNNING = " running (";
        SUBCASE("a file that does not exist") {
            const core::process_result result = run_editor({
              "--headless", "--project", path_argument(project), "--ai-port", "0", "--exec",
              path_argument(directory.path() / "nope.json"),
            });
            CHECK_MESSAGE(result.exitCode == BAD_ARGUMENTS_EXIT, result.standardError);
            CHECK(result.standardError.find("--exec: cannot read") != std::string::npos);
            CHECK(result.standardError.find("nope.json") != std::string::npos);
            CHECK(result.standardError.find("'conjure_editor --help'") != std::string::npos);
            CHECK(result.standardError.find(ENGINE_RUNNING) == std::string::npos);
            CHECK(result.standardOutput.empty());
            CHECK_FALSE(std::filesystem::exists(project / ".conjure"));
        }
        SUBCASE("a file that does not parse") {
            const std::filesystem::path commands = directory.path() / "broken.json";
            write_text(commands, "[{\"cmd\":\"editor.state\"},");
            const core::process_result result = run_editor({
              "--headless", "--project", path_argument(project), "--ai-port", "0", "--exec", path_argument(commands),
            });
            CHECK_MESSAGE(result.exitCode == BAD_ARGUMENTS_EXIT, result.standardError);
            CHECK(result.standardError.find("--exec: cannot parse") != std::string::npos);
            CHECK(result.standardError.find("broken.json") != std::string::npos);
            CHECK(result.standardError.find(ENGINE_RUNNING) == std::string::npos);
            CHECK(result.standardOutput.empty());
            CHECK_FALSE(std::filesystem::exists(project / ".conjure"));
        }
        SUBCASE("a file that parses starts the engine, which is what the two checks above look for") {
            const std::filesystem::path commands = directory.path() / "state.json";
            write_text(commands, R"([{"cmd":"editor.state"}])");
            const core::process_result result = run_editor({
              "--headless", "--project", path_argument(project), "--ai-port", "0", "--exec", path_argument(commands),
            });
            CHECK_MESSAGE(result.exitCode == 0, result.standardOutput << result.standardError);
            CHECK(result.standardError.find(ENGINE_RUNNING) != std::string::npos);
            CHECK(std::filesystem::is_directory(project / ".conjure"));
        }
    }

    TEST_CASE(
      "regression - code sweep F659 editor_process a broken startup scene is the first output line and the run "
      "still exits 0 [process]"
    ) {
        const std::string executable = CONJURE_EDITOR_EXECUTABLE;
        REQUIRE_MESSAGE(!executable.empty(), "the conjure_editor executable path was not configured");
        testing::temp_directory directory = {};
        const std::filesystem::path project = directory.path() / "project";
        write_project(project);
        write_text(
          project / "project.json",
          "{\n  \"schema_version\": 1,\n  \"name\": \"process_test\",\n  \"assets_root\": \"assets\",\n"
          "  \"startup_scene\": \"scenes/main.scene.json\"\n}\n"
        );
        std::error_code ignored = {};
        std::filesystem::create_directories(project / "assets" / "scenes", ignored);
        // A scene file that does not parse: the import fails and the editor starts over an empty world.
        write_text(project / "assets" / "scenes" / "main.scene.json", "{\"version\": 1, \"entities\": [");
        const std::filesystem::path commands = directory.path() / "state.json";
        write_text(commands, R"([{"cmd":"editor.state"},{"cmd":"scene.new","args":{"name":"fresh"}}])");
        const core::process_result result = run_editor({
          "--headless", "--project", path_argument(project), "--exec", path_argument(commands), "--ai-port", "0",
          "--log-level", "warn",
        });
        CHECK_MESSAGE(result.exitCode == 0, result.standardOutput << result.standardError);
        const std::vector<std::string> lines = editor_test::non_empty_lines(result.standardOutput);
        REQUIRE(lines.size() >= 3);
        const core::result<protocol_event> warning = parse_event(lines.front());
        REQUIRE_MESSAGE(warning.has_value(), lines.front());
        CHECK(warning.value().name == "warning");
        CHECK(warning.value().data.root()["code"].as_string() == "STARTUP_SCENE_FAILED");
        CHECK(warning.value().data.root()["message"].as_string().find("scenes/main.scene.json") != std::string::npos);
        usize okResponses = 0;
        for (const std::string &line : lines) {
            const core::result<protocol_response> response = parse_response(line);
            if (response.has_value() && response.value().ok) {
                ++okResponses;
            }
        }
        CHECK(okResponses == 2);
        const std::string stray = non_protocol_lines(result.standardOutput);
        CHECK_MESSAGE(stray.empty(), stray);
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
