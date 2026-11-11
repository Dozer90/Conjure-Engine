#pragma once

#include "conjure/editor/editor_context.h"
#include "conjure/editor/ui/editor_panel.h"
#include "conjure/editor/ui/gizmo_controller.h"

#include "conjure/core/api.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"

// The viewport (docs/design/04_editor.md 3.8, 3.9): a transparent window over the dockspace's pass-through
// central node (the scene renders underneath), the gizmo for the selection, click picking through view.pick,
// right-drag orbit, middle-drag pan and wheel zoom of the editor camera, and the operation/space toolbar.

namespace conjure::editor::ui {

    class CONJURE_API viewport_panel final : public editor_panel {
        private:
            gizmo_controller mGizmo = {};
            math::vec2 mRectMin = math::vec2(0.0f);
            math::vec2 mRectSize = math::vec2(1.0f);
            u32 mPicks = 0;

        public:
            viewport_panel();
            ~viewport_panel() override;

        public:
            void draw_contents(editor_context &context) override;

            [[nodiscard]] gizmo_controller &gizmo() noexcept {
                return mGizmo;
            }

            /// <summary>
            /// The viewport rectangle of the last frame (screen pixels).
            /// </summary>
            [[nodiscard]] const math::vec2 &rect_min() const noexcept {
                return mRectMin;
            }

            [[nodiscard]] const math::vec2 &rect_size() const noexcept {
                return mRectSize;
            }

            [[nodiscard]] u32 pick_count() const noexcept {
                return mPicks;
            }

            /// <summary>
            /// Picks at a viewport position (0..1) and updates the selection; public so tests drive it.
            /// </summary>
            void pick_at(editor_context &context, const math::vec2 &viewportUv, bool additive);

        private:
            void draw_toolbar(editor_context &context);
            void handle_camera_input(editor_context &context);
    };
}
