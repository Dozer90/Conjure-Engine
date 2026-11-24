#include "editor/core/scene_view.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER

#include "conjure/editor/editor_camera.h"
#include "conjure/editor/editor_context.h"
#include "conjure/editor/editor_error.h"
#include "conjure/editor/entity_ref.h"
#include "conjure/editor/screenshot_service.h"

#include "conjure/core/json/document.h"
#include "conjure/core/string_id.h"
#include "conjure/ecs/component_registry.h"
#include "conjure/ecs/view.h"
#include "conjure/ecs/world.h"
#include "conjure/renderer/camera.h"
#include "conjure/renderer/components.h"
#include "conjure/renderer/render_target.h"
#include "conjure/renderer/renderer.h"
#include "conjure/rhi/device.h"
#include "conjure/scene/components.h"
#include "conjure/scene/scene.h"

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <utility>

namespace conjure::editor {

    namespace {
        /// <summary>
        /// The entity's camera component, or nullptr. The renderer components are named in the ECS by the
        /// engine module, so the editor reaches them by registered id, as the picker does.
        /// </summary>
        const renderer::camera_component *camera_of(
          const ecs::world &world,
          ecs::entity entity,
          ecs::component_id cameraId
        ) {
            if (!cameraId.is_valid() || !world.has_raw(entity, cameraId)) {
                return nullptr;
            }
            return static_cast<const renderer::camera_component *>(world.get_raw(entity, cameraId));
        }

        /// <summary>
        /// The scene camera the engine's extraction would show first: among the enabled cameras, the main
        /// ones in priority order (every camera when none is main), world order breaking ties.
        /// </summary>
        std::optional<renderer::render_view> first_scene_camera_view(
          ecs::world &world,
          ecs::component_id cameraId,
          renderer::render_target_id target
        ) {
            std::optional<renderer::render_view> best = std::nullopt;
            i32 bestPriority = 0;
            bool bestIsMain = false;
            world.view<const scene::render_world_transform>()
              .without<scene::disabled>()
              .without<scene::inactive>()
              .for_each([&](ecs::entity entity, const scene::render_world_transform &transform) {
                  const renderer::camera_component *camera = camera_of(world, entity, cameraId);
                  if (camera == nullptr) {
                      return;
                  }
                  const bool better = !best.has_value() || (camera->isMain && !bestIsMain) ||
                    (camera->isMain == bestIsMain && camera->priority > bestPriority);
                  if (better) {
                      best = renderer::make_camera_view(*camera, transform.matrix, target);
                      bestPriority = camera->priority;
                      bestIsMain = camera->isMain;
                  }
              });
            return best;
        }

        /// <summary>
        /// view.screenshot through the renderer: a scene_view of the requested size, the view of the requested
        /// camera submitted every frame the request waits, and a read-back once a frame has been drawn into it.
        /// </summary>
        class renderer_screenshot_backend final : public screenshot_backend {
            private:
                editor_context *mContext = nullptr;
                renderer::renderer *mRenderer = nullptr;
                scene_view mView = {};
                u64 mFramesSeen = 0;        // frames_rendered() the last frame_rendered() answered for
                u64 mSubmittedFrame = 0;    // renderer::frame_count() when the view was last queued

            public:
                renderer_screenshot_backend(editor_context &context, renderer::renderer &target) noexcept :
                    mContext(&context),
                    mRenderer(&target) {}

                ~renderer_screenshot_backend() override = default;

            public:
                core::result<void> begin(const screenshot_request &request) override {
                    // The service takes one picture at a time, so nothing is in flight here; a target left by
                    // a request that was begun and never captured goes first.
                    mView.release();
                    CONJURE_TRY(mView.ensure(*mRenderer, math::uvec2(request.width, request.height)));
                    mFramesSeen = 0;
                    // The camera must resolve now, so a request for a camera that does not exist fails when it
                    // is made instead of producing an empty picture.
                    core::result<renderer::render_view> view =
                      resolve_camera_view(*mContext, request.camera, mView.target());
                    if (!view.has_value()) {
                        mView.release();
                        return view.get_error();
                    }
                    mView.submit(view.value());
                    mSubmittedFrame = mRenderer->frame_count();
                    return core::result<void>::success();
                }

                void prepare_frame(const screenshot_request &request) override {
                    // Once per rendered frame: a request made over the protocol is ticked in the frame that
                    // begin() already queued the view for, and a second copy would draw the scene twice.
                    if (!mView.is_valid() || mSubmittedFrame == mRenderer->frame_count()) {
                        return;
                    }
                    // A camera that went away while the request waited leaves the last picture in the target.
                    const core::result<renderer::render_view> view =
                      resolve_camera_view(*mContext, request.camera, mView.target());
                    if (view.has_value()) {
                        mView.submit(view.value());
                        mSubmittedFrame = mRenderer->frame_count();
                    }
                }

                bool frame_rendered(const screenshot_request &request) override {
                    (void) request;
                    const u64 rendered = mView.frames_rendered();
                    if (rendered == mFramesSeen) {
                        return false;
                    }
                    mFramesSeen = rendered;
                    return true;
                }

                core::result<core::image> capture(const screenshot_request &request) override {
                    (void) request;
                    if (!mView.is_valid()) {
                        return core::make_error(protocol_error_code::INTERNAL, "no render target to read back");
                    }
                    core::result<core::image> image = mRenderer->read_back(mView.target());
                    mView.release();
                    return image;
                }

                void abandon() noexcept override {
                    mView.forget();
                }
        };
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // Views

    renderer::render_view make_editor_camera_view(
      const editor_camera &camera,
      renderer::render_target_id target,
      bool postProcess
    ) {
        renderer::render_view view = {};
        view.cameraData.projection = camera.is_orthographic()
          ? renderer::projection_type::ORTHOGRAPHIC
          : renderer::projection_type::PERSPECTIVE;
        view.cameraData.verticalFovRadians = camera.vertical_fov();
        view.cameraData.orthographicHeight = camera.orthographic_height();
        view.cameraData.nearPlane = camera.near_plane();
        view.cameraData.farPlane = camera.far_plane();
        view.viewFromWorld = camera.view_matrix();
        view.cameraPosition = camera.position();
        view.target = target;
        // The debug overlays (grid, bounds, physics shapes) belong to the editor's picture of the scene.
        view.flags = renderer::view_flags::DRAW_DEBUG;
        if (!postProcess) {
            view.flags |= renderer::view_flags::NO_POST_PROCESS;
        }
        return view;
    }

    core::result<renderer::render_view> resolve_camera_view(
      editor_context &context,
      std::string_view camera,
      renderer::render_target_id target
    ) {
        if (camera == "editor") {
            const bool twoD = context.current_view_mode() == editor_context::view_mode::TWO_D;
            return make_editor_camera_view(context.camera(), target, !twoD);
        }
        if (!context.has_scene()) {
            return core::make_error(protocol_error_code::SCENE_NOT_OPEN, "no scene is open");
        }
        ecs::world &world = context.world();
        const ecs::component_id cameraId = context.components().find(
          core::string_id(renderer::CAMERA_COMPONENT_TYPE_NAME)
        );
        if (camera == "game") {
            const std::optional<renderer::render_view> view = first_scene_camera_view(world, cameraId, target);
            if (!view.has_value()) {
                return core::make_error(
                  protocol_error_code::COMPONENT_NOT_FOUND,
                  "camera \"game\": the scene has no enabled camera"
                );
            }
            return *view;
        }
        const core::json::document reference = core::json::object({{"camera", camera}});
        CONJURE_TRY_ASSIGN(
          const ecs::entity entity,
          entity_ref::resolve(reference.root()["camera"], context.scene())
        );
        const renderer::camera_component *component = camera_of(world, entity, cameraId);
        const scene::render_world_transform *transform = world.get<scene::render_world_transform>(entity);
        if (component == nullptr || transform == nullptr) {
            return core::make_error(
              protocol_error_code::COMPONENT_NOT_FOUND,
              std::format("camera '{}': the entity has no camera component", camera)
            );
        }
        return renderer::make_camera_view(*component, transform->matrix, target);
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // scene_view

    scene_view::scene_view() = default;

    scene_view::~scene_view() {
        release();
    }

    core::result<void> scene_view::ensure(renderer::renderer &renderer, math::uvec2 size) {
        const u32 limit = std::max<u32>(renderer.device().capabilities().maxTextureDimension2d, 1);
        const math::uvec2 clamped(std::clamp<u32>(size.x, 1, limit), std::clamp<u32>(size.y, 1, limit));
        if (mRenderer != &renderer) {
            // Another renderer means the engine restarted: the old target died with the old renderer.
            mRenderer = &renderer;
            mTarget = {};
            mSize = math::uvec2(0, 0);
        }
        if (mTarget.is_valid() && renderer.get_render_target(mTarget) == nullptr) {
            mTarget = {};
        }
        if (!mTarget.is_valid()) {
            renderer::render_target_desc desc = {};
            desc.size = clamped;
            desc.withDepth = true;
            core::result<renderer::render_target_id> created = renderer.create_render_target(desc);
            if (!created.has_value()) {
                mSize = math::uvec2(0, 0);
                return created.get_error();
            }
            mTarget = created.value();
            mSize = clamped;
            return core::result<void>::success();
        }
        if (mSize == clamped) {
            return core::result<void>::success();
        }
        const core::result<void> resized = renderer.resize_render_target(mTarget, clamped);
        if (!resized.has_value()) {
            release();
            return resized;
        }
        mSize = clamped;
        return core::result<void>::success();
    }

    void scene_view::release() noexcept {
        if (mRenderer != nullptr && mTarget.is_valid()) {
            mRenderer->destroy_render_target(mTarget);
        }
        mTarget = {};
        mSize = math::uvec2(0, 0);
    }

    void scene_view::forget() noexcept {
        mRenderer = nullptr;
        mTarget = {};
        mSize = math::uvec2(0, 0);
    }

    void scene_view::submit(renderer::render_view view) {
        if (mRenderer == nullptr || !mTarget.is_valid()) {
            return;
        }
        view.target = mTarget;
        mRenderer->submit_view(view);
    }

    renderer::gpu_texture_id scene_view::colour_texture() const noexcept {
        if (mRenderer == nullptr || !mTarget.is_valid()) {
            return {};
        }
        const renderer::render_target *target = mRenderer->get_render_target(mTarget);
        return target != nullptr ? target->colour_texture() : renderer::gpu_texture_id{};
    }

    u64 scene_view::frames_rendered() const noexcept {
        if (mRenderer == nullptr || !mTarget.is_valid()) {
            return 0;
        }
        const renderer::render_target *target = mRenderer->get_render_target(mTarget);
        return target != nullptr ? target->frames_rendered() : 0;
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // Screenshot backend

    std::unique_ptr<screenshot_backend> make_renderer_screenshot_backend(
      editor_context &context,
      renderer::renderer &target
    ) {
        return std::make_unique<renderer_screenshot_backend>(context, target);
    }
}

#endif
