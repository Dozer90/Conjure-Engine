#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/ui/imgui_frame_scope.h"
#include "conjure/editor/ui/panels/hierarchy_panel.h"

#include <imgui.h>

#include <string>

// hierarchy_panel.h: the scene tree drawn from hierarchy_cache against the null rhi.

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::editor::ui;

TEST_SUITE("editor/ui/panels/hierarchy_panel") {

    TEST_CASE("hierarchy_panel - constructs open with an empty cache") {
        hierarchy_panel panel = {};
        CHECK(panel.title() == "Hierarchy");
        CHECK(panel.is_open());
        CHECK(panel.rows_drawn() == 0);
        CHECK(panel.cache().rows().empty());
    }

    TEST_CASE("hierarchy_panel - draws the rows of the scene and reflects the selection") {
        editor_test::headless_editor editor = {};
        const std::string root = editor.create_entity("Root");
        (void) editor.create_entity("Child", root);
        (void) editor.create_entity("Other");
        (void) editor.ok("selection.set", core::json::object({{"entities", core::json::array({root})}}));
        hierarchy_panel panel = {};
        imgui_frame_scope scope(math::vec2(1280.0f, 2000.0f));
        // The first frame lays the window out; the rows are clipped to the second frame's region.
        for (int frame = 0; frame < 2; ++frame) {
            panel.update(editor.context());
            scope.begin_frame();
            ImGui::SetNextWindowSize(ImVec2(400.0f, 600.0f));
            panel.draw(editor.context());
            scope.end_frame();
        }
        CHECK(panel.rows_drawn() == 2);
        CHECK(has_flag(panel.cache().rows()[0].flags, hierarchy_row_flags::SELECTED));
        panel.cache().set_expanded(editor.scene().find_guid(*core::uuid::parse(root)), true);
        panel.update(editor.context());
        scope.begin_frame();
        ImGui::SetNextWindowSize(ImVec2(400.0f, 600.0f));
        panel.draw(editor.context());
        scope.end_frame();
        CHECK(panel.rows_drawn() == 3);
        (void) editor.ok("selection.clear");
        panel.update(editor.context());
        CHECK_FALSE(has_flag(panel.cache().rows()[0].flags, hierarchy_row_flags::SELECTED));
    }

    TEST_CASE("hierarchy_panel - without a scene it draws a hint and no rows") {
        editor_test::headless_editor_options options = {};
        options.withScene = false;
        editor_test::headless_editor editor(options);
        hierarchy_panel panel = {};
        imgui_frame_scope scope{};
        panel.update(editor.context());
        scope.begin_frame();
        panel.draw(editor.context());
        scope.end_frame();
        CHECK(panel.rows_drawn() == 0);
    }
}
