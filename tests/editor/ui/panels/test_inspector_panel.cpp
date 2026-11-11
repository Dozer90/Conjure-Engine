#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/editor/ui/imgui_frame_scope.h"
#include "conjure/editor/ui/panels/inspector_panel.h"
#include "conjure/editor/ui/property_drawer.h"

#include <string>

// inspector_panel.h: the components of the primary selection through the property drawers.

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::editor::ui;

TEST_SUITE("editor/ui/panels/inspector_panel") {

    TEST_CASE("inspector_panel - constructs over a drawer registry") {
        property_drawer_registry drawers = {};
        inspector_panel panel(drawers);
        CHECK(panel.title() == "Inspector");
        CHECK(panel.is_open());
        CHECK(panel.components_drawn() == 0);
    }

    TEST_CASE("inspector_panel - draws every component of the selected entity and nothing without a selection") {
        editor_test::headless_editor editor = {};
        property_drawer_registry drawers = {};
        inspector_panel panel(drawers);
        imgui_frame_scope scope{};
        scope.begin_frame();
        panel.draw(editor.context());
        scope.end_frame();
        CHECK(panel.components_drawn() == 0);
        const std::string guid = editor.create_entity("Inspected");
        (void) editor.ok("component.add", core::json::object({{"entity", guid}, {"type", "sprite"}}));
        (void) editor.ok("component.add", core::json::object({{"entity", guid}, {"type", "camera"}}));
        (void) editor.ok(
          "script.attach",
          core::json::object({
            {"entity", guid},
            {"className", "Game.Thing"},
            {"fields", core::json::object({{"speed", 2.0f}})},
          })
        );
        (void) editor.ok("selection.set", core::json::object({{"entities", core::json::array({guid})}}));
        const u64 revision = editor.context().revision();
        scope.begin_frame();
        panel.draw(editor.context());
        scope.end_frame();
        CHECK(panel.components_drawn() >= 4);
        // Drawing never edits: the revision stays.
        CHECK(editor.context().revision() == revision);
        (void) editor.ok("entity.destroy", core::json::object({{"entity", guid}}));
        scope.begin_frame();
        panel.draw(editor.context());
        scope.end_frame();
        CHECK(panel.components_drawn() == 0);
    }
}
