#include "conjure/testing/doctest.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/builtin_commands.h"
#include "conjure/editor/command_registry.h"

#include <array>
#include <string_view>

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

    TEST_CASE("builtin_command_count - matches a fresh registration and is idempotent") {
        CHECK(builtin_command_count() == builtin_command_count());
        command_registry registry = {};
        register_builtin_commands(registry);
        CHECK(registry.size() == builtin_command_count());
    }
}
