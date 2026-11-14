#pragma once

#include "conjure/editor/editor_context.h"
#include "conjure/editor/ui/editor_panel.h"
#include "conjure/editor/ui/gizmo_controller.h"

#include "conjure/core/api.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <memory>

// The viewport (docs/design/04_editor.md 3.8, 3.9): the scene through the editor camera, drawn every frame
// into an off-screen target the size of the panel and shown as an image (code sweep F104; without a window
// there is no ImGui renderer and the panel is the input surface alone), with the input surface over it: the
// gizmo for the selection, click picking through view.pick, right-drag orbit, middle-drag pan and wheel zoom
// of the editor camera, and the operation/space/snap toolbar. The picture, the pick ray and the gizmo all use
// the editor camera and the panel's rectangle, so what is clicked is what is seen.

namespace conjure::editor {
    class scene_view;
}

namespace conjure::editor::ui {

    class CONJURE_API viewport_panel final : public editor_panel {
        private:
            gizmo_controller mGizmo = {};
            // The off-screen picture of the scene (engine/src/editor/core/scene_view.h); it dies with the
            // panel, which the editor layer destroys before the renderer goes.
            std::unique_ptr<scene_view> mSceneView;
            const void *mTextureOwner = nullptr;    // the ImGui renderer mSceneTexture was registered with
            u64 mSceneTexture = 0;                  // ImGui texture id of the picture; 0 = none
            u64 mPicturesDrawn = 0;
            math::vec2 mRectMin = math::vec2(0.0f);
            math::vec2 mRectSize = math::vec2(1.0f);
            u32 mPicks = 0;
            bool mTargetFailureLogged = false;      // one WARN per run of failures to make the target

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
            /// Frames in which the panel showed the scene through the editor camera; stays 0 where no ImGui
            /// renderer exists to show it (headless, the null device).
            /// </summary>
            [[nodiscard]] u64 pictures_drawn() const noexcept {
                return mPicturesDrawn;
            }

            /// <summary>
            /// Picks at a viewport position (0..1) and updates the selection; public so tests drive it.
            /// </summary>
            void pick_at(editor_context &context, const math::vec2 &viewportUv, bool additive);

        private:
            void draw_toolbar(editor_context &context);
            void draw_snap_controls();
            void handle_camera_input(editor_context &context);

            /// <summary>
            /// Keeps the off-screen target at the rectangle's size in framebuffer pixels, submits the editor
            /// camera's view for this frame and draws the picture at the rectangle. False when nothing was
            /// drawn (no ImGui renderer, or the target could not be made).
            /// </summary>
            bool draw_scene_picture(editor_context &context, const math::vec2 &origin, const math::vec2 &size);
    };
}
