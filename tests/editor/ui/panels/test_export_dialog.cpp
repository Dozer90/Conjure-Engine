#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/result.h"
#include "conjure/editor/command_registry.h"
#include "conjure/editor/editor_command.h"
#include "conjure/editor/ui/imgui_frame_scope.h"
#include "conjure/editor/ui/panels/export_dialog.h"

#include <filesystem>
#include <memory>
#include <string>
#include <utility>

// export_dialog.h: project.export from the UI with progress.

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::editor::ui;

namespace {
    /// <summary>
    /// The editor of the cases that export for real. Its project disables scripting: a project that enables it
    /// gets its managed code published (follow-up FU-PDB), which needs the dotnet the fixture leaves out, and these
    /// cases test the dialog, not the export.
    /// </summary>
    [[nodiscard]] editor_test::headless_editor_options exporting_editor() {
        editor_test::headless_editor_options options = {};
        options.extraProjectJson = R"("scripting": {"enabled": false})";
        return options;
    }
}

TEST_SUITE("editor/ui/panels/export_dialog") {

    TEST_CASE("export_dialog - constructs closed and idle") {
        export_dialog dialog = {};
        CHECK(dialog.title() == "Export");
        CHECK_FALSE(dialog.is_open());
        CHECK_FALSE(dialog.is_running());
        CHECK(dialog.status().empty());
    }

    TEST_CASE("export_dialog - start_export runs project.export and reports the result") {
        editor_test::headless_editor editor(exporting_editor());
        (void) editor.create_entity("exported");
        (void) editor.ok("scene.save");
        export_dialog dialog = {};
        const std::filesystem::path output = editor.project_directory() / "ui_export";
        dialog.set_directory(output.generic_string());
        dialog.start_export(editor.context());
        for (u32 frame = 0; frame < 2000 && dialog.is_running(); ++frame) {
            editor.pump(1);
            dialog.update(editor.context());
        }
        CHECK_FALSE(dialog.is_running());
        CHECK(dialog.status().find("exported") != std::string::npos);
        CHECK(std::filesystem::is_regular_file(output / "project.json"));
        dialog.set_open(true);
        imgui_frame_scope scope{};
        scope.begin_frame();
        dialog.draw(editor.context());
        scope.end_frame();
    }

    TEST_CASE("regression - an export dialog destroyed before its update releases its event client and response sink") {
        editor_test::headless_editor editor(exporting_editor());
        (void) editor.create_entity("exported");
        (void) editor.ok("scene.save");
        const usize clientsBefore = editor.context().events().client_count();
        {
            export_dialog abandoned = {};
            abandoned.set_directory((editor.project_directory() / "abandoned_export").generic_string());
            abandoned.start_export(editor.context());
            CHECK(abandoned.is_running());
            CHECK(editor.context().events().client_count() == clientsBefore + 1);
        }
        CHECK(editor.context().events().client_count() == clientsBefore);
        // The abandoned export may still finish; its reply has nowhere to go and is dropped.
        editor.pump(4);
        export_dialog dialog = {};
        const std::filesystem::path output = editor.project_directory() / "second_export";
        dialog.set_directory(output.generic_string());
        dialog.start_export(editor.context());
        for (u32 frame = 0; frame < 2000 && dialog.is_running(); ++frame) {
            editor.pump(1);
            dialog.update(editor.context());
        }
        CHECK_FALSE(dialog.is_running());
        CHECK(dialog.status().find("exported") != std::string::npos);
        CHECK(editor.context().events().client_count() == clientsBefore);
    }

    TEST_CASE("export_dialog - without a project it only shows a hint") {
        editor_test::headless_editor_options options = {};
        options.withProject = false;
        options.withScene = false;
        editor_test::headless_editor editor(options);
        export_dialog dialog = {};
        dialog.set_open(true);
        imgui_frame_scope scope{};
        scope.begin_frame();
        dialog.draw(editor.context());
        scope.end_frame();
        dialog.start_export(editor.context());
        CHECK_FALSE(dialog.is_running());
    }

    TEST_CASE("regression - code sweep F650 the export dialog sends project.export without a configuration") {
        editor_test::headless_editor editor = {};
        // A stand-in records what the dialog sends: the request itself is what the decision is about, and the
        // case above already runs a real export.
        const std::shared_ptr<std::string> sent = std::make_shared<std::string>();
        command_registry &registry = editor.context().registry();
        REQUIRE(registry.remove("project.export"));
        command_descriptor recording = {};
        recording.name = "project.export";
        recording.summary = "A stand-in that records its arguments and exports nothing.";
        recording.argsSchema = R"({"type":"object","additionalProperties":true})";
        recording.resultSchema = R"({"type":"object"})";
        recording.owner = "test";
        recording.handler = [sent](command_invocation &invocation) -> core::result<command_outcome> {
            *sent = core::json::document::from_value(invocation.args).to_string();
            command_outcome outcome = {};
            outcome.result = core::json::object({{"assetCount", 0}, {"bytes", 0}, {"directory", "recorded"}});
            return outcome;
        };
        CONJURE_REQUIRE_OK(registry.add(std::move(recording)));

        export_dialog dialog = {};
        const std::filesystem::path output = editor.project_directory() / "dialog_export";
        dialog.set_directory(output.generic_string());
        dialog.start_export(editor.context());
        for (u32 frame = 0; frame < 2000 && dialog.is_running(); ++frame) {
            editor.pump(1);
            dialog.update(editor.context());
        }
        REQUIRE_FALSE(dialog.is_running());
        CHECK(dialog.status().find("exported") != std::string::npos);
        const core::result<core::json::document> args = core::json::document::parse(*sent);
        CONJURE_REQUIRE_OK(args);
        // The output directory and nothing else: project.export refuses a "config" member (INVALID_ARGS).
        CHECK(args.value().root()["directory"].as_string() == output.generic_string());
        CHECK_FALSE(args.value().root().contains("config"));
        CHECK(args.value().root().size() == 1);
    }
}
