#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/editor/play_controller.h"
#include "conjure/editor/ui/imgui_frame_scope.h"
#include "conjure/editor/ui/panels/toolbar_panel.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <vector>

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

    TEST_CASE("toolbar_panel - group separators are vertical bars in the button row") {
        editor_test::headless_editor editor = {};
        toolbar_panel panel = {};
        imgui_frame_scope scope(math::vec2(1280.0f, 720.0f));
        // A colour nothing else uses: the default style draws the window border in the separator colour.
        const ImU32 marker = IM_COL32(255, 0, 255, 255);
        for (int frame = 0; frame < 2; ++frame) {
            scope.begin_frame();
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(800.0f, 60.0f), ImGuiCond_Always);
            ImGui::PushStyleColor(ImGuiCol_Separator, marker);
            panel.draw(editor.context());
            ImGui::PopStyleColor();
            scope.end_frame();
        }
        const ImGuiWindow *window = ImGui::FindWindowByName("Toolbar");
        REQUIRE(window != nullptr);
        std::vector<ImVec2> corners = {};
        for (const ImDrawVert &vertex : window->DrawList->VtxBuffer) {
            if (vertex.col == marker) {
                corners.push_back(vertex.pos);
            }
        }
        // AddRectFilled writes four vertices per bar; two bars separate the three groups.
        REQUIRE(corners.size() == 8);
        for (usize bar = 0; bar < 2; ++bar) {
            CAPTURE(bar);
            f32 minX = corners[bar * 4].x;
            f32 maxX = minX;
            f32 minY = corners[bar * 4].y;
            f32 maxY = minY;
            for (usize corner = bar * 4; corner < bar * 4 + 4; ++corner) {
                minX = std::min(minX, corners[corner].x);
                maxX = std::max(maxX, corners[corner].x);
                minY = std::min(minY, corners[corner].y);
                maxY = std::max(maxY, corners[corner].y);
            }
            CHECK(maxX - minX <= 2.0f);
            CHECK(maxY - minY > 1.0f);
        }
    }
}
