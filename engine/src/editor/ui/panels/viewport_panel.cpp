#include "conjure/editor/ui/panels/viewport_panel.h"

#include "editor/ui/ui_support.h"

#include "conjure/editor/editor_camera.h"
#include "conjure/editor/picker.h"
#include "conjure/editor/play_controller.h"
#include "conjure/editor/selection_set.h"

#include <ImGuizmo.h>
#include <imgui.h>

#include <array>
#include <cmath>
#include <format>

namespace conjure::editor::ui {

    namespace {
        constexpr f32 ORBIT_RADIANS_PER_PIXEL = 0.005f;
        constexpr f32 PAN_UNITS_PER_PIXEL = 0.01f;
        constexpr f32 ZOOM_PER_WHEEL_TICK = 0.9f;
    }

    viewport_panel::viewport_panel() :
        editor_panel("Viewport") {}

    viewport_panel::~viewport_panel() = default;

    void viewport_panel::draw_contents(editor_context &context) {
        draw_toolbar(context);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImVec2 available = ImGui::GetContentRegionAvail();
        available.x = std::max(available.x, 1.0f);
        available.y = std::max(available.y, 1.0f);
        mRectMin = support::from_imvec(origin);
        mRectSize = support::from_imvec(available);

        // The scene renders underneath the pass-through dock node; the viewport itself is an input surface.
        ImGui::InvisibleButton(
          "##viewport_surface",
          available,
          ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle
        );
        const bool hovered = ImGui::IsItemHovered();
        if (hovered) {
            handle_camera_input(context);
        }
        bool gizmoUsed = false;
        if (context.has_scene() && !context.selection().empty()) {
            gizmoUsed = mGizmo.draw(context, mRectMin, mRectSize);
        }
        if (
          hovered && !gizmoUsed && !ImGuizmo::IsOver() && ImGui::IsMouseClicked(
            ImGuiMouseButton_Left
          ) && context.has_scene()
        ) {
            const ImVec2 mouse = ImGui::GetMousePos();
            const math::vec2 uv = math::vec2((mouse.x - origin.x) / available.x, (mouse.y - origin.y) / available.y);
            pick_at(context, uv, ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift);
        }

        // Overlay text in the corner: camera, mode and play state.
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        const editor_camera &camera = context.camera();
        const std::string overlay = std::format(
          "{} | {} | cam ({:.1f}, {:.1f}, {:.1f}) | {} {}",
          context.current_view_mode() == editor_context::view_mode::TWO_D ? "2D" : "3D",
          to_string(context.play().state()),
          camera.position().x,
          camera.position().y,
          camera.position().z,
          to_string(mGizmo.operation()),
          mGizmo.space() == gizmo_space::LOCAL ? "local" : "world"
        );
        drawList->AddText(ImVec2(origin.x + 6.0f, origin.y + 4.0f), IM_COL32(230, 230, 230, 220), overlay.c_str());
        if (context.debug_draw().grid && !context.has_scene()) {
            drawList->AddText(ImVec2(origin.x + 6.0f, origin.y + 22.0f), IM_COL32(180, 180, 180, 200), "no scene open");
        }
    }

    void viewport_panel::pick_at(editor_context &context, const math::vec2 &viewportUv, bool additive) {
        ++mPicks;
        core::json::document args = core::json::object({{"x", viewportUv.x}, {"y", viewportUv.y}});
        args.set("aspect", mRectSize.x / std::max(mRectSize.y, 1.0f));
        const core::result<core::json::document> result = support::run_command(context, "view.pick", std::move(args));
        if (!result.has_value()) {
            return;
        }
        const core::json::value root = result.value().root();
        if (!root["hit"].as_bool(false)) {
            if (!additive) {
                context.selection().clear();
            }
            return;
        }
        const std::optional<core::uuid> guid = core::uuid::parse(root["guid"].as_string(""));
        if (!guid.has_value()) {
            return;
        }
        const std::array<core::uuid, 1> guids = {*guid};
        if (!additive) {
            context.selection().set(guids);
        } else if (context.selection().contains(*guid)) {
            context.selection().remove(guids);
        } else {
            context.selection().add(guids);
        }
        context.selection().rebuild(context.scene());
    }

    void viewport_panel::draw_toolbar(editor_context &context) {
        const std::array<std::pair<const char *, gizmo_operation>, 4> operations = {
            std::pair{"Move", gizmo_operation::TRANSLATE},
            std::pair{"Rotate", gizmo_operation::ROTATE},
            std::pair{"Scale", gizmo_operation::SCALE},
            std::pair{"All", gizmo_operation::UNIVERSAL},
        };
        for (const auto &[label, operation] : operations) {
            if (ImGui::RadioButton(label, mGizmo.operation() == operation)) {
                mGizmo.set_operation(operation);
            }
            ImGui::SameLine();
        }
        ImGui::Separator();
        ImGui::SameLine();
        if (ImGui::RadioButton("World", mGizmo.space() == gizmo_space::WORLD)) {
            mGizmo.set_space(gizmo_space::WORLD);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Local", mGizmo.space() == gizmo_space::LOCAL)) {
            mGizmo.set_space(gizmo_space::LOCAL);
        }
        ImGui::SameLine();
        ImGui::Separator();
        ImGui::SameLine();
        const bool twoD = context.current_view_mode() == editor_context::view_mode::TWO_D;
        if (ImGui::Button(twoD ? "2D" : "3D")) {
            support::fire_command(context, "view.set_mode", core::json::object({{"mode", twoD ? "3d" : "2d"}}));
        }
        ImGui::SameLine();
        if (ImGui::Button("Focus") && context.has_scene()) {
            support::fire_command(context, "view.focus", core::json::document::object());
        }
        ImGui::SameLine();
        editor_context::debug_draw_flags &flags = context.debug_draw();
        ImGui::Checkbox("Grid", &flags.grid);
        ImGui::SameLine();
        ImGui::Checkbox("Bounds", &flags.bounds);
        ImGui::SameLine();
        ImGui::Checkbox("Physics", &flags.physics);
        if (ImGui::Shortcut(ImGuiKey_W)) {
            mGizmo.set_operation(gizmo_operation::TRANSLATE);
        }
        if (ImGui::Shortcut(ImGuiKey_E)) {
            mGizmo.set_operation(gizmo_operation::ROTATE);
        }
        if (ImGui::Shortcut(ImGuiKey_R)) {
            mGizmo.set_operation(gizmo_operation::SCALE);
        }
    }

    void viewport_panel::handle_camera_input(editor_context &context) {
        editor_camera &camera = context.camera();
        const ImGuiIO &io = ImGui::GetIO();
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            const ImVec2 delta = io.MouseDelta;
            if (context.current_view_mode() == editor_context::view_mode::TWO_D) {
                camera.pan(math::vec2(-delta.x * PAN_UNITS_PER_PIXEL, delta.y * PAN_UNITS_PER_PIXEL));
            } else {
                camera.orbit(-delta.x * ORBIT_RADIANS_PER_PIXEL, -delta.y * ORBIT_RADIANS_PER_PIXEL);
            }
        }
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            const ImVec2 delta = io.MouseDelta;
            camera.pan(math::vec2(-delta.x * PAN_UNITS_PER_PIXEL, delta.y * PAN_UNITS_PER_PIXEL));
        }
        if (std::abs(io.MouseWheel) > 0.0f) {
            camera.zoom(std::pow(ZOOM_PER_WHEEL_TICK, io.MouseWheel));
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F) && context.has_scene()) {
            support::fire_command(context, "view.focus", core::json::document::object());
        }
    }
}
