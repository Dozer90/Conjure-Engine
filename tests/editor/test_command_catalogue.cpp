#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/string_utils.h"
#include "conjure/editor/builtin_commands.h"
#include "conjure/editor/command_registry.h"
#include "conjure/editor/protocol/protocol_message.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <set>
#include <string>
#include <vector>

// The command catalogue (docs/design/04_editor.md 10, 08_testing.md 6.3 item 2, D8c): one fixture per command
// under tests/editor/data/commands/ runs through the dispatcher; every undoable mutation is undone and redone
// and the world hash, the serialised scene and the project files must come back exactly.

using namespace conjure;
using namespace conjure::editor;

namespace {
    struct expectation {
        bool ok = true;
        core::json::document result = core::json::document::object();
        std::string errorCode = "";
    };

    expectation parse_expectation(const core::json::value &value) {
        expectation expected = {};
        expected.ok = value["ok"].as_bool(true);
        if (value.contains("result")) {
            expected.result = core::json::document::from_value(value["result"]);
        }
        if (value.contains("error")) {
            expected.errorCode = std::string(value["error"]["code"].as_string());
        }
        return expected;
    }

    bool matches(const protocol_response &response, const expectation &expected, std::string &why) {
        if (response.ok != expected.ok) {
            why = std::format(
              "expected ok={} but got ok={} ({} {})",
              expected.ok,
              response.ok,
              to_string(response.errorCode),
              response.errorMessage
            );
            return false;
        }
        if (response.ok) {
            if (!editor_test::json_contains(response.result.root(), expected.result.root())) {
                why = std::format("result {} lacks {}", response.result.to_string(), expected.result.to_string());
                return false;
            }
            return true;
        }
        if (!expected.errorCode.empty() && to_string(response.errorCode) != expected.errorCode) {
            why = std::format(
              "expected error {} but got {} ({})",
              expected.errorCode,
              to_string(response.errorCode),
              response.errorMessage
            );
            return false;
        }
        return true;
    }

    void write_fixture_files(const editor_test::command_fixture &fixture, const std::filesystem::path &projectRoot) {
        const core::json::value files = fixture.document.root()["files"];
        if (!files.is_object()) {
            return;
        }
        for (const core::json::member entry : files.members()) {
            const std::filesystem::path path = projectRoot / std::filesystem::path(std::string(entry.key));
            std::error_code ignored = {};
            std::filesystem::create_directories(path.parent_path(), ignored);
            const std::string_view content = entry.item.as_string("");
            if (content == "@tiny_png") {
                (void) editor_test::write_tiny_png(path);
                continue;
            }
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            REQUIRE_MESSAGE(output.is_open(), path.generic_string());
            const std::string expanded = editor_test::expand_project(content, projectRoot);
            output.write(expanded.data(), static_cast<std::streamsize>(expanded.size()));
        }
    }

    core::json::document expanded_args(const core::json::value &args, const std::filesystem::path &projectRoot) {
        core::json::document document = args.is_object() ? core::json::document::from_value(
          args
        ) : core::json::document::object();
        std::string text = editor_test::expand_project(document.to_string(), projectRoot);
        text = core::str::replace_all(text, "@tiny_png_base64", editor_test::tiny_png_base64());
        core::result<core::json::document> expanded = core::json::document::parse(text);
        REQUIRE_MESSAGE(expanded.has_value(), testing::describe_error(expanded));
        return std::move(expanded).value();
    }

    void run_fixture(const editor_test::command_fixture &fixture) {
        INFO("fixture ", fixture.name);
        const core::json::value root = fixture.document.root();
        const std::string command = std::string(root["request"]["cmd"].as_string());
        REQUIRE_MESSAGE(!command.empty(), "a fixture names its command");
        editor_test::headless_editor_options options = {};
        options.withScene = root["editor"]["withScene"].as_bool(true);
        options.withProject = root["editor"]["withProject"].as_bool(true);
        editor_test::headless_editor editor(options);
        if (root["editor"]["allowErrors"].as_bool(false)) {
            editor.log().allow_errors();
        }
        write_fixture_files(fixture, editor.project_directory());
        for (const core::json::value step : root["setup"]) {
            const protocol_response response = editor.send(
              step["cmd"].as_string(),
              expanded_args(step["args"], editor.project_directory())
            );
            REQUIRE_MESSAGE(
              response.ok,
              std::format(
                "setup '{}' failed: {} {}",
                step["cmd"].as_string(),
                to_string(response.errorCode),
                response.errorMessage
              )
            );
        }

        const command_descriptor *descriptor = editor.context().registry().find(command);
        REQUIRE_MESSAGE(descriptor != nullptr, std::format("'{}' is not a registered command", command));
        const bool undoable = has_command_flag(
          *descriptor,
          command_flags::MUTATES
        ) && !has_command_flag(*descriptor, command_flags::NO_UNDO) &&
          root["roundTrip"].as_bool(true);
        const editor_test::editor_snapshot before = editor_test::capture_snapshot(editor);
        const usize undoBefore = editor.context().undo().size();
        const protocol_response response = editor.send(
          command,
          expanded_args(root["request"]["args"], editor.project_directory())
        );

        std::string why = "";
        bool matched = false;
        if (root.contains("expectAnyOf")) {
            for (const core::json::value candidate : root["expectAnyOf"]) {
                std::string ignored = "";
                if (matches(response, parse_expectation(candidate), ignored)) {
                    matched = true;
                    break;
                }
            }
            why = std::format(
              "no expectation matched: ok={} {} {}",
              response.ok,
              to_string(response.errorCode),
              response.errorMessage
            );
        } else {
            matched = matches(response, parse_expectation(root["expect"]), why);
        }
        CHECK_MESSAGE(matched, why);
        if (!response.ok || !undoable) {
            if (!response.ok) {
                // A failed mutation leaves nothing behind.
                const editor_test::editor_snapshot after = editor_test::capture_snapshot(editor);
                CHECK_MESSAGE(before == after, editor_test::describe_difference(before, after));
                CHECK(editor.context().undo().size() == undoBefore);
            }
            return;
        }
        REQUIRE_MESSAGE(
          editor.context().undo().size() == undoBefore + 1,
          "a mutating command records exactly one undo entry"
        );
        const editor_test::editor_snapshot after = editor_test::capture_snapshot(editor);
        (void) editor.ok("history.undo");
        const editor_test::editor_snapshot undone = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(before == undone, std::format("undo: {}", editor_test::describe_difference(before, undone)));
        (void) editor.ok("history.redo");
        const editor_test::editor_snapshot redone = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(after == redone, std::format("redo: {}", editor_test::describe_difference(after, redone)));
        (void) editor.ok("history.undo");
        const editor_test::editor_snapshot undoneAgain = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(
          before == undoneAgain,
          std::format("second undo: {}", editor_test::describe_difference(before, undoneAgain))
        );
    }
}

TEST_SUITE("editor/command_catalogue") {

    TEST_CASE("command_catalogue - every registered command has at least one fixture") {
        const std::vector<editor_test::command_fixture> fixtures = editor_test::load_command_fixtures();
        std::set<std::string> covered = {};
        for (const editor_test::command_fixture &fixture : fixtures) {
            covered.insert(std::string(fixture.document.root()["request"]["cmd"].as_string()));
        }
        command_registry registry = {};
        register_builtin_commands(registry);
        for (const command_descriptor *descriptor : registry.descriptors()) {
            CHECK_MESSAGE(covered.contains(descriptor->name), std::format("no fixture for '{}'", descriptor->name));
        }
        for (const std::string &name : covered) {
            CHECK_MESSAGE(registry.find(name) != nullptr, std::format("fixture names unknown command '{}'", name));
        }
    }

    TEST_CASE(
      "command_catalogue - every fixture answers as expected and every mutation round trips through undo and redo"
    ) {
        const std::vector<editor_test::command_fixture> fixtures = editor_test::load_command_fixtures();
        REQUIRE_FALSE(fixtures.empty());
        for (const editor_test::command_fixture &fixture : fixtures) {
            SUBCASE(fixture.name.c_str()) {
                run_fixture(fixture);
            }
        }
    }
}
