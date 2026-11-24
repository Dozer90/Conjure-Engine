#include "conjure/testing/doctest.h"

#include "editor_test_support.h"

#include "conjure/testing/approx.h"
#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"

#include "conjure/core/json/document.h"
#include "conjure/core/log.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/editor/editor_camera.h"
#include "conjure/editor/editor_context.h"
#include "conjure/editor/protocol/protocol_message.h"
#include "conjure/editor/screenshot_service.h"
#include "conjure/engine/application.h"
#include "conjure/engine/engine_config.h"
#include "editor/core/scene_view.h"

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER
    #include "conjure/renderer/camera.h"
    #include "conjure/renderer/render_scene.h"
    #include "conjure/renderer/render_target.h"
    #include "conjure/renderer/renderer.h"
#endif

#include <filesystem>
#include <optional>
#include <string>
#include <utility>

// editor/core/scene_view.h (docs/design/04_editor.md 3.8, 3.10; code sweep F104): the editor's own pictures of
// the scene. The view built from the editor camera must be the camera picking and the gizmos use, a
// screenshot's camera name must resolve to the view the engine would draw, and a scene_view must own its render
// target through resizes, releases and an engine that went away. Everything runs on the null device: what the
// pixels look like is the renderer's [gpu] cases' business.

using namespace conjure;
using namespace conjure::editor;
using namespace conjure::testing;

#if defined(CONJURE_WITH_RENDERER) && CONJURE_WITH_RENDERER

namespace {
    /// <summary>
    /// A headless engine with the renderer on the null device (the editor's own headless start has none).
    /// </summary>
    [[nodiscard]] engine::application_desc null_rendering_desc() {
        engine::application_desc desc = {};
        desc.config = engine::engine_config::headless_defaults();
        desc.config.scripting.enabled = false;
        desc.config.rendering.enabled = true;
        desc.config.rendering.backend = engine::rhi_backend_choice::NULL_DEVICE;
        desc.config.rendering.gpuValidation = engine::gpu_validation_level::OFF;
        desc.config.logLevel = core::log_level::WARN;
        return desc;
    }

    [[nodiscard]] std::string add_camera(
      editor_test::headless_editor &editor,
      std::string_view name,
      core::json::document camera,
      const math::vec3 &position
    ) {
        const std::string guid = editor.create_entity(name);
        (void) editor.ok(
          "component.add",
          core::json::object({{"entity", guid}, {"type", "camera"}, {"value", std::move(camera)}})
        );
        (void) editor.ok(
          "entity.set_transform",
          core::json::object({
            {"entity", guid},
            {"position", core::json::array({position.x, position.y, position.z})},
          })
        );
        return guid;
    }
}

TEST_SUITE("editor/scene_view") {

    TEST_CASE("make_editor_camera_view - the view is the editor camera that picking and the gizmos use") {
        editor_camera camera = {};
        camera.set_look_at(math::vec3(6.0f, 4.0f, 8.0f), math::vec3(1.0f, 0.5f, 0.0f));
        camera.set_vertical_fov(0.9f);
        camera.set_planes(0.25f, 500.0f);
        const renderer::render_target_id target = renderer::render_target_id::from_u64(0x0000000200000003ull);

        const renderer::render_view perspective = make_editor_camera_view(camera, target, true);
        CHECK(perspective.target == target);
        CONJURE_CHECK_APPROX(perspective.viewFromWorld, camera.view_matrix());
        CONJURE_CHECK_APPROX(perspective.cameraPosition, camera.position());
        CHECK(perspective.cameraData.projection == renderer::projection_type::PERSPECTIVE);
        CHECK(has_flag(perspective.flags, renderer::view_flags::DRAW_DEBUG));
        CHECK_FALSE(has_flag(perspective.flags, renderer::view_flags::NO_POST_PROCESS));
        CHECK(perspective.layerMask == 0xFFFFFFFFu);
        // The renderer builds its projection from cameraData and the target's aspect; it must be the matrix
        // editor_camera::screen_ray unprojects with, or a click would not hit what the picture shows.
        for (const f32 aspect : {1.0f, 16.0f / 9.0f, 0.5f}) {
            CAPTURE(aspect);
            CONJURE_CHECK_APPROX(
              renderer::clip_from_view(perspective.cameraData, aspect),
              camera.projection_matrix(aspect)
            );
        }

        camera.set_orthographic(true);
        camera.set_orthographic_height(12.0f);
        const renderer::render_view orthographic = make_editor_camera_view(camera, target, false);
        CHECK(orthographic.cameraData.projection == renderer::projection_type::ORTHOGRAPHIC);
        CHECK(has_flag(orthographic.flags, renderer::view_flags::NO_POST_PROCESS));
        CONJURE_CHECK_APPROX(
          renderer::clip_from_view(orthographic.cameraData, 1.5f),
          camera.projection_matrix(1.5f)
        );
    }

    TEST_CASE("resolve_camera_view - editor, game and an entity name the view the engine would draw") {
        editor_test::headless_editor editor = {};
        editor_context &context = editor.context();
        const renderer::render_target_id target = renderer::render_target_id::from_u64(0x0000000100000001ull);

        // "editor" needs no scene camera and follows the view mode: 2D keeps authored colours.
        (void) editor.ok(
          "view.set_camera",
          core::json::object({
            {"position", core::json::array({0.0f, 0.0f, 10.0f})},
            {"target", core::json::array({0.0f, 0.0f, 0.0f})},
          })
        );
        const core::result<renderer::render_view> editorView = resolve_camera_view(context, "editor", target);
        CONJURE_REQUIRE_OK(editorView);
        CHECK(editorView.value().target == target);
        CONJURE_CHECK_APPROX(editorView.value().cameraPosition, math::vec3(0.0f, 0.0f, 10.0f));
        CHECK_FALSE(has_flag(editorView.value().flags, renderer::view_flags::NO_POST_PROCESS));
        (void) editor.ok("view.set_mode", core::json::object({{"mode", "2d"}}));
        const core::result<renderer::render_view> flatView = resolve_camera_view(context, "editor", target);
        CONJURE_REQUIRE_OK(flatView);
        CHECK(has_flag(flatView.value().flags, renderer::view_flags::NO_POST_PROCESS));
        CHECK(flatView.value().cameraData.projection == renderer::projection_type::ORTHOGRAPHIC);

        // No camera in the scene yet.
        CONJURE_CHECK_ERROR(resolve_camera_view(context, "game", target), protocol_error_code::COMPONENT_NOT_FOUND);

        // "game" is the camera the engine shows first: main cameras before the others, then by priority.
        const std::string side = add_camera(
          editor,
          "Side",
          core::json::object({{"isMain", false}, {"priority", 9}}),
          math::vec3(-7.0f, 0.0f, 0.0f)
        );
        editor.pump(2);
        core::result<renderer::render_view> game = resolve_camera_view(context, "game", target);
        CONJURE_REQUIRE_OK(game);
        CONJURE_CHECK_APPROX(game.value().cameraPosition, math::vec3(-7.0f, 0.0f, 0.0f));
        const std::string low = add_camera(
          editor,
          "MainLow",
          core::json::object({{"priority", 1}, {"postProcess", false}}),
          math::vec3(0.0f, 3.0f, 0.0f)
        );
        const std::string high = add_camera(
          editor,
          "MainHigh",
          core::json::object({{"priority", 5}}),
          math::vec3(0.0f, 0.0f, 4.0f)
        );
        editor.pump(2);
        game = resolve_camera_view(context, "game", target);
        CONJURE_REQUIRE_OK(game);
        CONJURE_CHECK_APPROX(game.value().cameraPosition, math::vec3(0.0f, 0.0f, 4.0f));
        CHECK(game.value().target == target);

        // An entity by guid is that entity's camera, with its own settings.
        const core::result<renderer::render_view> named = resolve_camera_view(context, low, target);
        CONJURE_REQUIRE_OK(named);
        CONJURE_CHECK_APPROX(named.value().cameraPosition, math::vec3(0.0f, 3.0f, 0.0f));
        CHECK(has_flag(named.value().flags, renderer::view_flags::NO_POST_PROCESS));
        const core::result<renderer::render_view> sideView = resolve_camera_view(context, side, target);
        CONJURE_REQUIRE_OK(sideView);
        CONJURE_CHECK_APPROX(sideView.value().cameraPosition, math::vec3(-7.0f, 0.0f, 0.0f));
        CHECK_FALSE(high.empty());

        const std::string plain = editor.create_entity("Plain");
        CONJURE_CHECK_ERROR(resolve_camera_view(context, plain, target), protocol_error_code::COMPONENT_NOT_FOUND);
        CONJURE_CHECK_ERROR(
          resolve_camera_view(context, core::uuid::generate().to_string(), target),
          protocol_error_code::ENTITY_NOT_FOUND
        );
    }

    TEST_CASE("resolve_camera_view - without a scene only the editor camera resolves") {
        editor_test::headless_editor_options options = {};
        options.withScene = false;
        editor_test::headless_editor editor(options);
        const renderer::render_target_id target = renderer::render_target_id::from_u64(0x0000000100000001ull);
        CONJURE_CHECK_OK(resolve_camera_view(editor.context(), "editor", target));
        CONJURE_CHECK_ERROR(resolve_camera_view(editor.context(), "game", target), protocol_error_code::SCENE_NOT_OPEN);
    }

    TEST_CASE("scene_view - owns a render target through ensure, resize, submit and release") {
        log_capture logs = {};
        engine::application application(null_rendering_desc());
        CONJURE_REQUIRE_OK(application.initialise());
        renderer::renderer *instance = application.context().renderer;
        REQUIRE(instance != nullptr);
        {
            scene_view view = {};
            CHECK_FALSE(view.is_valid());
            CHECK(view.owner() == nullptr);
            CHECK_FALSE(view.colour_texture().is_valid());
            CHECK(view.frames_rendered() == 0);
            // Nothing to draw into: a submit is ignored.
            view.submit(renderer::render_view{});
            CHECK(instance->submitted_view_count() == 0);

            CONJURE_REQUIRE_OK(view.ensure(*instance, math::uvec2(320, 180)));
            REQUIRE(view.is_valid());
            CHECK(view.owner() == instance);
            CHECK(view.size() == math::uvec2(320, 180));
            const renderer::render_target_id target = view.target();
            const renderer::gpu_texture_id colour = view.colour_texture();
            CHECK(colour.is_valid());
            REQUIRE(instance->get_render_target(target) != nullptr);
            CHECK(instance->get_render_target(target)->size() == math::uvec2(320, 180));

            // The same size again does nothing; another size resizes in place, keeping the target and its
            // colour texture id, which is what keeps a registered ImGui texture valid.
            CONJURE_REQUIRE_OK(view.ensure(*instance, math::uvec2(320, 180)));
            CONJURE_REQUIRE_OK(view.ensure(*instance, math::uvec2(640, 400)));
            CHECK(view.target() == target);
            CHECK(view.colour_texture() == colour);
            CHECK(view.size() == math::uvec2(640, 400));
            CHECK(instance->get_render_target(target)->size() == math::uvec2(640, 400));
            // A zero side is clamped to one pixel instead of failing: a collapsed panel still has a picture.
            CONJURE_REQUIRE_OK(view.ensure(*instance, math::uvec2(0, 50)));
            CHECK(view.size() == math::uvec2(1, 50));

            // A submitted view is drawn into this view's target, whatever target it named.
            view.submit(make_editor_camera_view(editor_camera{}, renderer::render_target_id{}, true));
            CHECK(instance->submitted_view_count() == 1);
            CONJURE_REQUIRE_OK(application.step_frame());
            CHECK(instance->stats().viewsRendered == 1);
            CHECK(view.frames_rendered() == 1);

            // A target destroyed behind the view's back is made again.
            instance->destroy_render_target(target);
            CONJURE_REQUIRE_OK(view.ensure(*instance, math::uvec2(64, 64)));
            CHECK(view.is_valid());
            CHECK(view.size() == math::uvec2(64, 64));

            const renderer::render_target_id last = view.target();
            view.release();
            CHECK_FALSE(view.is_valid());
            CHECK(instance->get_render_target(last) == nullptr);

            // forget() lets go without destroying: for an owner that cannot know the renderer is alive.
            CONJURE_REQUIRE_OK(view.ensure(*instance, math::uvec2(32, 32)));
            const renderer::render_target_id kept = view.target();
            view.forget();
            CHECK_FALSE(view.is_valid());
            CHECK(view.owner() == nullptr);
            CHECK(instance->get_render_target(kept) != nullptr);
            instance->destroy_render_target(kept);

            // The destructor releases what is left.
            CONJURE_REQUIRE_OK(view.ensure(*instance, math::uvec2(16, 16)));
        }
        application.shutdown();
    }

    TEST_CASE("regression - code sweep F104 a screenshot is captured only after a frame drew the camera's view") {
        log_capture logs = {};
        temp_directory directory("editor_scene_view_shot");
        engine::application application(null_rendering_desc());
        CONJURE_REQUIRE_OK(application.initialise());
        renderer::renderer *instance = application.context().renderer;
        REQUIRE(instance != nullptr);
        {
            // Declared after the application so it is destroyed first, as editor_application does it.
            editor_context context(application, nullptr, editor_display_mode::HEADLESS_OFFSCREEN);
            // On the null device the context installs no backend (there are no pixels); the backend itself
            // works there and reads back zeros, which is enough to follow its frames.
            CHECK_FALSE(context.screenshots().available());
            context.screenshots().set_backend(make_renderer_screenshot_backend(context, *instance));
            std::optional<protocol_response> answer = std::nullopt;
            const u32 client = context.events().register_client({});
            context.register_response_sink(client, [&answer](const protocol_response &response) {
                answer = response;
            });

            const std::filesystem::path path = directory.path() / "view.png";
            protocol_request request = {};
            request.id = make_id(1);
            request.command = "view.screenshot";
            request.args = core::json::object({
              {"path", path.generic_string()},
              {"width", 48},
              {"height", 32},
              {"waitFrames", 1},
            });
            request.clientId = client;
            CHECK_FALSE(context.dispatcher().dispatch(request).has_value());
            REQUIRE(context.screenshots().pending_count() == 1);
            // The request made the view known for the frame being built.
            CHECK(instance->submitted_view_count() == 1);

            // The tick of the same frame, before anything was rendered: the request used to be captured here,
            // an empty target. Now it waits, and the view is asked for again.
            context.tick();
            CHECK(context.screenshots().pending_count() == 1);
            CHECK_FALSE(answer.has_value());

            // Frame 1 draws it; waitFrames 1 asks for one more drawn frame before the capture.
            CONJURE_REQUIRE_OK(application.step_frame());
            CHECK(instance->stats().viewsRendered == 1);
            context.tick();
            CHECK(context.screenshots().pending_count() == 1);
            CONJURE_REQUIRE_OK(application.step_frame());
            CHECK(instance->stats().viewsRendered == 1);
            context.tick();
            CHECK(context.screenshots().pending_count() == 0);
            for (u32 frame = 0; frame < 200 && !answer.has_value(); ++frame) {
                CONJURE_REQUIRE_OK(application.step_frame());
                context.tick();
            }
            REQUIRE(answer.has_value());
            REQUIRE_MESSAGE(answer->ok, answer->errorMessage);
            CHECK(answer->result.root()["width"].as_i64() == 48);
            CHECK(answer->result.root()["height"].as_i64() == 32);
            CHECK(std::filesystem::is_regular_file(path));
            // The capture gave its target back, and nothing is queued for later frames.
            CONJURE_REQUIRE_OK(application.step_frame());
            CHECK(instance->stats().viewsRendered == 0);

            // A camera that does not exist fails when the request is made.
            answer.reset();
            request.id = make_id(2);
            request.args = core::json::object({{"path", path.generic_string()}, {"camera", "game"}});
            const std::optional<protocol_response> refused = context.dispatcher().dispatch(request);
            if (refused.has_value()) {
                answer = refused;
            }
            for (u32 frame = 0; frame < 10 && !answer.has_value(); ++frame) {
                context.tick();
                CONJURE_REQUIRE_OK(application.step_frame());
            }
            REQUIRE(answer.has_value());
            CHECK_FALSE(answer->ok);
            CHECK(context.screenshots().pending_count() == 0);

            // A request still waiting when the engine goes: cancelled, and the backend lets go of its target
            // without calling into a renderer that may no longer exist.
            answer.reset();
            request.id = make_id(3);
            request.args = core::json::object({{"path", path.generic_string()}, {"waitFrames", 1000}});
            CHECK_FALSE(context.dispatcher().dispatch(request).has_value());
            CHECK(context.screenshots().pending_count() == 1);
            context.unregister_response_sink(client);
            context.events().unregister_client(client);
            application.shutdown();
        }
    }
}

#endif
