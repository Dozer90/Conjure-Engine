#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/editor/ui/imgui_frame_scope.h"
#include "conjure/editor/ui/panels/history_panel.h"

// history_panel.h: the undo stack as a table.

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::editor::ui;

TEST_SUITE("editor/ui/panels/history_panel") {

    TEST_CASE("history_panel - constructs closed by default") {
        history_panel panel = {};
        CHECK(panel.title() == "History");
        CHECK_FALSE(panel.is_open());
        CHECK(panel.entries_drawn() == 0);
    }

    TEST_CASE("history_panel - draws one row per undo entry") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("a");
        (void) editor.create_entity("b");
        (void) editor.ok("history.undo");
        history_panel panel = {};
        panel.set_open(true);
        imgui_frame_scope scope(math::vec2(1280.0f, 2000.0f));
        scope.begin_frame();
        panel.draw(editor.context());
        scope.end_frame();
        CHECK(panel.entries_drawn() == 2);
    }
}
