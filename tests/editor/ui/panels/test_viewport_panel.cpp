#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/math.h"
#include "conjure/editor/selection_set.h"
#include "conjure/editor/ui/imgui_frame_scope.h"
#include "conjure/editor/ui/panels/viewport_panel.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <string>

// viewport_panel.h: the input surface over the scene, picking and the gizmo toolbar.

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::editor::ui;

namespace {
    /// <summary>
    /// One frame of the panel in a window pinned to the whole 800x600 display.
    /// </summary>
    void draw_pinned(editor_test::headless_editor &editor, viewport_panel &panel, imgui_frame_scope &scope) {
        scope.begin_frame();
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(800.0f, 600.0f), ImGuiCond_Always);
        panel.draw(editor.context());
        scope.end_frame();
    }

    /// <summary>
    /// A 4x4 sprite at the origin under an editor camera looking at it from +z: the centre of the viewport
    /// picks it.
    /// </summary>
    [[nodiscard]] std::string add_centred_sprite(editor_test::headless_editor &editor) {
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
        return guid;
    }
}

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

    TEST_CASE("viewport_panel - a click picks again after the selection under the gizmo was cleared") {
        editor_test::headless_editor editor = {};
        const std::string guid = add_centred_sprite(editor);
        (void) editor.ok("selection.set", core::json::object({{"entities", core::json::array({guid})}}));
        viewport_panel panel = {};
        imgui_frame_scope scope(math::vec2(800.0f, 600.0f));
        draw_pinned(editor, panel, scope);
        // The projected origin: inside the translate gizmo's screen-move handle while the sprite is selected.
        const math::vec2 centre = panel.rect_min() + panel.rect_size() * 0.5f;
        for (int frame = 0; frame < 3; ++frame) {
            scope.set_mouse(centre, false);
            draw_pinned(editor, panel, scope);
        }
        REQUIRE(panel.gizmo().is_over());
        editor.context().selection().clear();
        draw_pinned(editor, panel, scope);
        CHECK_FALSE(panel.gizmo().is_over());
        scope.set_mouse(centre, true);
        draw_pinned(editor, panel, scope);
        scope.set_mouse(centre, false);
        draw_pinned(editor, panel, scope);
        CHECK(panel.pick_count() == 1);
        REQUIRE(editor.context().selection().size() == 1);
        CHECK(editor.context().selection().primary().to_string() == guid);
    }

    TEST_CASE("regression - F518 viewport_panel a right drag keeps orbiting after the cursor leaves the panel") {
        editor_test::headless_editor editor = {};
        viewport_panel panel = {};
        imgui_frame_scope scope(math::vec2(800.0f, 600.0f));
        draw_pinned(editor, panel, scope);
        ImGuiIO &io = ImGui::GetIO();
        const math::vec2 centre = panel.rect_min() + panel.rect_size() * 0.5f;
        scope.set_mouse(centre, false);
        draw_pinned(editor, panel, scope);
        io.AddMouseButtonEvent(ImGuiMouseButton_Right, true);
        draw_pinned(editor, panel, scope);
        scope.set_mouse(centre + math::vec2(40.0f, 0.0f), false);
        draw_pinned(editor, panel, scope);
        // Off the panel (and off the display) with the right button still held: the drag goes on.
        const math::vec3 before = editor.context().camera().position();
        scope.set_mouse(math::vec2(900.0f, centre.y), false);
        draw_pinned(editor, panel, scope);
        const math::vec3 after = editor.context().camera().position();
        CHECK(glm::length(after - before) > 1.0e-3f);
        io.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
        draw_pinned(editor, panel, scope);
        // Released outside: further movement there no longer moves the camera.
        const math::vec3 released = editor.context().camera().position();
        scope.set_mouse(math::vec2(950.0f, centre.y + 30.0f), false);
        draw_pinned(editor, panel, scope);
        CHECK(glm::length(editor.context().camera().position() - released) < 1.0e-6f);
        CHECK(panel.pick_count() == 0);
    }

    TEST_CASE("regression - F514 viewport_panel reports its aspect to the context") {
        editor_test::headless_editor editor = {};
        viewport_panel panel = {};
        imgui_frame_scope scope(math::vec2(800.0f, 600.0f));
        draw_pinned(editor, panel, scope);
        const f32 expected = panel.rect_size().x / panel.rect_size().y;
        CHECK(editor.context().viewport_aspect() == doctest::Approx(expected));
        CHECK(editor.context().viewport_aspect() != doctest::Approx(editor_context::DEFAULT_VIEWPORT_ASPECT));
    }

    TEST_CASE("viewport_panel - the toolbar's Snap toggle and step field drive the gizmo's snapping") {
        editor_test::headless_editor editor = {};
        viewport_panel panel = {};
        imgui_frame_scope scope(math::vec2(800.0f, 600.0f));
        draw_pinned(editor, panel, scope);
        ImGuiWindow *window = ImGui::FindWindowByName("Viewport");
        REQUIRE(window != nullptr);
        CHECK_FALSE(panel.gizmo().snap_enabled());
        ImGui::ActivateItemByID(window->GetID("Snap"));
        draw_pinned(editor, panel, scope);
        draw_pinned(editor, panel, scope);
        REQUIRE(panel.gizmo().snap_enabled());
        ImGuiIO &io = ImGui::GetIO();
        for (const gizmo_operation operation : {
          gizmo_operation::TRANSLATE, gizmo_operation::ROTATE, gizmo_operation::SCALE, gizmo_operation::UNIVERSAL,
        }) {
            CAPTURE(to_string(operation));
            panel.gizmo().set_operation(operation);
            panel.gizmo().set_snap(true, 0.5f, 15.0f, 0.1f);
            draw_pinned(editor, panel, scope);
            // Activating the step field for text input (what Ctrl+click does) shows it is drawn for this operation.
            ImGui::ActivateItemByID(window->GetID("##snap_step"));
            ImGui::GetCurrentContext()->NavNextActivateFlags = ImGuiActivateFlags_PreferInput;
            draw_pinned(editor, panel, scope);
            io.AddInputCharactersUTF8("7");
            draw_pinned(editor, panel, scope);
            io.AddKeyEvent(ImGuiKey_Enter, true);
            draw_pinned(editor, panel, scope);
            io.AddKeyEvent(ImGuiKey_Enter, false);
            draw_pinned(editor, panel, scope);
            if (operation == gizmo_operation::ROTATE) {
                CHECK(panel.gizmo().snap_rotate_degrees() == doctest::Approx(7.0f));
            } else if (operation == gizmo_operation::SCALE) {
                CHECK(panel.gizmo().snap_scale() == doctest::Approx(7.0f));
            } else {
                CHECK(panel.gizmo().snap_translate() == doctest::Approx(7.0f));
            }
        }
        CHECK(panel.gizmo().snap_enabled());
    }
}
