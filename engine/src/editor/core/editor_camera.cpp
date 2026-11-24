#include "conjure/editor/editor_camera.h"

#include "conjure/editor/editor_error.h"

#include <algorithm>
#include <cmath>

namespace conjure::editor {

    namespace {
        constexpr f32 MIN_DISTANCE = 0.01f;
        constexpr f32 MAX_PITCH = 1.5533f;      // 89 degrees
        constexpr f32 MAX_FOV_DEGREES = 180.0f;

        [[nodiscard]] core::result<math::vec3> read_vec3(const core::json::value &value, std::string_view name) {
            if (!value.is_array() || value.size() != 3) {
                return core::make_error(protocol_error_code::INVALID_ARGS, std::string(name) + ": expected [x, y, z]");
            }
            math::vec3 result = math::vec3(0.0f);
            for (usize index = 0; index < 3; ++index) {
                const core::json::value component = value[index];
                if (!component.is_number()) {
                    return core::make_error(
                      protocol_error_code::INVALID_ARGS,
                      std::string(name) + ": expected numbers"
                    );
                }
                result[static_cast<glm::length_t>(index)] = component.as_f32();
            }
            if (!std::isfinite(result.x) || !std::isfinite(result.y) || !std::isfinite(result.z)) {
                return core::make_error(
                  protocol_error_code::INVALID_ARGS,
                  std::string(name) + ": components must be finite"
                );
            }
            return result;
        }

        void write_vec3(core::json::document &target, std::string_view key, const math::vec3 &value) {
            core::json::document array = core::json::document::array();
            array.push_back(value.x);
            array.push_back(value.y);
            array.push_back(value.z);
            target.set(key, array);
        }
    }

    editor_camera::editor_camera() = default;
    editor_camera::~editor_camera() = default;

    void editor_camera::set_look_at(const math::vec3 &position, const math::vec3 &target) {
        const math::vec3 offset = target - position;
        if (glm::dot(offset, offset) < MIN_DISTANCE * MIN_DISTANCE) {
            const math::vec3 direction = forward();
            mPosition = position;
            mTarget = position + direction * std::max(glm::length(mTarget - mPosition), 1.0f);
            return;
        }
        mPosition = position;
        mTarget = target;
        // Keep the up vector from collapsing when looking straight up or down.
        const math::vec3 direction = glm::normalize(offset);
        if (std::abs(glm::dot(direction, math::vec3(0.0f, 1.0f, 0.0f))) > 0.999f) {
            mUp = math::vec3(0.0f, 0.0f, direction.y > 0.0f ? 1.0f : -1.0f);
        } else {
            mUp = math::vec3(0.0f, 1.0f, 0.0f);
        }
    }

    void editor_camera::focus(const math::aabb &bounds) {
        if (!bounds.is_valid()) {
            return;
        }
        const math::vec3 center = bounds.center();
        const f32 radius = std::max(glm::length(bounds.extents()), MIN_DISTANCE);
        const math::vec3 direction = forward();
        if (mOrthographic) {
            mOrthographicHeight = std::max(radius * 2.2f, MIN_DISTANCE);
            mTarget = center;
            mPosition = center - direction * std::max(radius * 4.0f, 1.0f);
            return;
        }
        const f32 halfFov = std::max(mVerticalFovRadians * 0.5f, 0.01f);
        const f32 distance = radius / std::sin(halfFov) * 1.1f;
        mTarget = center;
        mPosition = center - direction * distance;
    }

    void editor_camera::orbit(f32 yawRadians, f32 pitchRadians) {
        const math::vec3 offset = mPosition - mTarget;
        const f32 distance = std::max(glm::length(offset), MIN_DISTANCE);
        const math::vec3 direction = offset / distance;
        f32 pitch = std::asin(std::clamp(direction.y, -1.0f, 1.0f));
        f32 yaw = std::atan2(direction.x, direction.z);
        yaw += yawRadians;
        pitch = std::clamp(pitch + pitchRadians, -MAX_PITCH, MAX_PITCH);
        const math::vec3 rotated(std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw));
        mPosition = mTarget + rotated * distance;
        mUp = math::vec3(0.0f, 1.0f, 0.0f);
    }

    void editor_camera::pan(const math::vec2 &viewOffset) {
        const math::mat4 worldFromView = world_from_view();
        const math::vec3 right = math::vec3(worldFromView[0]);
        const math::vec3 up = math::vec3(worldFromView[1]);
        const math::vec3 delta = right * viewOffset.x + up * viewOffset.y;
        mPosition += delta;
        mTarget += delta;
    }

    void editor_camera::zoom(f32 factor) {
        if (!(factor > 0.0f) || !std::isfinite(factor)) {
            return;
        }
        if (mOrthographic) {
            mOrthographicHeight = std::max(mOrthographicHeight * factor, MIN_DISTANCE);
            return;
        }
        const math::vec3 offset = mPosition - mTarget;
        const f32 distance = std::max(glm::length(offset) * factor, MIN_DISTANCE);
        mPosition = mTarget + math::safe_normalize(offset, math::vec3(0.0f, 0.0f, 1.0f)) * distance;
    }

    void editor_camera::set_vertical_fov(f32 radians) noexcept {
        if (std::isfinite(radians)) {
            mVerticalFovRadians = std::clamp(radians, 0.01f, 3.1f);
        }
    }

    void editor_camera::set_orthographic(bool orthographic) noexcept {
        mOrthographic = orthographic;
    }

    void editor_camera::set_orthographic_height(f32 height) noexcept {
        if (std::isfinite(height) && height > 0.0f) {
            mOrthographicHeight = height;
        }
    }

    void editor_camera::set_planes(f32 nearPlane, f32 farPlane) noexcept {
        if (std::isfinite(nearPlane) && std::isfinite(farPlane) && nearPlane > 0.0f && farPlane > nearPlane) {
            mNearPlane = nearPlane;
            mFarPlane = farPlane;
        }
    }

    math::vec3 editor_camera::forward() const noexcept {
        return math::safe_normalize(mTarget - mPosition, math::vec3(0.0f, 0.0f, -1.0f));
    }

    math::mat4 editor_camera::view_matrix() const noexcept {
        return glm::lookAtRH(mPosition, mTarget, mUp);
    }

    math::mat4 editor_camera::world_from_view() const noexcept {
        return glm::inverse(view_matrix());
    }

    math::mat4 editor_camera::projection_matrix(f32 aspectRatio) const noexcept {
        const f32 aspect = (std::isfinite(aspectRatio) && aspectRatio > 0.0f) ? aspectRatio : 1.0f;
        // Reverse-Z: the near plane maps to depth 1 and the far plane to depth 0 (near and far swapped).
        if (mOrthographic) {
            const f32 halfHeight = mOrthographicHeight * 0.5f;
            const f32 halfWidth = halfHeight * aspect;
            return glm::orthoRH_ZO(-halfWidth, halfWidth, -halfHeight, halfHeight, mFarPlane, mNearPlane);
        }
        return glm::perspectiveRH_ZO(mVerticalFovRadians, aspect, mFarPlane, mNearPlane);
    }

    math::ray editor_camera::screen_ray(const math::vec2 &viewportUv, f32 aspectRatio) const noexcept {
        const math::mat4 clipFromWorld = projection_matrix(aspectRatio) * view_matrix();
        const math::mat4 worldFromClip = glm::inverse(clipFromWorld);
        const f32 ndcX = viewportUv.x * 2.0f - 1.0f;
        const f32 ndcY = 1.0f - viewportUv.y * 2.0f;
        // Reverse-Z: depth 1 is the near plane, depth 0 the far plane.
        math::vec4 nearPoint = worldFromClip * math::vec4(ndcX, ndcY, 1.0f, 1.0f);
        math::vec4 farPoint = worldFromClip * math::vec4(ndcX, ndcY, 0.0f, 1.0f);
        if (std::abs(nearPoint.w) > 1.0e-12f) {
            nearPoint /= nearPoint.w;
        }
        if (std::abs(farPoint.w) > 1.0e-12f) {
            farPoint /= farPoint.w;
        }
        math::ray result = {};
        result.origin = math::vec3(nearPoint);
        result.direction = math::safe_normalize(math::vec3(farPoint) - math::vec3(nearPoint), forward());
        return result;
    }

    f32 editor_camera::world_units_per_pixel(f32 viewportHeightPixels) const noexcept {
        const f32 pixels = std::isfinite(viewportHeightPixels) ? std::max(viewportHeightPixels, 1.0f) : 1.0f;
        if (mOrthographic) {
            return mOrthographicHeight / pixels;
        }
        const f32 distance = glm::length(mPosition - mTarget);
        const f32 visibleHeight = 2.0f * distance * std::tan(mVerticalFovRadians * 0.5f);
        return visibleHeight / pixels;
    }

    core::json::document editor_camera::to_json() const {
        core::json::document document = core::json::document::object();
        write_vec3(document, "position", mPosition);
        write_vec3(document, "target", mTarget);
        document.set("fov", math::degrees(mVerticalFovRadians));
        document.set("orthographic", mOrthographic);
        document.set("orthographicHeight", mOrthographicHeight);
        document.set("near", mNearPlane);
        document.set("far", mFarPlane);
        return document;
    }

    core::result<void> editor_camera::apply_json(const core::json::value &value) {
        if (!value.is_object()) {
            return core::make_error(protocol_error_code::INVALID_ARGS, "camera settings must be an object");
        }

        // Every member is validated into a local first and the camera is written only once all of them passed,
        // so a failed view.set_camera leaves the camera exactly as it was (docs/design/04_editor.md D4).
        math::vec3 position = mPosition;
        math::vec3 target = mTarget;
        f32 fovRadians = mVerticalFovRadians;
        bool orthographic = mOrthographic;
        f32 orthographicHeight = mOrthographicHeight;
        f32 nearPlane = mNearPlane;
        f32 farPlane = mFarPlane;
        if (value.contains("position")) {
            CONJURE_TRY_ASSIGN(position, read_vec3(value["position"], "position"));
        }
        if (value.contains("target")) {
            CONJURE_TRY_ASSIGN(target, read_vec3(value["target"], "target"));
        }
        if (value.contains("position") || value.contains("target")) {
            // set_look_at would silently keep the old direction for coincident points; a client gets an error.
            const math::vec3 offset = target - position;
            if (!(glm::dot(offset, offset) >= MIN_DISTANCE * MIN_DISTANCE)) {
                return core::make_error(
                  protocol_error_code::INVALID_ARGS,
                  "position and target must be at least 0.01 apart"
                );
            }
        }
        if (value.contains("fov")) {
            // The wire unit is degrees (the catalogue: "vertical fov (degrees)"); radians stay internal.
            const core::json::value fov = value["fov"];
            if (!fov.is_number() || !(fov.as_f32() > 0.0f) || !(fov.as_f32() < MAX_FOV_DEGREES)) {
                return core::make_error(protocol_error_code::INVALID_ARGS, "fov: expected degrees in (0, 180)");
            }
            fovRadians = math::radians(fov.as_f32());
        }
        if (value.contains("orthographic")) {
            const core::json::value orthographicValue = value["orthographic"];
            if (!orthographicValue.is_bool()) {
                return core::make_error(protocol_error_code::INVALID_ARGS, "orthographic: expected a boolean");
            }
            orthographic = orthographicValue.as_bool();
        }
        if (value.contains("orthographicHeight")) {
            const core::json::value height = value["orthographicHeight"];
            if (!height.is_number() || !(height.as_f32() > 0.0f)) {
                return core::make_error(
                  protocol_error_code::INVALID_ARGS,
                  "orthographicHeight: expected a positive number"
                );
            }
            orthographicHeight = height.as_f32();
        }
        if (value.contains("near") || value.contains("far")) {
            nearPlane = value.contains("near") ? value["near"].as_f32(mNearPlane) : mNearPlane;
            farPlane = value.contains("far") ? value["far"].as_f32(mFarPlane) : mFarPlane;
            if (!(nearPlane > 0.0f) || !(farPlane > nearPlane)) {
                return core::make_error(protocol_error_code::INVALID_ARGS, "near/far: 0 < near < far is required");
            }
        }

        mVerticalFovRadians = fovRadians;
        mOrthographic = orthographic;
        mOrthographicHeight = orthographicHeight;
        mNearPlane = nearPlane;
        mFarPlane = farPlane;
        set_look_at(position, target);
        return core::result<void>::success();
    }
}
