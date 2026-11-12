#include "conjure/editor/ui/panels/toolbar_panel.h"

#include "editor/ui/ui_support.h"

#include "conjure/editor/play_controller.h"
#include "conjure/editor/undo_stack.h"

#include <imgui.h>

#include <algorithm>
#include <format>

namespace conjure::editor::ui {

    toolbar_panel::toolbar_panel() :
        editor_panel("Toolbar") {}

    toolbar_panel::~toolbar_panel() = default;

    void toolbar_panel::draw_contents(editor_context &context) {
        const play_state state = context.play().state();
        const bool hasScene = context.has_scene();

        // The global shortcuts (Ctrl+Z, Ctrl+Y, Ctrl+S, F5, Shift+F5) live in editor_workspace::draw, so they work
        // while this panel is closed, collapsed or an unselected dock tab.
        ImGui::BeginDisabled(!hasScene || state != play_state::EDITING);
        if (ImGui::Button("Play")) {
            support::fire_command(context, "play.start", core::json::document::object());
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(state != play_state::PLAYING);
        if (ImGui::Button("Pause")) {
            support::fire_command(context, "play.pause", core::json::document::object());
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(state != play_state::PAUSED);
        if (ImGui::Button("Resume")) {
            support::fire_command(context, "play.resume", core::json::document::object());
        }
        ImGui::SameLine();
        if (ImGui::Button("Step")) {
            support::fire_command(context, "play.step", core::json::object({{"fixedSteps", std::max(mStepCount, 1)}}));
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        if (ImGui::InputInt("##steps", &mStepCount)) {
            mStepCount = std::clamp(mStepCount, 1, 100000);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(state == play_state::EDITING);
        if (ImGui::Button("Stop")) {
            support::fire_command(context, "play.stop", core::json::document::object());
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        support::vertical_separator();
        ImGui::SameLine();
        ImGui::BeginDisabled(!context.undo().can_undo() || context.undo().frozen());
        if (ImGui::Button("Undo")) {
            support::fire_command(context, "history.undo", core::json::document::object());
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!context.undo().can_redo() || context.undo().frozen());
        if (ImGui::Button("Redo")) {
            support::fire_command(context, "history.redo", core::json::document::object());
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        support::vertical_separator();
        ImGui::SameLine();
        const std::string status = std::format(
          "{} | frame {} | sim {}",
          to_string(state),
          context.frame_index(),
          context.play().simulation_frame()
        );
        ImGui::TextUnformatted(status.c_str());
        ImGui::SameLine();
        if (hasScene) {
            const std::string scene = std::format(
              "| {}{}",
              context.scene_path().empty() ? std::string("(unsaved scene)") : context.scene_path(),
              context.scene_dirty() ? "*" : ""
            );
            ImGui::TextUnformatted(scene.c_str());
        } else {
            support::hint_text("| no scene");
        }
    }
}
