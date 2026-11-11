#pragma once

#include "conjure/editor/editor_context.h"

#include "conjure/core/api.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"

#include <optional>
#include <string_view>
#include <vector>

// Transform gizmos over ImGuizmo (docs/design/04_editor.md 3.9): the first manipulated frame opens a
// transaction and caches the selected entities' world matrices; every frame with a delta dispatches
// component.set_many on the local transforms (through the parents' inverse world matrices, pivoting at the
// centroid) with the drag session as coalesce key; the release ends the transaction.

namespace conjure::editor::ui {

    enum class gizmo_operation : u8 {
        TRANSLATE = 0,
        ROTATE,
        SCALE,
        UNIVERSAL
    };

    enum class gizmo_space : u8 {
        LOCAL = 0,
        WORLD
    };

    /// <summary>
    /// The wire name of an operation ("translate").
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(gizmo_operation operation) noexcept;

    class CONJURE_API gizmo_controller {
        private:
            struct dragged_entity {
                core::uuid guid = {};
                math::mat4 worldMatrix = math::mat4(1.0f);
            };

            std::vector<dragged_entity> mDragged = {};
            math::mat4 mPivot = math::mat4(1.0f);
            gizmo_operation mOperation = gizmo_operation::TRANSLATE;
            gizmo_space mSpace = gizmo_space::WORLD;
            u64 mDragSession = 0;
            u32 mManipulations = 0;
            f32 mSnapTranslate = 0.0f;
            f32 mSnapRotateDegrees = 0.0f;
            f32 mSnapScale = 0.0f;
            bool mDragging = false;
            bool mUseSnap = false;

        public:
            gizmo_controller();
            ~gizmo_controller();

            gizmo_controller(const gizmo_controller &) = delete;
            gizmo_controller &operator=(const gizmo_controller &) = delete;

        public:
            /// <summary>
            /// Draws the gizmo for the current selection inside the viewport rectangle and applies the user's
            /// manipulation. Call once per frame between ImGui::Begin and End of the viewport window.
            /// Returns true while a drag is in progress.
            /// </summary>
            bool draw(editor_context &context, const math::vec2 &rectMin, const math::vec2 &rectSize);

            /// <summary>
            /// Applies a delta matrix to the selection as if the gizmo produced it (tests, keyboard nudges):
            /// begins the drag when none is open.
            /// </summary>
            void apply_delta(editor_context &context, const math::mat4 &deltaWorld);

            /// <summary>
            /// Ends an open drag (release).
            /// </summary>
            void end_drag(editor_context &context);

            void set_operation(gizmo_operation operation) noexcept {
                mOperation = operation;
            }

            [[nodiscard]] gizmo_operation operation() const noexcept {
                return mOperation;
            }

            void set_space(gizmo_space space) noexcept {
                mSpace = space;
            }

            [[nodiscard]] gizmo_space space() const noexcept {
                return mSpace;
            }

            void set_snap(bool enabled, f32 translate, f32 rotateDegrees, f32 scale) noexcept;

            [[nodiscard]] bool is_dragging() const noexcept {
                return mDragging;
            }

            [[nodiscard]] u32 manipulation_count() const noexcept {
                return mManipulations;
            }

        private:
            void begin_drag(editor_context &context);
            [[nodiscard]] bool gather_selection(editor_context &context);
    };
}
