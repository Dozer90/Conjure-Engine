#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/math.h"
#include "conjure/core/uuid.h"
#include "conjure/editor/command_dispatcher.h"
#include "conjure/editor/editor_command.h"
#include "conjure/editor/ui/gizmo_controller.h"
#include "conjure/editor/ui/imgui_frame_scope.h"
#include "conjure/editor/undo_stack.h"

#include <imgui.h>

#include <array>
#include <string>

// gizmo_controller.h (docs/design/04_editor.md 3.9): ImGuizmo over the selection, edits through component.set_many.

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::editor::ui;

TEST_SUITE("editor/ui/gizmo_controller") {

    TEST_CASE("gizmo_operation - names and settings") {
        CHECK(to_string(gizmo_operation::TRANSLATE) == "translate");
        CHECK(to_string(gizmo_operation::ROTATE) == "rotate");
        CHECK(to_string(gizmo_operation::SCALE) == "scale");
        CHECK(to_string(gizmo_operation::UNIVERSAL) == "universal");
        gizmo_controller gizmo = {};
        CHECK(gizmo.operation() == gizmo_operation::TRANSLATE);
        CHECK(gizmo.space() == gizmo_space::WORLD);
        gizmo.set_operation(gizmo_operation::SCALE);
        gizmo.set_space(gizmo_space::LOCAL);
        gizmo.set_snap(true, 0.5f, 15.0f, 0.1f);
        CHECK(gizmo.operation() == gizmo_operation::SCALE);
        CHECK(gizmo.space() == gizmo_space::LOCAL);
        CHECK_FALSE(gizmo.is_dragging());
        CHECK(gizmo.manipulation_count() == 0);
    }

    TEST_CASE("gizmo_controller - draws nothing without a selection and draws the gizmo for a selected entity") {
        editor_test::headless_editor editor = {};
        gizmo_controller gizmo = {};
        imgui_frame_scope scope{};
        scope.begin_frame();
        ImGui::Begin("viewport");
        CHECK_FALSE(gizmo.draw(editor.context(), math::vec2(0.0f), math::vec2(640.0f, 480.0f)));
        ImGui::End();
        scope.end_frame();
        const std::string guid = editor.create_entity("Moved");
        (void) editor.ok("selection.set", core::json::object({{"entities", core::json::array({guid})}}));
        scope.begin_frame();
        ImGui::Begin("viewport");
        const bool manipulated = gizmo.draw(editor.context(), math::vec2(0.0f), math::vec2(640.0f, 480.0f));
        ImGui::End();
        scope.end_frame();
        CHECK_FALSE(manipulated);
        CHECK_FALSE(gizmo.is_dragging());
        CHECK_FALSE(gizmo.draw(editor.context(), math::vec2(0.0f), math::vec2(0.0f)));
    }

    TEST_CASE("gizmo_controller - apply_delta moves the selection through component.set_many in local space") {
        editor_test::headless_editor editor = {};
        const std::string parent = editor.create_entity("Parent");
        const std::string child = editor.create_entity("Child", parent);
        (void) editor.ok(
          "entity.set_transform",
          core::json::object({{"entity", parent}, {"position", core::json::array({10.0f, 0.0f, 0.0f})}})
        );
        (void) editor.ok(
          "entity.set_transform",
          core::json::object({{"entity", child}, {"position", core::json::array({1.0f, 0.0f, 0.0f})}})
        );
        editor.pump(1);
        (void) editor.ok("selection.set", core::json::object({{"entities", core::json::array({child})}}));
        gizmo_controller gizmo = {};
        imgui_frame_scope scope{};
        scope.begin_frame();
        ImGui::Begin("viewport");
        (void) gizmo.draw(editor.context(), math::vec2(0.0f), math::vec2(640.0f, 480.0f));
        ImGui::End();
        scope.end_frame();
        const usize undoBefore = editor.context().undo().size();
        // Simulate a drag: the draw above cached the selection's world matrices; begin a transaction (what the
        // first manipulated frame does), apply two world deltas and end it.
        command_request begin = {};
        begin.name = "editor.begin_transaction";
        begin.args = core::json::object({{"label", "translate"}});
        CONJURE_REQUIRE_OK(editor.context().dispatcher().execute(begin));
        gizmo.apply_delta(editor.context(), glm::translate(math::mat4(1.0f), math::vec3(0.0f, 2.0f, 0.0f)));
        gizmo.apply_delta(editor.context(), glm::translate(math::mat4(1.0f), math::vec3(0.0f, 3.0f, 0.0f)));
        command_request end = {};
        end.name = "editor.end_transaction";
        CONJURE_REQUIRE_OK(editor.context().dispatcher().execute(end));
        CHECK(gizmo.manipulation_count() == 2);
        // One undo entry for the whole drag.
        CHECK(editor.context().undo().size() == undoBefore + 1);
        const ecs::entity childEntity = editor.scene().find_guid(*core::uuid::parse(child));
        // The world delta became a local translation under the parent at x = 10.
        CONJURE_CHECK_APPROX(editor.scene().local_transform_of(childEntity)->position, math::vec3(1.0f, 5.0f, 0.0f));
        (void) editor.ok("history.undo");
        CONJURE_CHECK_APPROX(editor.scene().local_transform_of(childEntity)->position, math::vec3(1.0f, 0.0f, 0.0f));
        gizmo.end_drag(editor.context());
        CHECK_FALSE(gizmo.is_dragging());
    }

    TEST_CASE("regression - a selected child of a selected parent moves once with its parent") {
        editor_test::headless_editor editor = {};
        const std::string parent = editor.create_entity("Parent");
        const std::string child = editor.create_entity("Child", parent);
        (void) editor.ok(
          "entity.set_transform",
          core::json::object({{"entity", parent}, {"position", core::json::array({10.0f, 0.0f, 0.0f})}})
        );
        (void) editor.ok(
          "entity.set_transform",
          core::json::object({{"entity", child}, {"position", core::json::array({1.0f, 0.0f, 0.0f})}})
        );
        editor.pump(1);
        // The child first: the order of the selection must not matter.
        (void) editor.ok("selection.set", core::json::object({{"entities", core::json::array({child, parent})}}));
        gizmo_controller gizmo = {};
        imgui_frame_scope scope{};
        scope.begin_frame();
        ImGui::Begin("viewport");
        (void) gizmo.draw(editor.context(), math::vec2(0.0f), math::vec2(640.0f, 480.0f));
        ImGui::End();
        scope.end_frame();
        command_request begin = {};
        begin.name = "editor.begin_transaction";
        begin.args = core::json::object({{"label", "translate"}});
        CONJURE_REQUIRE_OK(editor.context().dispatcher().execute(begin));
        gizmo.apply_delta(editor.context(), glm::translate(math::mat4(1.0f), math::vec3(0.0f, 2.0f, 0.0f)));
        command_request end = {};
        end.name = "editor.end_transaction";
        CONJURE_REQUIRE_OK(editor.context().dispatcher().execute(end));
        const ecs::entity parentEntity = editor.scene().find_guid(*core::uuid::parse(parent));
        const ecs::entity childEntity = editor.scene().find_guid(*core::uuid::parse(child));
        CONJURE_CHECK_APPROX(editor.scene().local_transform_of(parentEntity)->position, math::vec3(10.0f, 2.0f, 0.0f));
        CONJURE_CHECK_APPROX(editor.scene().local_transform_of(childEntity)->position, math::vec3(1.0f, 0.0f, 0.0f));
    }

    TEST_CASE("gizmo_controller - snapping defaults to 0.5 units, 15 degrees and 0.1 and set_snap round-trips") {
        gizmo_controller gizmo = {};
        CHECK_FALSE(gizmo.snap_enabled());
        CHECK(gizmo.snap_translate() == doctest::Approx(0.5f));
        CHECK(gizmo.snap_rotate_degrees() == doctest::Approx(15.0f));
        CHECK(gizmo.snap_scale() == doctest::Approx(0.1f));
        gizmo.set_snap(true, 0.25f, 45.0f, 0.5f);
        CHECK(gizmo.snap_enabled());
        CHECK(gizmo.snap_translate() == doctest::Approx(0.25f));
        CHECK(gizmo.snap_rotate_degrees() == doctest::Approx(45.0f));
        CHECK(gizmo.snap_scale() == doctest::Approx(0.5f));
        gizmo.set_snap(false, 0.25f, 45.0f, 0.5f);
        CHECK_FALSE(gizmo.snap_enabled());
    }

    TEST_CASE("gizmo_controller - is_over is cleared on a frame with nothing selected") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("Centre");
        (void) editor.ok("selection.set", core::json::object({{"entities", core::json::array({guid})}}));
        gizmo_controller gizmo = {};
        imgui_frame_scope scope(math::vec2(640.0f, 480.0f));
        // The default editor camera looks at the origin, which projects to the centre of the rectangle; the
        // translate gizmo's screen-move handle sits there.
        const math::vec2 centre = math::vec2(320.0f, 240.0f);
        for (int frame = 0; frame < 3; ++frame) {
            scope.set_mouse(centre, false);
            scope.begin_frame();
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(640.0f, 480.0f), ImGuiCond_Always);
            ImGui::Begin("viewport");
            (void) gizmo.draw(editor.context(), math::vec2(0.0f), math::vec2(640.0f, 480.0f));
            ImGui::End();
            scope.end_frame();
        }
        REQUIRE(gizmo.is_over());
        (void) editor.ok("selection.clear");
        scope.set_mouse(centre, false);
        scope.begin_frame();
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(640.0f, 480.0f), ImGuiCond_Always);
        ImGui::Begin("viewport");
        CHECK_FALSE(gizmo.draw(editor.context(), math::vec2(0.0f), math::vec2(640.0f, 480.0f)));
        ImGui::End();
        scope.end_frame();
        CHECK_FALSE(gizmo.is_over());
    }
}
