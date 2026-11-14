#pragma once

#include "conjure/core/math.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    #include "conjure/renderer/gpu_ids.h"
    #include "conjure/renderer/render_scene.h"
#endif

#include <memory>
#include <string_view>

// Scene views (docs/design/04_editor.md 3.8, 3.10; code sweep F104): the editor's own pictures of the scene,
// drawn into an off-screen render target instead of the swapchain. The viewport panel shows one through the
// editor camera, view.screenshot captures one through the camera it names. Every frame the owner submits one
// renderer::render_view; the renderer draws it with the scene's own views and before the ImGui pass, so a
// panel shows the picture in the frame it was drawn.
//
// Private to the editor libraries: the renderer is a private dependency of both, so none of this may appear
// in a public editor header. Without the renderer compiled in the header declares an empty scene_view, which
// keeps the owners' members and destructors identical in both builds.

namespace conjure::renderer {
    class renderer;
}

namespace conjure::editor {

    class editor_camera;
    class editor_context;
    class screenshot_backend;

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER

    /// <summary>
    /// The view that shows the scene through the editor camera into 'target': the camera's view matrix and a
    /// projection that equals editor_camera::projection_matrix for the target's aspect, so picking
    /// (editor_camera::screen_ray) and the gizmos, which use the camera's own matrices, agree with the picture.
    /// 'postProcess' false keeps authored colours (the 2D view mode).
    /// </summary>
    [[nodiscard]] renderer::render_view make_editor_camera_view(
      const editor_camera &camera,
      renderer::render_target_id target,
      bool postProcess
    );

    /// <summary>
    /// The view a screenshot's camera names, into 'target': "editor" is the editor camera (as the viewport
    /// shows it), "game" the scene camera the engine would show first (the main camera of the highest
    /// priority; any camera when none is main) and anything else the guid of an entity with a camera.
    /// SCENE_NOT_OPEN without a scene, COMPONENT_NOT_FOUND when "game" finds no camera or the entity has none,
    /// ENTITY_NOT_FOUND for a guid that names no live entity.
    /// </summary>
    [[nodiscard]] core::result<renderer::render_view> resolve_camera_view(
      editor_context &context,
      std::string_view camera,
      renderer::render_target_id target
    );

    /// <summary>
    /// A render target and the view drawn into it. The target belongs to one renderer; ensure() with another
    /// one (the engine was restarted) forgets the old target instead of destroying it through a dead renderer.
    /// The owner must destroy or release() the view while that renderer is alive.
    /// </summary>
    class scene_view {
        private:
            renderer::renderer *mRenderer = nullptr;
            renderer::render_target_id mTarget = {};
            math::uvec2 mSize = math::uvec2(0, 0);

        public:
            scene_view();
            ~scene_view();

            scene_view(const scene_view &) = delete;
            scene_view &operator=(const scene_view &) = delete;

        public:
            /// <summary>
            /// Makes the target exist at 'size' pixels (each side clamped to 1..the device's texture limit):
            /// created on first use, resized when the size differs, which waits for the GPU. The target id and
            /// its colour texture id survive a resize. After a failure the view has no target.
            /// </summary>
            [[nodiscard]] core::result<void> ensure(renderer::renderer &renderer, math::uvec2 size);

            /// <summary>
            /// Destroys the target; the view can be used again with ensure().
            /// </summary>
            void release() noexcept;

            /// <summary>
            /// Drops the target without destroying it, for an owner that cannot tell whether the renderer
            /// is still alive (the engine is restarting or shutting down): a renderer destroys every target
            /// it still holds when it goes.
            /// </summary>
            void forget() noexcept;

            /// <summary>
            /// Queues 'view' for the frame being built, with this view's target in place of its own. Nothing
            /// happens without a target.
            /// </summary>
            void submit(renderer::render_view view);

            [[nodiscard]] bool is_valid() const noexcept {
                return mTarget.is_valid();
            }

            [[nodiscard]] renderer::render_target_id target() const noexcept {
                return mTarget;
            }

            [[nodiscard]] math::uvec2 size() const noexcept {
                return mSize;
            }

            /// <summary>
            /// The renderer the target lives in; nullptr without a target.
            /// </summary>
            [[nodiscard]] renderer::renderer *owner() const noexcept {
                return mRenderer;
            }

            /// <summary>
            /// The target's colour texture (for imgui_renderer::register_texture); invalid without a target.
            /// </summary>
            [[nodiscard]] renderer::gpu_texture_id colour_texture() const noexcept;

            /// <summary>
            /// Frames rendered into the target since it was created or last resized; 0 without a target.
            /// </summary>
            [[nodiscard]] u64 frames_rendered() const noexcept;
    };

    /// <summary>
    /// The screenshot backend over a renderer: a scene_view of the requested size through the requested
    /// camera, read back with renderer::read_back. screenshot_service::create_renderer_backend returns it on a
    /// real device only; the null device reads back zeros, which is of use to tests and to nobody else.
    /// </summary>
    [[nodiscard]] std::unique_ptr<screenshot_backend> make_renderer_screenshot_backend(
      editor_context &context,
      renderer::renderer &target
    );

#else

    class scene_view {};

#endif
}
