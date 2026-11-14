#include "conjure/editor/ui/panels/viewport_panel.h"

#include "editor/core/scene_view.h"
#include "editor/ui/ui_support.h"

#include "conjure/editor/editor_camera.h"
#include "conjure/editor/log_category.h"
#include "conjure/editor/picker.h"
#include "conjure/editor/play_controller.h"
#include "conjure/editor/selection_set.h"

#include "conjure/core/log.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    #include "conjure/renderer/imgui_renderer.h"
    #include "conjure/renderer/renderer.h"
#endif

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
        constexpr ImU32 BACKDROP_COLOUR = IM_COL32(30, 31, 36, 255);    // behind the scene picture, opaque
    }

    viewport_panel::viewport_panel() :
        editor_panel("Viewport"),
        mSceneView(std::make_unique<scene_view>()) {}

    viewport_panel::~viewport_panel() {
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
        // The texture id first: it names the target's colour texture, which goes with the scene view.
        renderer::renderer *owner = mSceneView->owner();
        if (mSceneTexture != 0 && owner != nullptr && owner->imgui() == mTextureOwner) {
            owner->imgui()->unregister_texture(mSceneTexture);
        }
#endif
    }

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

        // The scene through the editor camera, then the input surface over the same rectangle.
        if (draw_scene_picture(context, mRectMin, mRectSize)) {
            ++mPicturesDrawn;
        }
        ImGui::SetCursorScreenPos(origin);
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

    bool viewport_panel::draw_scene_picture(editor_context &context, const math::vec2 &origin, const math::vec2 &size) {
#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
        renderer::renderer *renderer = context.renderer();
        renderer::imgui_renderer *backend = renderer != nullptr ? renderer->imgui() : nullptr;
        if (backend == nullptr) {
            // No window: nothing could show the picture, so none is drawn (view.screenshot renders its own).
            return false;
        }
        // The target is as large as the rectangle is on screen: ImGui lays out in logical units, the
        // framebuffer scale turns them into pixels.
        const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
        const math::uvec2 pixels(
          static_cast<u32>(std::max(std::lround(size.x * std::max(scale.x, 0.0f)), 1L)),
          static_cast<u32>(std::max(std::lround(size.y * std::max(scale.y, 0.0f)), 1L))
        );
        // Resizing waits for the GPU, so the target keeps its size while a mouse button is down: dragging a
        // splitter would otherwise stall every frame. The picture is stretched meanwhile and snaps to the
        // rectangle when the button goes up.
        const bool settled = !ImGui::IsMouseDown(ImGuiMouseButton_Left);
        if (!mSceneView->is_valid() || mSceneView->owner() != renderer || settled) {
            const core::result<void> sized = mSceneView->ensure(*renderer, pixels);
            if (!sized.has_value()) {
                if (!mTargetFailureLogged) {
                    mTargetFailureLogged = true;
                    CONJURE_LOG_WARN(
                      LOG_EDITOR,
                      "viewport: no {}x{} render target, the scene is not shown: {}",
                      pixels.x,
                      pixels.y,
                      sized.get_error().to_string()
                    );
                }
                return false;
            }
            mTargetFailureLogged = false;
        }
        // The colour texture's id survives a resize, so one registration lasts as long as the ImGui renderer.
        if (mSceneTexture == 0 || mTextureOwner != backend) {
            mSceneTexture = backend->register_texture(mSceneView->colour_texture());
            mTextureOwner = backend;
        }
        const bool twoD = context.current_view_mode() == editor_context::view_mode::TWO_D;
        mSceneView->submit(make_editor_camera_view(context.camera(), mSceneView->target(), !twoD));
        if (mSceneTexture == 0) {
            return false;
        }
        // The target is transparent where the scene drew nothing. An opaque backdrop keeps what lies behind
        // the panel from showing through the picture: the window's translucent background and, behind it,
        // whatever a scene camera drew into the back buffer.
        ImGui::GetWindowDrawList()->AddRectFilled(
          support::to_imvec(origin),
          support::to_imvec(origin + size),
          BACKDROP_COLOUR
        );
        ImGui::SetCursorScreenPos(support::to_imvec(origin));
        ImGui::Image(static_cast<ImTextureID>(mSceneTexture), support::to_imvec(size));
        return true;
#else
        (void) context;
        (void) origin;
        (void) size;
        return false;
#endif
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
