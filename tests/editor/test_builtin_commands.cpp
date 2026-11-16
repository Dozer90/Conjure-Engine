#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/log.h"
#include "conjure/core/result.h"
#include "conjure/ecs/scheduler.h"
#include "conjure/ecs/system_phase.h"
#include "conjure/editor/builtin_commands.h"
#include "conjure/editor/command_registry.h"
#include "conjure/editor/console_buffer.h"
#include "conjure/editor/editor_command.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/event_broadcaster.h"
#include "conjure/engine/engine_context.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

// builtin_commands.h: the catalogue of docs/design/04_editor.md 4.4 is registered completely and consistently.

using namespace conjure;
using namespace conjure::editor;

namespace {
    constexpr std::string_view EXPECTED_COMMANDS[] = {
        "session.hello", "session.subscribe", "session.ping",
        "editor.schema", "editor.state", "editor.batch", "editor.begin_transaction", "editor.end_transaction",
        "editor.wait_frames", "editor.exec_file", "editor.quit",
        "history.undo", "history.redo", "history.list", "history.clear",
        "console.read", "console.clear",
        "project.create", "project.open", "project.save", "project.info", "project.list_scenes", "project.set_settings",
        "project.export",
        "scene.new", "scene.open", "scene.save", "scene.save_as", "scene.close", "scene.info", "scene.query",
        "scene.get_hierarchy", "scene.set_settings",
        "entity.create", "entity.create_many", "entity.destroy", "entity.destroy_many", "entity.restore",
        "entity.duplicate", "entity.rename", "entity.set_parent", "entity.set_enabled", "entity.set_transform",
        "entity.get", "entity.find", "entity.bounds",
        "component.add", "component.remove", "component.set", "component.set_many", "component.get", "component.list",
        "asset.import", "asset.list", "asset.info", "asset.create_folder", "asset.delete", "asset.reimport",
        "asset.create_material", "asset.set", "asset.restore",
        "prefab.create", "prefab.instantiate", "prefab.apply", "prefab.unpack",
        "script.create", "script.read", "script.write", "script.delete", "script.list", "script.compile",
        "script.reload", "script.gc", "script.attach",
        "play.start", "play.stop", "play.pause", "play.resume", "play.step", "play.state",
        "input.inject", "input.clear",
        "view.screenshot", "view.set_camera", "view.get_camera", "view.focus", "view.pick", "view.mode",
        "view.set_mode", "view.debug_draw",
        "physics.raycast", "physics.overlap_sphere",
        "selection.get", "selection.set", "selection.clear",
        "reflection.components", "reflection.type", "reflection.asset_types",
        "module.list", "module.discover", "module.load", "module.unload", "module.reload", "module.build",
        "profiler.frame_stats",
    };
}

TEST_SUITE("editor/builtin_commands") {

    TEST_CASE("register_builtin_commands - every command of the catalogue is registered") {
        command_registry registry = {};
        register_builtin_commands(registry);
        usize expected = 0;
        for (const std::string_view name : EXPECTED_COMMANDS) {
            ++expected;
            CHECK_MESSAGE(registry.find(name) != nullptr, name);
        }
        CHECK(registry.size() == expected);
        CHECK(builtin_command_count() == expected);
    }

    TEST_CASE("register_builtin_commands - every command has a summary, a compiled schema and consistent flags") {
        command_registry registry = {};
        register_builtin_commands(registry);
        for (const command_descriptor *descriptor : registry.descriptors()) {
            CHECK_MESSAGE(!descriptor->summary.empty(), descriptor->name);
            CHECK_MESSAGE(registry.args_schema(descriptor->name) != nullptr, descriptor->name);
            CHECK_MESSAGE(descriptor->owner == "editor", descriptor->name);
            CHECK_MESSAGE(static_cast<bool>(descriptor->handler), descriptor->name);
            if (has_command_flag(*descriptor, command_flags::ASYNC)) {
                CHECK_MESSAGE(has_command_flag(*descriptor, command_flags::NOT_IN_BATCH), descriptor->name);
            }
            if (has_command_flag(*descriptor, command_flags::NO_UNDO)) {
                CHECK_MESSAGE(has_command_flag(*descriptor, command_flags::MUTATES), descriptor->name);
            }
            if (has_command_flag(*descriptor, command_flags::ON_DISK)) {
                CHECK_MESSAGE(has_command_flag(*descriptor, command_flags::MUTATES), descriptor->name);
                CHECK_MESSAGE(!has_command_flag(*descriptor, command_flags::NO_UNDO), descriptor->name);
            }
            CHECK_MESSAGE(
              !(has_command_flag(
                *descriptor,
                command_flags::EDITING_ONLY
              ) && has_command_flag(*descriptor, command_flags::PLAYING_ONLY)),
              descriptor->name
            );
            const core::json::document description = registry.describe(*descriptor);
            CHECK(description.root()["args"].is_object());
            CHECK(description.root()["result"].is_object());
        }
    }

    TEST_CASE("register_builtin_commands - the asynchronous commands are exactly the ones the design lists") {
        command_registry registry = {};
        register_builtin_commands(registry);
        const std::array<std::string_view, 7> asynchronous = {
            "editor.wait_frames", "editor.exec_file", "project.export", "script.compile", "play.step",
            "view.screenshot",
            "module.build",
        };
        for (const command_descriptor *descriptor : registry.descriptors()) {
            bool expected = false;
            for (const std::string_view name : asynchronous) {
                expected = expected || name == descriptor->name;
            }
            CHECK_MESSAGE(has_command_flag(*descriptor, command_flags::ASYNC) == expected, descriptor->name);
        }
    }

    TEST_CASE("regression - code sweep F089 the on_disk commands are exactly the undoable file edits") {
        command_registry registry = {};
        register_builtin_commands(registry);
        // Every undoable command whose effect lives in the project's files and whose inverse and replay touch no
        // scene state. prefab.create is not one: its inverse unpacks an entity (it is editing-only instead).
        const std::array<std::string_view, 11> onDisk = {
            "asset.import", "asset.create_folder", "asset.delete", "asset.create_material", "asset.set",
            "asset.restore", "prefab.apply", "project.set_settings", "script.create", "script.write",
            "script.delete",
        };
        for (const command_descriptor *descriptor : registry.descriptors()) {
            bool expected = false;
            for (const std::string_view name : onDisk) {
                expected = expected || name == descriptor->name;
            }
            CHECK_MESSAGE(has_command_flag(*descriptor, command_flags::ON_DISK) == expected, descriptor->name);
            if (expected) {
                // Usable while playing: that is where the flag matters.
                CHECK_MESSAGE(!has_command_flag(*descriptor, command_flags::EDITING_ONLY), descriptor->name);
            }
        }
        for (const std::string_view name : onDisk) {
            CHECK_MESSAGE(registry.find(name) != nullptr, name);
        }
    }

    TEST_CASE("regression - code sweep F089 prefab.create, scene.save and scene.save_as are editing-state commands") {
        command_registry registry = {};
        register_builtin_commands(registry);
        const std::array<std::string_view, 3> editingOnly = {"prefab.create", "scene.save", "scene.save_as"};
        for (const std::string_view name : editingOnly) {
            const command_descriptor *descriptor = registry.find(name);
            REQUIRE_MESSAGE(descriptor != nullptr, name);
            CHECK_MESSAGE(has_command_flag(*descriptor, command_flags::EDITING_ONLY), name);
            CHECK_MESSAGE(!has_command_flag(*descriptor, command_flags::ON_DISK), name);
        }
    }

    TEST_CASE("builtin_command_count - matches a fresh registration and is idempotent") {
        CHECK(builtin_command_count() == builtin_command_count());
        command_registry registry = {};
        register_builtin_commands(registry);
        CHECK(registry.size() == builtin_command_count());
    }

    TEST_CASE("console.read - minLevel filters by level, and off or an unknown name is INVALID_ARGS") {
        editor_test::headless_editor editor = {};
        const u32 client = editor.client_id();
        console_buffer &console = editor.context().console();
        console.push(core::log_level::INFO, "f209", "info line");
        console.push(core::log_level::WARN, "f209", "warn line");
        console.push(core::log_level::FATAL, "f209", "fatal line");

        const core::json::document all =
          editor.ok("console.read", core::json::object({{"minLevel", "trace"}, {"category", "f209"}}));
        CHECK(all.root()["entries"].size() == 3);
        const core::json::document warnings =
          editor.ok("console.read", core::json::object({{"minLevel", "warn"}, {"category", "f209"}}));
        REQUIRE(warnings.root()["entries"].size() == 2);
        CHECK(warnings.root()["entries"][0]["message"].as_string() == "warn line");
        CHECK(warnings.root()["entries"][0]["level"].as_string() == "WARN");
        const core::json::document fatal =
          editor.ok("console.read", core::json::object({{"minLevel", "fatal"}, {"category", "f209"}}));
        REQUIRE(fatal.root()["entries"].size() == 1);
        CHECK(fatal.root()["entries"][0]["message"].as_string() == "fatal line");

        // OFF is a level no line has, so it is not a filter: refused like a name that is no level at all.
        constexpr std::array<std::string_view, 4> REFUSED = {"off", "OFF", "loud", ""};
        for (const std::string_view name : REFUSED) {
            CAPTURE(name);
            CHECK(
              editor.fail("console.read", core::json::object({{"minLevel", name}})) == protocol_error_code::INVALID_ARGS
            );
        }

        // The handler refuses them itself, whatever the schema in front of it does: a caller that reaches it
        // without the dispatcher gets the same answer.
        const command_descriptor *descriptor = editor.context().registry().find("console.read");
        REQUIRE(descriptor != nullptr);
        for (const std::string_view name : REFUSED) {
            CAPTURE(name);
            const core::json::document args = core::json::object({{"minLevel", name}});
            const core::json::value root = args.root();
            command_invocation invocation{editor.context(), root, client, {}, core::json::document::object()};
            const core::result<command_outcome> outcome = descriptor->handler(invocation);
            CONJURE_CHECK_ERROR(outcome, protocol_error_code::INVALID_ARGS);
            if (!outcome.has_value()) {
                CHECK(outcome.get_error().message().find("args.minLevel") != std::string::npos);
            }
        }

        // The handler reads the name case-insensitively, as parse_log_level does.
        const core::json::document upperArgs = core::json::object({{"minLevel", "FATAL"}, {"category", "f209"}});
        const core::json::value upperRoot = upperArgs.root();
        command_invocation upper{editor.context(), upperRoot, client, {}, core::json::document::object()};
        const core::result<command_outcome> upperOutcome = descriptor->handler(upper);
        CONJURE_REQUIRE_OK(upperOutcome);
        CHECK(upperOutcome.value().result.root()["entries"].size() == 1);
    }

    TEST_CASE("session.subscribe - minLogLevel sets the client's level, and off or an unknown name is INVALID_ARGS") {
        editor_test::headless_editor editor = {};
        event_broadcaster &events = editor.context().events();
        const u32 client = editor.client_id();

        (void) editor.ok(
          "session.subscribe",
          core::json::object({{"events", core::json::array({"log"})}, {"minLogLevel", "error"}})
        );
        CHECK(events.min_log_level(client) == core::log_level::ERR);
        const u64 subscribed = events.subscriptions(client);
        CHECK((subscribed & event_bit(editor_event::LOG)) != 0);

        // OFF is no level of a log event: refused like an unknown name, and nothing of the request is applied.
        constexpr std::array<std::string_view, 4> REFUSED = {"off", "OFF", "loud", ""};
        for (const std::string_view name : REFUSED) {
            CAPTURE(name);
            CHECK(editor.fail(
              "session.subscribe",
              core::json::object({{"events", core::json::document::array()}, {"minLogLevel", name}})
            ) == protocol_error_code::INVALID_ARGS);
        }

        const command_descriptor *descriptor = editor.context().registry().find("session.subscribe");
        REQUIRE(descriptor != nullptr);
        for (const std::string_view name : REFUSED) {
            CAPTURE(name);
            const core::json::document args =
              core::json::object({{"events", core::json::document::array()}, {"minLogLevel", name}});
            const core::json::value root = args.root();
            command_invocation invocation{editor.context(), root, client, {}, core::json::document::object()};
            const core::result<command_outcome> outcome = descriptor->handler(invocation);
            CONJURE_CHECK_ERROR(outcome, protocol_error_code::INVALID_ARGS);
            if (!outcome.has_value()) {
                CHECK(outcome.get_error().message().find("args.minLogLevel") != std::string::npos);
            }
        }
        CHECK(events.min_log_level(client) == core::log_level::ERR);
        CHECK(events.subscriptions(client) == subscribed);

        // The handler reads the name case-insensitively, as parse_log_level does.
        const core::json::document upperArgs =
          core::json::object({{"events", core::json::array({"log"})}, {"minLogLevel", "FATAL"}});
        const core::json::value upperRoot = upperArgs.root();
        command_invocation upper{editor.context(), upperRoot, client, {}, core::json::document::object()};
        CONJURE_REQUIRE_OK(descriptor->handler(upper));
        CHECK(events.min_log_level(client) == core::log_level::FATAL);
    }

    TEST_CASE("profiler.frame_stats - systems lists the scheduler's systems and phases the per-phase times, gpuMs null "
      "on the null device") {
        editor_test::headless_editor editor = {};
        editor.pump(2);
        const core::json::document stats = editor.ok("profiler.frame_stats");
        const core::json::value root = stats.root();
        CHECK(root["gpuMs"].is_null());

        const core::json::value phases = root["phases"];
        REQUIRE(phases.size() == ecs::SYSTEM_PHASE_COUNT);
        for (u32 phase = 0; phase < ecs::SYSTEM_PHASE_COUNT; ++phase) {
            CAPTURE(phase);
            CHECK(phases[phase]["name"].as_string() == ecs::to_string(static_cast<ecs::system_phase>(phase)));
            CHECK(phases[phase]["ms"].is_number());
            CHECK(phases[phase]["averageMs"].is_number());
        }

        const ecs::scheduler *scheduler = editor.context().engine_context().scheduler;
        REQUIRE(scheduler != nullptr);
        std::vector<std::string> expected = {};
        for (u32 phase = 0; phase < ecs::SYSTEM_PHASE_COUNT; ++phase) {
            for (const ecs::system_timing &timing : scheduler->last_timings(static_cast<ecs::system_phase>(phase))) {
                expected.push_back(timing.name.to_string());
            }
        }
        REQUIRE_FALSE(expected.empty());
        const core::json::value systems = root["systems"];
        REQUIRE(systems.size() == expected.size());
        for (usize index = 0; index < expected.size(); ++index) {
            CAPTURE(index);
            CHECK(systems[index]["name"].as_string() == expected[index]);
            CHECK_FALSE(systems[index]["phase"].as_string().empty());
            CHECK(systems[index]["ms"].as_f64(-1.0) >= 0.0);
        }
    }
}
