#include "bindings/entry_support.h"

#include "conjure/engine/game_screen.h"
#include "conjure/engine/main_camera.h"

#include "conjure/core/math.h"
#include "conjure/renderer/camera.h"
#include "conjure/renderer/components.h"
#include "conjure/renderer/render_scene.h"
#include "conjure/scene/components.h"
#include "conjure/scene/scene.h"

#include <cmath>
#include <optional>

// camera_* (wave 3 GAP-CSHARP 2.2): the main camera and screen-to-world picking for scripts. Screen coordinates
// are pixels of the game screen (engine::game_screen_of, the window framebuffer), origin top-left, y down; depth
// is metres along the camera's forward axis. Every entry uses the camera's current pose, composed now from the
// local transforms (scene::world_transform_of), so a script that turns the camera and picks in the same Update
// gets this frame's ray; an entity without a local_transform uses its render_world_transform. The view matrix
// comes from renderer::make_camera_view and the projection from renderer::clip_from_view, as the renderer draws
// them. Built only with CONJURE_WITH_RENDERER; the stubs cover the group otherwise. No GPU is needed.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::bindings::detail;

namespace {

    constexpr std::string_view CAMERA = "camera";
    constexpr std::string_view TRANSFORM = "transform";
    constexpr std::string_view NON_FINITE = "non-finite input";
    constexpr std::string_view INVALID_CAMERA =
      "camera data out of range (near > 0, far > near, 0 < fov < pi, orthographic height > 0)";

    /// <summary>
    /// What the entries need of one camera: its projection data, its pose both ways and the game screen size.
    /// </summary>
    struct camera_frame {
        renderer::camera data = {};
        math::mat4 worldFromView = math::mat4(1.0f);
        math::mat4 viewFromWorld = math::mat4(1.0f);
        math::vec2 screen = math::vec2(0.0f);      // pixels; 0 x 0 while minimised or without a window
    };

    [[nodiscard]] bool is_finite(const conjure_vec2 &value) noexcept {
        return std::isfinite(value.x) && std::isfinite(value.y);
    }

    [[nodiscard]] bool is_finite(const conjure_vec3 &value) noexcept {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    [[nodiscard]] conjure_vec3 to_abi(const math::vec3 &value) noexcept {
        return conjure_vec3{value.x, value.y, value.z};
    }

    /// <summary>
    /// The rule clip_from_view asserts on, without the aspect ratio (the entries check the screen apart), with
    /// non-finite values refused too.
    /// </summary>
    [[nodiscard]] bool valid_camera_data(const renderer::camera &data) noexcept {
        if (!std::isfinite(data.nearPlane) || !std::isfinite(data.farPlane)) {
            return false;
        }
        if (!(data.nearPlane > 0.0f) || !(data.farPlane > data.nearPlane)) {
            return false;
        }
        if (data.projection == renderer::projection_type::PERSPECTIVE) {
            return data.verticalFovRadians > 0.0f && data.verticalFovRadians < math::PI;
        }
        return std::isfinite(data.orthographicHeight) && data.orthographicHeight > 0.0f;
    }

    /// <summary>
    /// The camera of 'packed' at its current pose with the game screen; records ENTITY_NOT_ALIVE,
    /// COMPONENT_MISSING "camera" or "transform" (neither a local nor a render world transform) or
    /// INVALID_ARGUMENT (camera data clip_from_view would refuse) and returns false otherwise.
    /// </summary>
    [[nodiscard]] bool resolve_camera(
      native_function_id id,
      uint64_t packed,
      camera_frame &frame,
      int32_t &status
    ) {
        const renderer::camera_component *camera = named_component_for_read<renderer::camera_component>(
          id, packed, renderer::CAMERA_COMPONENT_TYPE_NAME, CAMERA, status
        );
        if (camera == nullptr) {
            return false;
        }
        const ecs::entity entity = marshal::to_entity(packed);
        const ecs::world &world = *active_world();

        // The current pose: composed now for a scene entity, the propagated matrix for a module-built camera.
        math::mat4 worldFromCamera = math::mat4(1.0f);
        const scene::scene *activeScene = active_scene();
        const std::optional<math::transform> composed =
          activeScene != nullptr ? activeScene->world_transform_of(entity) : std::nullopt;
        if (composed.has_value()) {
            worldFromCamera = composed->to_matrix();
        } else {
            const scene::render_world_transform *rendered = world.get<scene::render_world_transform>(entity);
            if (rendered == nullptr) {
                status = fail_entry(id, bindings_error::COMPONENT_MISSING, TRANSFORM);
                return false;
            }
            worldFromCamera = rendered->matrix;
        }
        if (!valid_camera_data(camera->cameraData)) {
            status = fail_entry(id, bindings_error::INVALID_ARGUMENT, INVALID_CAMERA);
            return false;
        }

        const renderer::render_view view = renderer::make_camera_view(*camera, worldFromCamera);
        const engine::game_screen screen = engine::game_screen_of(active_context().window);
        frame.data = camera->cameraData;
        frame.viewFromWorld = view.viewFromWorld;
        frame.worldFromView = glm::inverse(view.viewFromWorld);
        frame.screen = math::vec2(static_cast<f32>(screen.pixels.x), static_cast<f32>(screen.pixels.y));
        return true;
    }

    [[nodiscard]] bool has_area(const camera_frame &frame) noexcept {
        return frame.screen.x > 0.0f && frame.screen.y > 0.0f;
    }

    [[nodiscard]] math::vec3 eye_of(const camera_frame &frame) noexcept {
        return math::vec3(frame.worldFromView[3]);
    }

    [[nodiscard]] math::vec3 forward_of(const camera_frame &frame) noexcept {
        return math::safe_normalize(-math::vec3(frame.worldFromView[2]));
    }

    /// <summary>
    /// The two scale terms of clip_from_view: x and y in clip space per view-space unit (perspective: at a depth
    /// of 1 m). Needs a screen with area and valid camera data.
    /// </summary>
    [[nodiscard]] math::vec2 projection_scale(const camera_frame &frame) noexcept {
        const f32 aspectRatio = frame.screen.x / frame.screen.y;
        const math::mat4 clipFromView = renderer::clip_from_view(frame.data, aspectRatio);
        return math::vec2(clipFromView[0][0], clipFromView[1][1]);
    }

    /// <summary>
    /// The world point under a pixel at 'depth' metres along the forward axis (a screen with area).
    /// </summary>
    [[nodiscard]] math::vec3 unproject(const camera_frame &frame, math::vec2 pixels, f32 depth) noexcept {
        const math::vec2 scale = projection_scale(frame);
        const f32 ndcX = (pixels.x / frame.screen.x) * 2.0f - 1.0f;
        const f32 ndcY = 1.0f - (pixels.y / frame.screen.y) * 2.0f;
        // Perspective clip coordinates are divided by the view depth, orthographic ones are not.
        const f32 spread = frame.data.projection == renderer::projection_type::PERSPECTIVE ? depth : 1.0f;
        const math::vec4 inView = math::vec4(ndcX * spread / scale.x, ndcY * spread / scale.y, -depth, 1.0f);
        return math::vec3(frame.worldFromView * inView);
    }
}

uint64_t conjure::bindings::entries::camera_main() {
    constexpr native_function_id ID = native_function_id::camera_main;
    CONJURE_BINDINGS_GUARD_ENTITY(ID);
    const ecs::world *world = active_world();
    if (world == nullptr) {
        static_cast<void>(unavailable(ID, "world"));
        return 0;
    }
    return marshal::from_entity(engine::find_main_camera(*world));
}

int32_t conjure::bindings::entries::camera_screen_ray(
  uint64_t camera,
  const conjure_vec2 *screenPixels,
  conjure_vec3 *outOrigin,
  conjure_vec3 *outDirection
) {
    constexpr native_function_id ID = native_function_id::camera_screen_ray;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (screenPixels == nullptr || outOrigin == nullptr || outDirection == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "null pointer");
    }
    if (!is_finite(*screenPixels)) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, NON_FINITE);
    }
    int32_t status = 0;
    camera_frame frame = {};
    if (!resolve_camera(ID, camera, frame, status)) {
        return status;
    }

    const math::vec3 forward = forward_of(frame);
    if (!has_area(frame)) {
        *outOrigin = to_abi(eye_of(frame));
        *outDirection = to_abi(forward);
        return 0;
    }
    // The ray starts on the near plane, as renderer::ray_from_viewport's does; a perspective ray passes through
    // the eye, an orthographic one runs along the forward axis.
    const math::vec2 pixels = math::vec2(screenPixels->x, screenPixels->y);
    const math::vec3 origin = unproject(frame, pixels, frame.data.nearPlane);
    const bool perspective = frame.data.projection == renderer::projection_type::PERSPECTIVE;
    const math::vec3 direction = perspective ? math::safe_normalize(origin - eye_of(frame), forward) : forward;
    *outOrigin = to_abi(origin);
    *outDirection = to_abi(direction);
    return 1;
}

int32_t conjure::bindings::entries::camera_screen_to_world(
  uint64_t camera,
  const conjure_vec3 *screenPixelsAndDepth,
  conjure_vec3 *outWorld
) {
    constexpr native_function_id ID = native_function_id::camera_screen_to_world;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (screenPixelsAndDepth == nullptr || outWorld == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "null pointer");
    }
    if (!is_finite(*screenPixelsAndDepth)) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, NON_FINITE);
    }
    int32_t status = 0;
    camera_frame frame = {};
    if (!resolve_camera(ID, camera, frame, status)) {
        return status;
    }

    const f32 depth = screenPixelsAndDepth->z;
    if (!has_area(frame)) {
        *outWorld = to_abi(eye_of(frame) + forward_of(frame) * depth);
        return 0;
    }
    const math::vec2 pixels = math::vec2(screenPixelsAndDepth->x, screenPixelsAndDepth->y);
    *outWorld = to_abi(unproject(frame, pixels, depth));
    return 1;
}

int32_t conjure::bindings::entries::camera_world_to_screen(
  uint64_t camera,
  const conjure_vec3 *world,
  conjure_vec3 *outScreenPixelsAndDepth
) {
    constexpr native_function_id ID = native_function_id::camera_world_to_screen;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (world == nullptr || outScreenPixelsAndDepth == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "null pointer");
    }
    if (!is_finite(*world)) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, NON_FINITE);
    }
    int32_t status = 0;
    camera_frame frame = {};
    if (!resolve_camera(ID, camera, frame, status)) {
        return status;
    }

    const math::vec4 inView = frame.viewFromWorld * math::vec4(world->x, world->y, world->z, 1.0f);
    const f32 depth = -inView.z;
    if (!has_area(frame)) {
        *outScreenPixelsAndDepth = conjure_vec3{0.0f, 0.0f, depth};
        return 0;
    }
    const math::vec2 scale = projection_scale(frame);
    const bool perspective = frame.data.projection == renderer::projection_type::PERSPECTIVE;
    // A perspective point on the camera plane (|depth| <= EPSILON) has no projection: it is written at the screen
    // centre and answers 0, even a hair in front of the eye.
    const bool divisible = !perspective || math::absolute(depth) > math::EPSILON;
    const f32 divisor = perspective ? depth : 1.0f;
    const f32 ndcX = divisible ? inView.x * scale.x / divisor : 0.0f;
    const f32 ndcY = divisible ? inView.y * scale.y / divisor : 0.0f;
    const f32 pixelX = (ndcX + 1.0f) * 0.5f * frame.screen.x;
    const f32 pixelY = (1.0f - ndcY) * 0.5f * frame.screen.y;
    *outScreenPixelsAndDepth = conjure_vec3{pixelX, pixelY, depth};
    return divisible && depth > 0.0f ? 1 : 0;
}
