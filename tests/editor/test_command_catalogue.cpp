#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/string_utils.h"
#include "conjure/editor/builtin_commands.h"
#include "conjure/editor/command_registry.h"
#include "conjure/editor/editor_command.h"
#include "conjure/editor/play_controller.h"
#include "conjure/editor/protocol/access_token.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/editor/protocol/protocol_server.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

// The command catalogue (docs/design/04_editor.md 10, 08_testing.md 6.3 item 2, D8c): one fixture per command
// under tests/editor/data/commands/ runs through the dispatcher; every undoable mutation must change the
// snapshot, then is undone and redone, and the serialised scene and the project files must come back exactly.
// The same fixtures hold the read_only flag to its promise (D25): a command an inspect-only client may run
// leaves the editor as it found it, and such a client is refused every other command.

using namespace conjure;
using namespace conjure::editor;

namespace {
    struct expectation {
        bool ok = true;
        core::json::document result = core::json::document::object();
        std::string errorCode = "";
    };

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

    expectation parse_expectation(const core::json::value &value, const std::filesystem::path &projectRoot) {
        expectation expected = {};
        expected.ok = value["ok"].as_bool(true);
        if (value.contains("result")) {
            // A result that names a project file pins it as ${project}/..., never as a temporary path.
            expected.result = expanded_args(value["result"], projectRoot);
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

    /// <summary>
    /// Why an undoable fixture fails the change check, "" when it passes: unless the fixture says
    /// "expectChange": false, its command must change the snapshot, or the undo round trip would also pass for an
    /// inverse that does nothing.
    /// </summary>
    std::string change_failure(
      const core::json::value &root,
      const editor_test::editor_snapshot &before,
      const editor_test::editor_snapshot &after
    ) {
        if (!root["expectChange"].as_bool(true) || before != after) {
            return "";
        }
        return "an undoable command changed nothing the snapshot sees (\"expectChange\": false and a _comment "
               "when its effect lies outside the snapshot)";
    }

    /// <summary>
    /// The state the editor shares between its clients besides the scene and the project files, as text: the
    /// selection, the camera and view mode, the play state, the scene revision and the size of the history.
    /// </summary>
    std::string shared_state(editor_test::headless_editor &editor) {
        editor_context &context = editor.context();
        std::string text = std::format(
          "play {} revision {} undo {} view {} camera {} selection",
          to_string(context.play().state()),
          context.revision(),
          context.undo().size(),
          context.current_view_mode() == editor_context::view_mode::TWO_D ? "2d" : "3d",
          context.camera().to_json().to_string()
        );
        for (const core::uuid &guid : context.selection().guids()) {
            text += ' ';
            text += guid.to_string();
        }
        return text;
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
        const std::string sharedBefore = shared_state(editor);
        const protocol_response response = editor.send(
          command,
          expanded_args(root["request"]["args"], editor.project_directory())
        );
        if (has_command_flag(*descriptor, command_flags::READ_ONLY)) {
            // The promise of the flag: whatever the command answered, nothing other clients share has changed.
            const editor_test::editor_snapshot after = editor_test::capture_snapshot(editor);
            CHECK_MESSAGE(
              before == after,
              std::format("read_only: {}", editor_test::describe_difference(before, after))
            );
            CHECK(shared_state(editor) == sharedBefore);
        }

        std::string why = "";
        bool matched = false;
        if (root.contains("expectAnyOf")) {
            for (const core::json::value candidate : root["expectAnyOf"]) {
                std::string ignored = "";
                if (matches(response, parse_expectation(candidate, editor.project_directory()), ignored)) {
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
            matched = matches(response, parse_expectation(root["expect"], editor.project_directory()), why);
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
        const std::string unchanged = change_failure(root, before, after);
        CHECK_MESSAGE(unchanged.empty(), unchanged);
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

    /// <summary>
    /// A fixture whose undoable command changes nothing: entity.set_transform to the position the entity already
    /// has succeeds and records an undo entry, yet leaves the snapshot as it was.
    /// </summary>
    editor_test::command_fixture no_op_fixture(bool expectChange) {
        constexpr std::string_view GUID = "5a1c7e3b-2d4f-5c6a-8b9e-0f1a2b3c4d5e";
        editor_test::command_fixture fixture = {};
        fixture.name = expectChange ? "no_op" : "no_op_opted_out";
        fixture.document = core::json::object({
          {"expectChange", expectChange},
          {"setup",
           core::json::array({core::json::object({
             {"cmd", "entity.create"},
             {"args", core::json::object({{"name", "Still"}, {"guid", GUID}})},
           })})},
          {"request",
           core::json::object({
             {"cmd", "entity.set_transform"},
             {"args", core::json::object({{"entity", GUID}, {"position", core::json::array({0, 0, 0})}})},
           })},
          {"expect", core::json::object({{"ok", true}})},
        });
        return fixture;
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

    TEST_CASE("command_catalogue - the commands an inspect-only client may run are these and no others") {
        // Pinned on purpose: flagging one more command read_only opens it to every inspect-only client, and
        // that is a decision to take here, in the open (docs/design/04_editor.md D25).
        const std::set<std::string> expected = {
            "asset.info", "asset.list", "component.get", "component.list", "console.read", "editor.schema",
            "editor.state", "editor.wait_frames", "entity.bounds", "entity.find", "entity.get", "history.list",
            "module.list", "physics.overlap_sphere", "physics.raycast", "play.state", "profiler.frame_stats",
            "project.info", "project.list_scenes", "project.trust_state", "reflection.asset_types",
            "reflection.components", "reflection.type", "scene.get_hierarchy", "scene.info", "scene.query",
            "script.list", "script.read", "selection.get", "session.hello", "session.ping", "session.subscribe",
            "view.get_camera", "view.mode", "view.pick",
        };
        command_registry registry = {};
        register_builtin_commands(registry);
        std::set<std::string> flagged = {};
        for (const command_descriptor *descriptor : registry.descriptors()) {
            CAPTURE(descriptor->name);
            const bool readOnly = has_command_flag(*descriptor, command_flags::READ_ONLY);
            CHECK(readOnly == expected.contains(descriptor->name));
            const access_level expectedAccess = readOnly ? access_level::INSPECT : access_level::FULL_CONTROL;
            CHECK(required_access(descriptor->flags) == expectedAccess);
            if (readOnly) {
                flagged.insert(descriptor->name);
                // What the registry also enforces: no command that mutates is open to inspect-only clients.
                CHECK_FALSE(has_command_flag(*descriptor, command_flags::MUTATES));
            }
        }
        CHECK(flagged.size() == expected.size());
    }

    TEST_CASE("regression - code sweep F187 an inspect-only client is refused every command that is not read_only, "
              "and no command that is") {
        editor_test::headless_editor editor = {};
        const std::string entity = editor.create_entity("Watched");
        protocol_server &server = editor.application().server();
        CONJURE_REQUIRE_OK(server.set_token("tok-7f3a9c"));
        CONJURE_REQUIRE_OK(server.set_inspect_token("look-51d0e2"));
        editor_test::loopback_session session(editor);
        const core::json::document hello = session.ok("session.hello", core::json::object({{"token", "look-51d0e2"}}));
        REQUIRE(hello.root()["access"].as_string() == "inspect");
        const editor_test::editor_snapshot before = editor_test::capture_snapshot(editor);
        const std::string sharedBefore = shared_state(editor);

        usize refused = 0;
        usize allowed = 0;
        for (const command_descriptor *descriptor : editor.context().registry().descriptors()) {
            CAPTURE(descriptor->name);
            // No arguments at all: a command that is refused never gets as far as looking at them, and one that
            // is allowed answers whatever it answers to that (INVALID_ARGS for most). session.hello alone needs
            // its token, or its own handler would refuse the hello.
            core::json::document args = core::json::document::object();
            if (descriptor->name == "session.hello") {
                args.set("token", "look-51d0e2");
            }
            const protocol_response response = session.request(descriptor->name, std::move(args));
            if (has_command_flag(*descriptor, command_flags::READ_ONLY)) {
                CHECK(response.errorCode != protocol_error_code::UNAUTHORISED);
                ++allowed;
                continue;
            }
            CHECK_FALSE(response.ok);
            CHECK(response.errorCode == protocol_error_code::UNAUTHORISED);
            CHECK(response.errorDetails.root()["required"].as_string() == "full_control");
            CHECK(response.errorDetails.root()["granted"].as_string() == "inspect");
            CHECK(response.errorMessage.find(descriptor->name) != std::string::npos);
            ++refused;
        }
        CHECK(allowed == 35);
        CHECK(refused == editor.context().registry().size() - allowed);

        // Nothing happened: editor.quit, play.start, scene.close, entity.destroy_many and the rest never ran.
        const editor_test::editor_snapshot after = editor_test::capture_snapshot(editor);
        CHECK_MESSAGE(before == after, editor_test::describe_difference(before, after));
        CHECK(shared_state(editor) == sharedBefore);
        CHECK(editor.context().dispatcher().pending_count() == 0);
        CHECK_FALSE(editor.application().is_quit_requested());
        // A refusal does not cost the client its connection, and the other token opens the same commands.
        CHECK(session.client().is_connected());
        const core::json::document raised = session.ok("session.hello", core::json::object({{"token", "tok-7f3a9c"}}));
        CHECK(raised.root()["access"].as_string() == "full_control");
        (void) session.ok("entity.rename", core::json::object({{"entity", entity}, {"name", "Renamed"}}));
        CHECK(editor.scene().world().entity_count() == 1);
        CHECK_FALSE(editor.log().contains("tok-7f3a9c"));
        CHECK_FALSE(editor.log().contains("look-51d0e2"));
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

    TEST_CASE("regression - F703 command_catalogue an undoable command that changes nothing fails the change check") {
        editor_test::headless_editor editor = {};
        const std::string entity = editor.create_entity("Still");
        const core::json::document plain = core::json::document::object();
        const core::json::document optedOut = core::json::object({{"expectChange", false}});

        // The position it already has: the command succeeds and records an undo entry, yet changes nothing.
        const editor_test::editor_snapshot before = editor_test::capture_snapshot(editor);
        (void) editor.ok(
          "entity.set_transform",
          core::json::object({{"entity", entity}, {"position", core::json::array({0.0f, 0.0f, 0.0f})}})
        );
        const editor_test::editor_snapshot unchanged = editor_test::capture_snapshot(editor);
        CHECK(before == unchanged);
        CHECK_FALSE(change_failure(plain.root(), before, unchanged).empty());
        CHECK(change_failure(optedOut.root(), before, unchanged).empty());

        (void) editor.ok(
          "entity.set_transform",
          core::json::object({{"entity", entity}, {"position", core::json::array({1.0f, 0.0f, 0.0f})}})
        );
        const editor_test::editor_snapshot moved = editor_test::capture_snapshot(editor);
        CHECK(change_failure(plain.root(), before, moved).empty());
    }

    // The two cases below prove run_fixture applies the change check: the same no-op fixture passes when it opts
    // out, so the only failure left once it does not is the change check itself.
    TEST_CASE("regression - F703 run_fixture passes the no-op fixture that says expectChange false") {
        run_fixture(no_op_fixture(false));
    }

    TEST_CASE(
      "regression - F703 run_fixture fails an undoable fixture whose command changes nothing" * doctest::should_fail()
    ) {
        run_fixture(no_op_fixture(true));
    }
}
