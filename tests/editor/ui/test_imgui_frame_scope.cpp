#include "conjure/testing/doctest.h"

#include "conjure/core/math.h"
#include "conjure/editor/ui/imgui_frame_scope.h"

#include <imgui.h>

// imgui_frame_scope.h: a self-contained ImGui frame for UI code without the engine's imgui_layer.

using namespace conjure;
using namespace conjure::editor::ui;

TEST_SUITE("editor/ui/imgui_frame_scope") {

    TEST_CASE("imgui_frame_scope - creates a context when none exists and destroys it afterwards") {
        CHECK(ImGui::GetCurrentContext() == nullptr);
        {
            imgui_frame_scope scope(math::vec2(640.0f, 480.0f));
            CHECK(scope.owns_context());
            CHECK(scope.context() != nullptr);
            CHECK(ImGui::GetCurrentContext() == scope.context());
            CHECK_FALSE(scope.is_frame_open());
            CHECK(ImGui::GetIO().Fonts->IsBuilt());
            CHECK((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable) != 0);
        }
        CHECK(ImGui::GetCurrentContext() == nullptr);
    }

    TEST_CASE("imgui_frame_scope - begin_frame and end_frame bracket frames and count them") {
        imgui_frame_scope scope{};
        scope.begin_frame();
        CHECK(scope.is_frame_open());
        scope.begin_frame();
        CHECK(ImGui::Begin("probe"));
        ImGui::TextUnformatted("hello");
        ImGui::End();
        scope.end_frame();
        CHECK_FALSE(scope.is_frame_open());
        CHECK(scope.frame_count() == 1);
        scope.end_frame();
        CHECK(scope.frame_count() == 1);
        scope.begin_frame();
        scope.end_frame();
        CHECK(scope.frame_count() == 2);
        CHECK(ImGui::GetIO().DisplaySize.x == doctest::Approx(1280.0f));
    }

    TEST_CASE("imgui_frame_scope - a nested scope reuses the existing context and leaves it alive") {
        imgui_frame_scope outer{};
        {
            imgui_frame_scope inner(math::vec2(100.0f, 100.0f));
            CHECK_FALSE(inner.owns_context());
            CHECK(inner.context() == outer.context());
            inner.begin_frame();
            inner.end_frame();
        }
        CHECK(ImGui::GetCurrentContext() == outer.context());
        outer.begin_frame();
        outer.end_frame();
        CHECK(outer.frame_count() == 1);
    }

    TEST_CASE("imgui_frame_scope - set_mouse feeds the IO queue") {
        imgui_frame_scope scope{};
        scope.set_mouse(math::vec2(12.0f, 34.0f), true);
        scope.begin_frame();
        CHECK(ImGui::GetIO().MousePos.x == doctest::Approx(12.0f));
        CHECK(ImGui::GetIO().MousePos.y == doctest::Approx(34.0f));
        CHECK(ImGui::IsMouseDown(ImGuiMouseButton_Left));
        scope.end_frame();
        scope.set_mouse(math::vec2(12.0f, 34.0f), false);
        scope.begin_frame();
        CHECK_FALSE(ImGui::IsMouseDown(ImGuiMouseButton_Left));
        scope.end_frame();
        // An open frame is closed by the destructor.
        scope.begin_frame();
    }
}
