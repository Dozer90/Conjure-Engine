#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/ui/imgui_frame_scope.h"
#include "conjure/editor/ui/panels/asset_browser_panel.h"

// asset_browser_panel.h: the asset database as a table.

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::editor::ui;

TEST_SUITE("editor/ui/panels/asset_browser_panel") {

    TEST_CASE("asset_browser_panel - constructs open and empty") {
        asset_browser_panel panel = {};
        CHECK(panel.title() == "Assets");
        CHECK(panel.is_open());
        CHECK(panel.rows_drawn() == 0);
        CHECK(panel.selected_path().empty());
    }

    TEST_CASE("asset_browser_panel - draws one row per primary asset") {
        editor_test::headless_editor editor = {};
        (void) editor_test::write_tiny_png(editor.project_directory() / "assets" / "one.png");
        (void) editor.ok("asset.import", core::json::object({{"source", "one.png"}}));
        (void) editor.ok("scene.save");
        asset_browser_panel panel = {};
        imgui_frame_scope scope{};
        scope.begin_frame();
        panel.draw(editor.context());
        scope.end_frame();
        CHECK(panel.rows_drawn() == 2);
    }

    TEST_CASE("asset_browser_panel - without a project it draws a hint") {
        editor_test::headless_editor_options options = {};
        options.withProject = false;
        options.withScene = false;
        editor_test::headless_editor editor(options);
        asset_browser_panel panel = {};
        imgui_frame_scope scope{};
        scope.begin_frame();
        panel.draw(editor.context());
        scope.end_frame();
        CHECK(panel.rows_drawn() == 0);
    }
}
