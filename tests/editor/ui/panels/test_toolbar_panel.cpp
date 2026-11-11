#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/editor/play_controller.h"
#include "conjure/editor/ui/imgui_frame_scope.h"
#include "conjure/editor/ui/panels/toolbar_panel.h"

// toolbar_panel.h: the play bar.

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::editor::ui;

TEST_SUITE("editor/ui/panels/toolbar_panel") {

    TEST_CASE("toolbar_panel - constructs open") {
        toolbar_panel panel = {};
        CHECK(panel.title() == "Toolbar");
        CHECK(panel.is_open());
    }

    TEST_CASE("toolbar_panel - draws in every play state without changing it") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("x");
        toolbar_panel panel = {};
        imgui_frame_scope scope{};
        scope.begin_frame();
        panel.draw(editor.context());
        scope.end_frame();
        CHECK(editor.context().play().state() == play_state::EDITING);
        (void) editor.ok("play.start");
        scope.begin_frame();
        panel.draw(editor.context());
        scope.end_frame();
        CHECK(editor.context().play().state() == play_state::PLAYING);
        (void) editor.ok("play.pause");
        scope.begin_frame();
        panel.draw(editor.context());
        scope.end_frame();
        CHECK(editor.context().play().state() == play_state::PAUSED);
        (void) editor.ok("play.stop");
    }

    TEST_CASE("toolbar_panel - draws without a scene") {
        editor_test::headless_editor_options options = {};
        options.withScene = false;
        editor_test::headless_editor editor(options);
        toolbar_panel panel = {};
        imgui_frame_scope scope{};
        scope.begin_frame();
        panel.draw(editor.context());
        scope.end_frame();
        CHECK_FALSE(editor.context().has_scene());
    }
}
