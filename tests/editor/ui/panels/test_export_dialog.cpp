#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/ui/imgui_frame_scope.h"
#include "conjure/editor/ui/panels/export_dialog.h"

#include <filesystem>
#include <string>

// export_dialog.h: project.export from the UI with progress.

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::editor::ui;

TEST_SUITE("editor/ui/panels/export_dialog") {

    TEST_CASE("export_dialog - constructs closed and idle") {
        export_dialog dialog = {};
        CHECK(dialog.title() == "Export");
        CHECK_FALSE(dialog.is_open());
        CHECK_FALSE(dialog.is_running());
        CHECK(dialog.status().empty());
    }

    TEST_CASE("export_dialog - start_export runs project.export and reports the result") {
        editor_test::headless_editor editor = {};
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
        editor_test::headless_editor editor = {};
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
}
