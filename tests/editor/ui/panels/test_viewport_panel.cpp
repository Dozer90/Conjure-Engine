#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/math.h"
#include "conjure/editor/ui/imgui_frame_scope.h"
#include "conjure/editor/ui/panels/viewport_panel.h"

#include <string>

// viewport_panel.h: the input surface over the scene, picking and the gizmo toolbar.

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::editor::ui;

TEST_SUITE("editor/ui/panels/viewport_panel") {

    TEST_CASE("viewport_panel - constructs with a translate gizmo") {
        viewport_panel panel = {};
        CHECK(panel.title() == "Viewport");
        CHECK(panel.gizmo().operation() == gizmo_operation::TRANSLATE);
        CHECK(panel.pick_count() == 0);
        CHECK(panel.rect_size().x == doctest::Approx(1.0f));
    }

    TEST_CASE("viewport_panel - draws and records the rectangle") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("Thing");
        viewport_panel panel = {};
        imgui_frame_scope scope(math::vec2(800.0f, 600.0f));
        scope.begin_frame();
        panel.draw(editor.context());
        scope.end_frame();
        CHECK(panel.rect_size().x >= 1.0f);
        CHECK(panel.rect_size().y >= 1.0f);
        scope.begin_frame();
        panel.draw(editor.context());
        scope.end_frame();
        CHECK(panel.pick_count() == 0);
    }

    TEST_CASE("viewport_panel - pick_at selects the sprite under the centre and clears on a miss") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("Target");
        (void) editor.ok(
          "component.add",
          core::json::object({
            {"entity", guid},
            {"type", "sprite"},
            {"value", core::json::object({{"size", core::json::array({4.0f, 4.0f})}})},
          })
        );
        (void) editor.ok(
          "view.set_camera",
          core::json::object({
            {"position", core::json::array({0.0f, 0.0f, 10.0f})},
            {"target", core::json::array({0.0f, 0.0f, 0.0f})},
          })
        );
        editor.pump(2);
        viewport_panel panel = {};
        panel.pick_at(editor.context(), math::vec2(0.5f, 0.5f), false);
        CHECK(panel.pick_count() == 1);
        REQUIRE(editor.context().selection().size() == 1);
        CHECK(editor.context().selection().primary().to_string() == guid);
        panel.pick_at(editor.context(), math::vec2(0.5f, 0.5f), true);
        CHECK(editor.context().selection().empty());
        panel.pick_at(editor.context(), math::vec2(0.5f, 0.5f), false);
        CHECK(editor.context().selection().size() == 1);
        panel.pick_at(editor.context(), math::vec2(0.01f, 0.01f), false);
        CHECK(editor.context().selection().empty());
        CHECK(panel.pick_count() == 4);
    }
}
