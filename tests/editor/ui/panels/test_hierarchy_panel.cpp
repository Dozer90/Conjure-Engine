#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/core/json/document.h"
#include "conjure/core/uuid.h"
#include "conjure/editor/selection_set.h"
#include "conjure/editor/ui/imgui_frame_scope.h"
#include "conjure/editor/ui/panels/hierarchy_panel.h"
#include "conjure/scene/hierarchy.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <array>
#include <string>
#include <string_view>

// hierarchy_panel.h: the scene tree drawn from hierarchy_cache against the null rhi.

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::editor::ui;

namespace {
    ecs::entity entity_of(editor_test::headless_editor &editor, const std::string &guid) {
        return editor.scene().find_guid(*core::uuid::parse(guid));
    }

    /// <summary>
    /// update() and one frame of the panel in a window pinned at the origin, 400x600.
    /// </summary>
    void draw_pinned(editor_test::headless_editor &editor, hierarchy_panel &panel, imgui_frame_scope &scope) {
        panel.update(editor.context());
        scope.begin_frame();
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(400.0f, 600.0f), ImGuiCond_Always);
        panel.draw(editor.context());
        scope.end_frame();
    }

    /// <summary>
    /// The panel's "rows" child window (ImGui names it "Hierarchy/rows_<id>").
    /// </summary>
    [[nodiscard]] const ImGuiWindow *rows_window() {
        for (const ImGuiWindow *window : ImGui::GetCurrentContext()->Windows) {
            if (std::string_view(window->Name).starts_with("Hierarchy/rows")) {
                return window;
            }
        }
        return nullptr;
    }

    /// <summary>
    /// A point on the label of a row as the last frame laid it out (rows are one frame tall).
    /// </summary>
    [[nodiscard]] math::vec2 row_point(usize index) {
        const ImGuiWindow *rows = rows_window();
        REQUIRE(rows != nullptr);
        const f32 rowHeight = ImGui::GetFrameHeightWithSpacing();
        return math::vec2(
          rows->DC.CursorStartPos.x + 150.0f,
          rows->DC.CursorStartPos.y + rowHeight * static_cast<f32>(index) + ImGui::GetFrameHeight() * 0.5f
        );
    }

    /// <summary>
    /// A press and a release of the left button at a point, one frame each, after a frame hovering it.
    /// </summary>
    void click(
      editor_test::headless_editor &editor,
      hierarchy_panel &panel,
      imgui_frame_scope &scope,
      const math::vec2 &point
    ) {
        scope.set_mouse(point, false);
        draw_pinned(editor, panel, scope);
        scope.set_mouse(point, true);
        draw_pinned(editor, panel, scope);
        scope.set_mouse(point, false);
        draw_pinned(editor, panel, scope);
    }

    /// <summary>
    /// The modifier that sets io.KeyCtrl: with macOS behaviours on, AddKeyEvent swaps Ctrl and Super.
    /// </summary>
    [[nodiscard]] ImGuiKey ctrl_modifier() {
        return ImGui::GetIO().ConfigMacOSXBehaviors ? ImGuiMod_Super : ImGuiMod_Ctrl;
    }

    void select(editor_test::headless_editor &editor, const std::string &guid) {
        (void) editor.ok("selection.set", core::json::object({{"entities", core::json::array({guid})}}));
    }
}

TEST_SUITE("editor/ui/panels/hierarchy_panel") {

    TEST_CASE("hierarchy_panel - constructs open with an empty cache") {
        hierarchy_panel panel = {};
        CHECK(panel.title() == "Hierarchy");
        CHECK(panel.is_open());
        CHECK(panel.rows_drawn() == 0);
        CHECK(panel.cache().rows().empty());
        CHECK_FALSE(panel.is_renaming());
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
        panel.cache().set_expanded(editor.scene(), entity_of(editor, root), true);
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

    TEST_CASE("regression - Delete while renaming an entity edits the name and destroys nothing") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("Target");
        select(editor, guid);
        hierarchy_panel panel = {};
        imgui_frame_scope scope(math::vec2(1280.0f, 2000.0f));
        draw_pinned(editor, panel, scope);
        panel.begin_rename(editor.context(), entity_of(editor, guid));
        REQUIRE(panel.is_renaming());
        // The field asks for the focus on its first frame and holds it from the next.
        for (int frame = 0; frame < 3; ++frame) {
            draw_pinned(editor, panel, scope);
        }
        REQUIRE(ImGui::GetActiveID() != 0);
        ImGuiIO &io = ImGui::GetIO();
        io.AddKeyEvent(ImGuiKey_Delete, true);
        draw_pinned(editor, panel, scope);
        io.AddKeyEvent(ImGuiKey_Delete, false);
        draw_pinned(editor, panel, scope);
        CHECK_FALSE(entity_of(editor, guid).is_null());
        REQUIRE(editor.context().selection().size() == 1);
        CHECK(editor.context().selection().primary().to_string() == guid);
        CHECK(panel.is_renaming());
    }

    TEST_CASE("regression - a renamed or disabled entity's row updates on the next draw") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("Before");
        hierarchy_panel panel = {};
        imgui_frame_scope scope(math::vec2(1280.0f, 2000.0f));
        for (int frame = 0; frame < 2; ++frame) {
            draw_pinned(editor, panel, scope);
        }
        REQUIRE(panel.cache().rows().size() == 1);
        CHECK(panel.cache().rows()[0].nameId.view() == "Before");
        (void) editor.ok("entity.rename", core::json::object({{"entity", guid}, {"name", "After"}}));
        (void) editor.ok("entity.set_enabled", core::json::object({{"entity", guid}, {"enabled", false}}));
        draw_pinned(editor, panel, scope);
        REQUIRE(panel.cache().rows().size() == 1);
        CHECK(panel.cache().rows()[0].nameId.view() == "After");
        CHECK(has_flag(panel.cache().rows()[0].flags, hierarchy_row_flags::DISABLED));
        // Under a filter a rename can change which rows match, so it rebuilds.
        panel.cache().set_filter("after");
        draw_pinned(editor, panel, scope);
        CHECK(panel.cache().rows().size() == 1);
        (void) editor.ok("entity.rename", core::json::object({{"entity", guid}, {"name", "Elsewhere"}}));
        draw_pinned(editor, panel, scope);
        CHECK(panel.cache().rows().empty());
    }

    TEST_CASE("regression - dropping an entity below the last row reparents it to the root") {
        editor_test::headless_editor editor = {};
        const std::string root = editor.create_entity("Root");
        const std::string child = editor.create_entity("Child", root);
        hierarchy_panel panel = {};
        panel.cache().set_expanded(editor.scene(), entity_of(editor, root), true);
        imgui_frame_scope scope(math::vec2(1280.0f, 2000.0f));
        for (int frame = 0; frame < 2; ++frame) {
            draw_pinned(editor, panel, scope);
        }
        REQUIRE(panel.cache().rows().size() == 2);
        const math::vec2 source = row_point(1);
        const ImGuiWindow *rows = rows_window();
        REQUIRE(rows != nullptr);
        const math::vec2 below = math::vec2(source.x, rows->Pos.y + rows->Size.y - 20.0f);
        REQUIRE(below.y > source.y + 100.0f);
        // Press on Child's row, drag past the threshold, hover the empty area for two frames and release there.
        scope.set_mouse(source, false);
        draw_pinned(editor, panel, scope);
        scope.set_mouse(source, true);
        draw_pinned(editor, panel, scope);
        scope.set_mouse(source + math::vec2(0.0f, 12.0f), true);
        draw_pinned(editor, panel, scope);
        scope.set_mouse(below, true);
        draw_pinned(editor, panel, scope);
        draw_pinned(editor, panel, scope);
        scope.set_mouse(below, false);
        draw_pinned(editor, panel, scope);
        draw_pinned(editor, panel, scope);
        CHECK(editor.scene().tree().parent_of(entity_of(editor, child)).is_null());
    }

    TEST_CASE("regression - the hierarchy clipper draws only the rows that fit the window") {
        editor_test::headless_editor editor = {};
        (void) editor.ok("entity.create_many", core::json::object({{"count", 200}}));
        hierarchy_panel panel = {};
        imgui_frame_scope scope(math::vec2(1280.0f, 2000.0f));
        for (int frame = 0; frame < 2; ++frame) {
            draw_pinned(editor, panel, scope);
        }
        const u32 fitting = static_cast<u32>(600.0f / ImGui::GetFrameHeightWithSpacing()) + 2;
        CHECK(panel.rows_drawn() > 0);
        CHECK(panel.rows_drawn() <= fitting);
    }

    TEST_CASE("hierarchy_panel - clicking away from an inline rename commits the typed name and releases the focus") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("Target");
        hierarchy_panel panel = {};
        imgui_frame_scope scope(math::vec2(1280.0f, 2000.0f));
        draw_pinned(editor, panel, scope);
        panel.begin_rename(editor.context(), entity_of(editor, guid));
        for (int frame = 0; frame < 2; ++frame) {
            draw_pinned(editor, panel, scope);
        }
        ImGui::GetIO().AddInputCharactersUTF8("Renamed");
        draw_pinned(editor, panel, scope);
        draw_pinned(editor, panel, scope);
        // Outside the window: the field loses the focus, which commits what was typed.
        click(editor, panel, scope, math::vec2(900.0f, 900.0f));
        CHECK(editor.scene().name_of(entity_of(editor, guid)).view() == "Renamed");
        CHECK_FALSE(panel.is_renaming());
        draw_pinned(editor, panel, scope);
        CHECK(ImGui::GetActiveID() == 0);
        CHECK(panel.cache().rows()[0].nameId.view() == "Renamed");
    }

    TEST_CASE("hierarchy_panel - Escape cancels an inline rename and keeps the name") {
        editor_test::headless_editor editor = {};
        const std::string guid = editor.create_entity("Kept");
        hierarchy_panel panel = {};
        imgui_frame_scope scope(math::vec2(1280.0f, 2000.0f));
        draw_pinned(editor, panel, scope);
        panel.begin_rename(editor.context(), entity_of(editor, guid));
        for (int frame = 0; frame < 2; ++frame) {
            draw_pinned(editor, panel, scope);
        }
        ImGui::GetIO().AddInputCharactersUTF8("Discarded");
        draw_pinned(editor, panel, scope);
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
        draw_pinned(editor, panel, scope);
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
        draw_pinned(editor, panel, scope);
        CHECK_FALSE(panel.is_renaming());
        CHECK(editor.scene().name_of(entity_of(editor, guid)).view() == "Kept");
    }

    TEST_CASE("hierarchy_panel - F2 renames the primary selection") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("First");
        const std::string second = editor.create_entity("Second");
        hierarchy_panel panel = {};
        imgui_frame_scope scope(math::vec2(1280.0f, 2000.0f));
        for (int frame = 0; frame < 2; ++frame) {
            draw_pinned(editor, panel, scope);
        }
        // Clicking the row selects it and focuses the tree; F2 then works with the mouse away from every row.
        click(editor, panel, scope, row_point(1));
        REQUIRE(editor.context().selection().primary().to_string() == second);
        scope.set_mouse(row_point(1) + math::vec2(0.0f, 200.0f), false);
        draw_pinned(editor, panel, scope);
        ImGui::GetIO().AddKeyEvent(ImGuiKey_F2, true);
        draw_pinned(editor, panel, scope);
        ImGui::GetIO().AddKeyEvent(ImGuiKey_F2, false);
        draw_pinned(editor, panel, scope);
        CHECK(panel.is_renaming());
    }

    TEST_CASE("hierarchy_panel - shift-click selects the range from the anchor row") {
        editor_test::headless_editor editor = {};
        std::array<std::string, 5> guids = {};
        for (usize index = 0; index < guids.size(); ++index) {
            guids[index] = editor.create_entity(std::string("Row") + std::to_string(index));
        }
        hierarchy_panel panel = {};
        imgui_frame_scope scope(math::vec2(1280.0f, 2000.0f));
        for (int frame = 0; frame < 2; ++frame) {
            draw_pinned(editor, panel, scope);
        }
        click(editor, panel, scope, row_point(1));
        REQUIRE(editor.context().selection().size() == 1);
        ImGuiIO &io = ImGui::GetIO();
        io.AddKeyEvent(ImGuiMod_Shift, true);
        click(editor, panel, scope, row_point(3));
        io.AddKeyEvent(ImGuiMod_Shift, false);
        draw_pinned(editor, panel, scope);
        const selection_set &selection = editor.context().selection();
        CHECK(selection.size() == 3);
        for (usize index = 0; index < guids.size(); ++index) {
            CAPTURE(index);
            CHECK(selection.contains(*core::uuid::parse(guids[index])) == (index >= 1 && index <= 3));
        }
        // The clicked row becomes the primary; the anchor stays, so a second range starts from row 1 again.
        CHECK(selection.primary().to_string() == guids[3]);
        io.AddKeyEvent(ImGuiMod_Shift, true);
        click(editor, panel, scope, row_point(0));
        io.AddKeyEvent(ImGuiMod_Shift, false);
        draw_pinned(editor, panel, scope);
        CHECK(selection.size() == 2);
        CHECK(selection.contains(*core::uuid::parse(guids[0])));
        CHECK(selection.contains(*core::uuid::parse(guids[1])));
    }

    TEST_CASE("regression - a selection made outside the tree expands the selected entity's ancestors") {
        editor_test::headless_editor editor = {};
        const std::string root = editor.create_entity("Root");
        const std::string mid = editor.create_entity("Mid", root);
        const std::string leaf = editor.create_entity("Leaf", mid);
        hierarchy_panel panel = {};
        imgui_frame_scope scope(math::vec2(1280.0f, 2000.0f));
        draw_pinned(editor, panel, scope);
        REQUIRE(panel.cache().rows().size() == 1);
        select(editor, leaf);
        panel.update(editor.context());
        CHECK(panel.cache().is_expanded(editor.scene(), entity_of(editor, root)));
        CHECK(panel.cache().is_expanded(editor.scene(), entity_of(editor, mid)));
        CHECK(panel.cache().rows().size() == 3);
        draw_pinned(editor, panel, scope);
        CHECK(panel.rows_drawn() == 3);
    }

    TEST_CASE("hierarchy_panel - a Ctrl-click in the tree does not suppress the next outside reveal") {
        editor_test::headless_editor editor = {};
        (void) editor.create_entity("First");
        const std::string second = editor.create_entity("Second");
        const std::string mid = editor.create_entity("Mid", second);
        const std::string leaf = editor.create_entity("Leaf", mid);
        hierarchy_panel panel = {};
        imgui_frame_scope scope(math::vec2(1280.0f, 2000.0f));
        for (int frame = 0; frame < 2; ++frame) {
            draw_pinned(editor, panel, scope);
        }
        ImGuiIO &io = ImGui::GetIO();
        click(editor, panel, scope, row_point(0));
        io.AddKeyEvent(ctrl_modifier(), true);
        click(editor, panel, scope, row_point(1));
        // Ctrl-clicking the selected First again removes it and leaves the primary (Second) where it was.
        click(editor, panel, scope, row_point(0));
        io.AddKeyEvent(ctrl_modifier(), false);
        draw_pinned(editor, panel, scope);
        REQUIRE(editor.context().selection().size() == 1);
        REQUIRE(editor.context().selection().primary().to_string() == second);
        CHECK_FALSE(panel.cache().is_expanded(editor.scene(), entity_of(editor, mid)));
        select(editor, leaf);
        panel.update(editor.context());
        CHECK(panel.cache().is_expanded(editor.scene(), entity_of(editor, second)));
        CHECK(panel.cache().is_expanded(editor.scene(), entity_of(editor, mid)));
    }
}
