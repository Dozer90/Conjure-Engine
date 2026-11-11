#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/command_desc.h"
#include "conjure/editor/command_registry.h"
#include "conjure/editor/editor_module_consumer.h"
#include "conjure/editor/panel_desc.h"
#include "conjure/modules/module_context.h"
#include "conjure/modules/module_error.h"
#include "conjure/modules/module_manager.h"
#include "conjure/modules/module_registration_set.h"
#include "conjure/modules/module_registry.h"
#include "conjure/modules/module_version.h"

#include <cstring>
#include <string>

// editor_module_consumer.h (docs/design/04_editor.md R14, 07_modules.md): modules contribute commands and
// panels through the editor.command and editor.panel extension points.

using namespace conjure;
using namespace conjure::editor;

namespace {
    core::result<core::json::document> module_handler(const core::json::value &args, void *userData) {
        (void) userData;
        return core::json::object({{"echo", args["x"]}});
    }

    void module_draw(void *imguiContext, void *userData) {
        (void) imguiContext;
        (void) userData;
    }

    template <typename DescType>
    void add_extension(modules::module_registration_set &set, const DescType &desc) {
        const usize offset = (
          set.extensionStorage.size() + alignof(DescType) - 1
        ) / alignof(DescType) * alignof(DescType);
        set.extensionStorage.resize(offset + sizeof(DescType));
        std::memcpy(set.extensionStorage.data() + offset, &desc, sizeof(DescType));
        modules::extension_record record = {};
        record.point = DescType::EXTENSION_POINT;
        record.offset = static_cast<u32>(offset);
        record.size = static_cast<u32>(sizeof(DescType));
        record.alignment = static_cast<u32>(alignof(DescType));
        set.extensions.push_back(record);
    }

    command_desc make_command(const char *name) {
        command_desc desc = {};
        desc.name = name;
        desc.summary = "a module command";
        desc.argsSchema = R"({"type":"object","properties":{"x":{"type":"integer"}}})";
        desc.flags = command_flags::NONE;
        desc.handler = &module_handler;
        return desc;
    }

    /// <summary>
    /// The engine's first registered (built-in) module stands in for a plugin: it has a manifest and a context
    /// can be built for it.
    /// </summary>
    modules::module_id any_module(editor_test::headless_editor &editor) {
        const std::span<const modules::module_id> all = editor.context().modules()->registry().all();
        REQUIRE_FALSE(all.empty());
        return all.front();
    }
}

TEST_SUITE("editor/editor_module_consumer") {

    TEST_CASE("editor_module_consumer - is installed into the engine's module manager and named") {
        editor_test::headless_editor editor = {};
        editor_module_consumer &consumer = editor.context().module_consumer();
        CHECK(consumer.name() == core::string_id("editor"));
        CHECK(consumer.attached().size() == editor.context().modules()->registry().load_order().size());
        CHECK(consumer.panels().empty());
        CHECK(consumer.live_use_count(any_module(editor)) == 0);
    }

    TEST_CASE("editor_module_consumer - validate rejects a command name that is already registered") {
        editor_test::headless_editor editor = {};
        command_registry registry = {};
        CONJURE_REQUIRE_OK(registry.add([]() {
            command_descriptor descriptor = {};
            descriptor.name = "entity.create";
            descriptor.summary = "taken";
            descriptor.handler = [](command_invocation &) -> core::result<command_outcome> {
                return command_outcome{};
            };
            return descriptor;
        }()));
        editor_module_consumer consumer(registry, &editor.context().play());
        modules::module_registration_set conflicting = {};
        add_extension(conflicting, make_command("entity.create"));
        const modules::module_id id = any_module(editor);
        CONJURE_CHECK_ERROR(
          consumer.validate(id, conflicting, editor.context().modules()->registry()),
          modules::module_error::REGISTRATION_CONFLICT
        );
        modules::module_registration_set badSchema = {};
        command_desc bad = make_command("plugin.bad");
        bad.argsSchema = "{not json";
        add_extension(badSchema, bad);
        CONJURE_CHECK_ERROR(
          consumer.validate(id, badSchema, editor.context().modules()->registry()),
          modules::module_error::REGISTRATION_CONFLICT
        );
        modules::module_registration_set fine = {};
        add_extension(fine, make_command("plugin.fine"));
        CONJURE_CHECK_OK(consumer.validate(id, fine, editor.context().modules()->registry()));
    }

    TEST_CASE(
      "editor_module_consumer - attach registers commands and panels owned by the module, detach removes them"
    ) {
        editor_test::headless_editor editor = {};
        command_registry registry = {};
        editor_module_consumer consumer(registry, &editor.context().play());
        int panelChanges = 0;
        core::event_subscription token = consumer.on_panels_changed().subscribe([&panelChanges]() {
            ++panelChanges;
        });
        const modules::module_id id = any_module(editor);
        const modules::module_registry &moduleRegistry = editor.context().modules()->registry();
        modules::module_context context(
          id,
          modules::module_host_kind::EDITOR,
          modules::host_engine_version(),
          moduleRegistry.manifest(id),
          moduleRegistry.root_directory(id),
          editor.context().modules()->services()
        );
        modules::module_registration_set set = {};
        add_extension(set, make_command("plugin.echo"));
        panel_desc panel = {};
        panel.title = "Plugin Panel";
        panel.draw = &module_draw;
        add_extension(set, panel);
        consumer.attach(id, context, set);
        REQUIRE(registry.find("plugin.echo") != nullptr);
        CHECK(registry.find("plugin.echo")->owner == moduleRegistry.manifest(id).name);
        REQUIRE(consumer.panels().size() == 1);
        CHECK(consumer.panels()[0].title == "Plugin Panel");
        CHECK(consumer.panels()[0].owner == moduleRegistry.manifest(id).name);
        CHECK(consumer.attached().size() == 1);
        CHECK(panelChanges == 1);

        // The adapted handler answers through the C function.
        command_request request = {};
        request.name = "plugin.echo";
        request.args = core::json::object({{"x", 4}});
        editor_context &editorContext = editor.context();
        command_invocation invocation{editorContext, request.args.root(), 0, {}, core::json::document::object()};
        const core::result<command_outcome> outcome = registry.find("plugin.echo")->handler(invocation);
        CONJURE_REQUIRE_OK(outcome);
        CHECK(outcome.value().result.root()["echo"].as_i64() == 4);

        CONJURE_CHECK_OK(consumer.can_detach(id, modules::unload_policy::REFUSE_IF_IN_USE));
        consumer.detach(id, nullptr);
        CHECK(registry.find("plugin.echo") == nullptr);
        CHECK(consumer.panels().empty());
        CHECK(consumer.attached().empty());
        CHECK(panelChanges == 2);
        token.release();
    }

    TEST_CASE("regression - a module MUTATES command runs through the dispatcher as NO_UNDO without asserting") {
        editor_test::headless_editor editor = {};
        // A consumer over the editor's own registry, so the command is reachable through the dispatcher.
        editor_module_consumer consumer(editor.context().registry(), &editor.context().play());
        const modules::module_id id = any_module(editor);
        const modules::module_registry &moduleRegistry = editor.context().modules()->registry();
        modules::module_context context(
          id,
          modules::module_host_kind::EDITOR,
          modules::host_engine_version(),
          moduleRegistry.manifest(id),
          moduleRegistry.root_directory(id),
          editor.context().modules()->services()
        );
        modules::module_registration_set set = {};
        command_desc mutating = make_command("plugin.mutate");
        mutating.flags = command_flags::MUTATES;
        add_extension(set, mutating);
        CONJURE_REQUIRE_OK(consumer.validate(id, set, moduleRegistry));
        consumer.attach(id, context, set);
        const command_descriptor *descriptor = editor.context().registry().find("plugin.mutate");
        REQUIRE(descriptor != nullptr);
        CHECK(has_command_flag(*descriptor, command_flags::MUTATES));
        CHECK(has_command_flag(*descriptor, command_flags::NO_UNDO));

        const u64 revision = editor.context().revision();
        const usize entries = editor.context().undo().size();
        const core::json::document result = editor.ok("plugin.mutate", core::json::object({{"x", 7}}));
        CHECK(result.root()["echo"].as_i64() == 7);
        CHECK(result.root()["revision"].as_u64() == revision + 1);
        CHECK(editor.context().undo().size() == entries);
        consumer.detach(id, nullptr);
        CHECK(editor.context().registry().find("plugin.mutate") == nullptr);
    }

    TEST_CASE("editor_module_consumer - can_detach is UNLOAD_BLOCKED while playing") {
        editor_test::headless_editor editor = {};
        editor_module_consumer &consumer = editor.context().module_consumer();
        const modules::module_id id = any_module(editor);
        CONJURE_CHECK_OK(consumer.can_detach(id, modules::unload_policy::REFUSE_IF_IN_USE));
        (void) editor.ok("play.start");
        CONJURE_CHECK_ERROR(
          consumer.can_detach(id, modules::unload_policy::REFUSE_IF_IN_USE),
          modules::module_error::UNLOAD_BLOCKED
        );
        CONJURE_CHECK_ERROR(
          consumer.can_detach(id, modules::unload_policy::REMOVE_INSTANCES),
          modules::module_error::UNLOAD_BLOCKED
        );
        (void) editor.ok("play.stop");
        CONJURE_CHECK_OK(consumer.can_detach(id, modules::unload_policy::REFUSE_IF_IN_USE));
    }

    TEST_CASE("module.list - every built-in module is loaded and listed with its command count") {
        editor_test::headless_editor editor = {};
        const core::json::document list = editor.ok("module.list");
        REQUIRE(list.root()["modules"].size() >= 1);
        CHECK(list.root()["loadOrder"].size() >= 1);
        CHECK(list.root()["pending"].as_i64() == 0);
        bool core = false;
        for (const core::json::value entry : list.root()["modules"]) {
            CHECK(entry["state"].as_string() == "LOADED");
            CHECK(entry["kind"].as_string() == "engine");
            core = core || entry["name"].as_string() == "conjure.core";
        }
        CHECK(core);
        CHECK(editor.fail(
          "module.unload",
          core::json::object({{"name", "conjure.core"}})
        ) == protocol_error_code::INVALID_ARGS);
        CHECK(editor.fail(
          "module.load",
          core::json::object({{"name", "no_such_module"}})
        ) == protocol_error_code::INVALID_ARGS);
        const core::json::document loaded = editor.ok("module.load", core::json::object({{"name", "conjure.core"}}));
        CHECK_FALSE(loaded.root()["requested"].as_bool());
        CHECK(editor.fail(
          "module.discover",
          core::json::object({{"directory", "nowhere_at_all"}})
        ) == protocol_error_code::PATH_NOT_FOUND);
        CHECK(editor.fail("module.build") == protocol_error_code::NOT_AVAILABLE);
    }
}
