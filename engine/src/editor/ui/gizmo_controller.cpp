#include "conjure/editor/ui/gizmo_controller.h"

#include "editor/ui/ui_support.h"

#include "conjure/editor/editor_camera.h"
#include "conjure/editor/log_category.h"
#include "conjure/editor/selection_set.h"

#include <ImGuizmo.h>
#include <imgui.h>

#include <array>
#include <cmath>
#include <format>
#include <utility>

namespace conjure::editor::ui {

    namespace {
        [[nodiscard]] ImGuizmo::OPERATION to_imguizmo(gizmo_operation operation) noexcept {
            switch (operation) {
                case gizmo_operation::TRANSLATE:
                    return ImGuizmo::TRANSLATE;
                case gizmo_operation::ROTATE:
                    return ImGuizmo::ROTATE;
                case gizmo_operation::SCALE:
                    return ImGuizmo::SCALE;
                case gizmo_operation::UNIVERSAL:
                    return ImGuizmo::UNIVERSAL;
            }
            return ImGuizmo::TRANSLATE;
        }

        [[nodiscard]] core::json::document vec3_json(const math::vec3 &value) {
            return core::json::array({value.x, value.y, value.z});
        }

        [[nodiscard]] core::json::document quat_json(const math::quat &value) {
            return core::json::array({value.x, value.y, value.z, value.w});
        }

        [[nodiscard]] bool is_identity(const math::mat4 &matrix) noexcept {
            const math::mat4 identity = math::mat4(1.0f);
            for (glm::length_t column = 0; column < 4; ++column) {
                for (glm::length_t row = 0; row < 4; ++row) {
                    if (std::abs(matrix[column][row] - identity[column][row]) > 1.0e-6f) {
                        return false;
                    }
                }
            }
            return true;
        }
    }

    std::string_view to_string(gizmo_operation operation) noexcept {
        switch (operation) {
            case gizmo_operation::TRANSLATE:
                return "translate";
            case gizmo_operation::ROTATE:
                return "rotate";
            case gizmo_operation::SCALE:
                return "scale";
            case gizmo_operation::UNIVERSAL:
                return "universal";
        }
        return "translate";
    }

    gizmo_controller::gizmo_controller() = default;
    gizmo_controller::~gizmo_controller() = default;

    void gizmo_controller::set_snap(bool enabled, f32 translate, f32 rotateDegrees, f32 scale) noexcept {
        mUseSnap = enabled;
        mSnapTranslate = translate;
        mSnapRotateDegrees = rotateDegrees;
        mSnapScale = scale;
    }

    bool gizmo_controller::draw(editor_context &context, const math::vec2 &rectMin, const math::vec2 &rectSize) {
        if (!support::frame_is_open() || rectSize.x <= 1.0f || rectSize.y <= 1.0f) {
            return false;
        }
        if (!context.has_scene() || context.selection().empty()) {
            if (mDragging) {
                end_drag(context);
            }
            return false;
        }
        if (!gather_selection(context)) {
            return false;
        }
        ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
        ImGuizmo::BeginFrame();
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
        ImGuizmo::SetRect(rectMin.x, rectMin.y, rectSize.x, rectSize.y);
        ImGuizmo::SetOrthographic(context.camera().is_orthographic());
        const math::mat4 view = context.camera().view_matrix();
        const math::mat4 projection = context.camera().projection_matrix(rectSize.x / rectSize.y);
        math::mat4 pivot = mPivot;
        math::mat4 delta = math::mat4(1.0f);
        std::array<f32, 3> snap = {mSnapTranslate, mSnapTranslate, mSnapTranslate};
        if (mOperation == gizmo_operation::ROTATE) {
            snap = {mSnapRotateDegrees, mSnapRotateDegrees, mSnapRotateDegrees};
        } else if (mOperation == gizmo_operation::SCALE) {
            snap = {mSnapScale, mSnapScale, mSnapScale};
        }
        const bool manipulated = ImGuizmo::Manipulate(
          &view[0][0],
          &projection[0][0],
          to_imguizmo(mOperation),
          mSpace == gizmo_space::LOCAL ? ImGuizmo::LOCAL : ImGuizmo::WORLD,
          &pivot[0][0],
          &delta[0][0],
          mUseSnap ? snap.data() : nullptr
        );
        const bool inUse = ImGuizmo::IsUsing();
        if (inUse && !mDragging) {
            begin_drag(context);
        }
        if (manipulated && mDragging && !is_identity(delta)) {
            mPivot = pivot;
            apply_delta(context, delta);
        }
        if (!inUse && mDragging) {
            end_drag(context);
        }
        return manipulated;
    }

    void gizmo_controller::apply_delta(editor_context &context, const math::mat4 &deltaWorld) {
        if (mDragged.empty() || !context.has_scene()) {
            return;
        }
        scene::scene &scene = context.scene();
        core::json::document sets = core::json::document::array();
        for (dragged_entity &dragged : mDragged) {
            const ecs::entity entity = scene.find_guid(dragged.guid);
            if (entity.is_null()) {
                continue;
            }
            dragged.worldMatrix = deltaWorld * dragged.worldMatrix;
            math::mat4 localMatrix = dragged.worldMatrix;
            const ecs::entity parent = scene.tree().parent_of(entity);
            if (!parent.is_null()) {
                const std::optional<math::transform> parentWorld = scene.world_transform_of(parent);
                if (parentWorld.has_value()) {
                    localMatrix = glm::inverse(parentWorld->to_matrix()) * dragged.worldMatrix;
                }
            }
            const math::transform local = math::transform::from_matrix(localMatrix);
            const std::string guid = dragged.guid.to_string();
            sets.push_back(core::json::object({
              {"entity", guid},
              {"path", "transform.position"},
              {"value", vec3_json(local.position)},
            }));
            if (mOperation != gizmo_operation::TRANSLATE) {
                sets.push_back(core::json::object({
                  {"entity", guid},
                  {"path", "transform.rotation"},
                  {"value", quat_json(local.rotation)},
                }));
                sets.push_back(core::json::object({
                  {"entity", guid},
                  {"path", "transform.scale"},
                  {"value", vec3_json(local.scale)},
                }));
            }
        }
        if (sets.root().size() == 0) {
            return;
        }
        ++mManipulations;
        (void) support::run_command(context, "component.set_many", core::json::object({{"sets", sets}}), mDragSession);
    }

    void gizmo_controller::end_drag(editor_context &context) {
        if (!mDragging) {
            return;
        }
        mDragging = false;
        mDragged.clear();
        support::fire_command(context, "editor.end_transaction", core::json::document::object());
    }

    void gizmo_controller::begin_drag(editor_context &context) {
        mDragging = true;
        ++mDragSession;
        const std::string label = std::format(
          "{} {}",
          to_string(mOperation),
          mDragged.size() == 1 ? "entity" : "entities"
        );
        support::fire_command(context, "editor.begin_transaction", core::json::object({{"label", label}}));
    }

    bool gizmo_controller::gather_selection(editor_context &context) {
        if (mDragging) {
            // The cached matrices move with the drag; the pivot is what ImGuizmo edits.
            return !mDragged.empty();
        }
        scene::scene &scene = context.scene();
        mDragged.clear();
        math::vec3 centroid = math::vec3(0.0f);
        math::quat rotation = math::quat(1.0f, 0.0f, 0.0f, 0.0f);
        for (const core::uuid &guid : context.selection().guids()) {
            const ecs::entity entity = scene.find_guid(guid);
            if (entity.is_null()) {
                continue;
            }
            const std::optional<math::transform> world = scene.world_transform_of(entity);
            if (!world.has_value()) {
                continue;
            }
            dragged_entity dragged = {};
            dragged.guid = guid;
            dragged.worldMatrix = world->to_matrix();
            mDragged.push_back(dragged);
            centroid += world->position;
            rotation = world->rotation;
        }
        if (mDragged.empty()) {
            return false;
        }
        centroid /= static_cast<f32>(mDragged.size());
        math::transform pivot = {};
        pivot.position = centroid;
        if (mDragged.size() == 1 && mSpace == gizmo_space::LOCAL) {
            pivot.rotation = rotation;
        }
        mPivot = pivot.to_matrix();
        return true;
    }
}
