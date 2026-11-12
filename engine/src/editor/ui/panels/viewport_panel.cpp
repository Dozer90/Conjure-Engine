#include "conjure/editor/ui/panels/viewport_panel.h"

#include "editor/ui/ui_support.h"

#include "conjure/editor/editor_camera.h"
#include "conjure/editor/picker.h"
#include "conjure/editor/play_controller.h"
#include "conjure/editor/selection_set.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <optional>
#include <string>
#include <utility>

namespace conjure::editor::ui {

    namespace {
        constexpr f32 ORBIT_RADIANS_PER_PIXEL = 0.005f;
        constexpr f32 ZOOM_PER_WHEEL_TICK = 0.9f;
        constexpr f32 SNAP_FIELD_WIDTH = 70.0f;
        constexpr f32 MIN_SNAP_STEP = 0.001f;           // ImGuizmo silently skips steps near zero
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
        // view.pick from a protocol client builds its ray with the shape the user sees.
        context.set_viewport_aspect(mRectSize.x / std::max(mRectSize.y, 1.0f));

        // An ordinary (opaque) window whose content area is an input surface; it does not show the scene through
        // the editor camera yet (code sweep 2026-11-12 B10).
        ImGui::InvisibleButton(
          "##viewport_surface",
          available,
          ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle
        );
        const bool hovered = ImGui::IsItemHovered();
        // A drag that started on the surface keeps the camera moving after the cursor leaves the panel.
        const bool active = ImGui::IsItemActive();
        if (hovered || active) {
            handle_camera_input(context);
        }
        // Drawn every frame: without a scene or a selection it returns false, ends a pending drag and clears its
        // hover state, so a hover left over from the last selection cannot block the next pick.
        const bool gizmoUsed = mGizmo.draw(context, mRectMin, mRectSize);
        if (
          hovered && !gizmoUsed && !mGizmo.is_over() && ImGui::IsMouseClicked(
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
        support::vertical_separator();
        ImGui::SameLine();
        if (ImGui::RadioButton("World", mGizmo.space() == gizmo_space::WORLD)) {
            mGizmo.set_space(gizmo_space::WORLD);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Local", mGizmo.space() == gizmo_space::LOCAL)) {
            mGizmo.set_space(gizmo_space::LOCAL);
        }
        ImGui::SameLine();
        draw_snap_controls();
        ImGui::SameLine();
        support::vertical_separator();
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

    void viewport_panel::draw_snap_controls() {
        bool snap = mGizmo.snap_enabled();
        f32 translate = mGizmo.snap_translate();
        f32 rotate = mGizmo.snap_rotate_degrees();
        f32 scale = mGizmo.snap_scale();
        bool changed = ImGui::Checkbox("Snap", &snap);
        if (snap) {
            // One step field, for the operation the gizmo shows (universal snaps with the translate step).
            ImGui::SameLine();
            ImGui::SetNextItemWidth(SNAP_FIELD_WIDTH);
            const ImGuiSliderFlags clamp = ImGuiSliderFlags_AlwaysClamp;
            bool stepChanged = false;
            if (mGizmo.operation() == gizmo_operation::ROTATE) {
                stepChanged = ImGui::DragFloat("##snap_step", &rotate, 1.0f, 1.0f, 180.0f, "%.0f deg", clamp);
            } else if (mGizmo.operation() == gizmo_operation::SCALE) {
                stepChanged = ImGui::DragFloat("##snap_step", &scale, 0.01f, 0.01f, 10.0f, "%.2f", clamp);
            } else {
                stepChanged = ImGui::DragFloat("##snap_step", &translate, 0.05f, 0.01f, 100.0f, "%.2f", clamp);
            }
            changed = changed || stepChanged;
        }
        if (changed) {
            mGizmo.set_snap(
              snap,
              std::max(translate, MIN_SNAP_STEP),
              std::max(rotate, MIN_SNAP_STEP),
              std::max(scale, MIN_SNAP_STEP)
            );
        }
    }

    void viewport_panel::handle_camera_input(editor_context &context) {
        editor_camera &camera = context.camera();
        const ImGuiIO &io = ImGui::GetIO();
        // The scene under the cursor follows it at any zoom.
        const f32 unitsPerPixel = camera.world_units_per_pixel(mRectSize.y);
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            const ImVec2 delta = io.MouseDelta;
            if (context.current_view_mode() == editor_context::view_mode::TWO_D) {
                camera.pan(math::vec2(-delta.x * unitsPerPixel, delta.y * unitsPerPixel));
            } else {
                camera.orbit(-delta.x * ORBIT_RADIANS_PER_PIXEL, -delta.y * ORBIT_RADIANS_PER_PIXEL);
            }
        }
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            const ImVec2 delta = io.MouseDelta;
            camera.pan(math::vec2(-delta.x * unitsPerPixel, delta.y * unitsPerPixel));
        }
        if (std::abs(io.MouseWheel) > 0.0f) {
            camera.zoom(std::pow(ZOOM_PER_WHEEL_TICK, io.MouseWheel));
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F) && context.has_scene()) {
            support::fire_command(context, "view.focus", core::json::document::object());
        }
    }
}
