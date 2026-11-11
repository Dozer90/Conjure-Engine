#pragma once

#include "conjure/core/api.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/math.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

// The editor viewport camera (docs/design/04_editor.md 4.1, 7): lives in core so view.pick, view.set_camera and
// view.focus work headless. An orbit camera (position, target, up) with perspective or orthographic projection;
// the reverse-Z convention of the renderer is used for the projection matrix so gizmos and picking agree with
// what the renderer draws.

namespace conjure::editor {

    class CONJURE_API editor_camera {
        private:
            math::vec3 mPosition = math::vec3(10.0f, 10.0f, 10.0f);
            math::vec3 mTarget = math::vec3(0.0f);
            math::vec3 mUp = math::vec3(0.0f, 1.0f, 0.0f);
            f32 mVerticalFovRadians = 1.0472f;              // 60 degrees
            f32 mOrthographicHeight = 10.0f;
            f32 mNearPlane = 0.1f;
            f32 mFarPlane = 1000.0f;
            bool mOrthographic = false;

        public:
            editor_camera();
            ~editor_camera();

            editor_camera(const editor_camera &) = default;
            editor_camera &operator=(const editor_camera &) = default;

        public:
            /// <summary>
            /// Places the camera at 'position' looking at 'target' (a target equal to the position keeps
            /// the previous view direction).
            /// </summary>
            void set_look_at(const math::vec3 &position, const math::vec3 &target);

            /// <summary>
            /// Frames a box: the target becomes its centre and the distance fits the box in the vertical field
            /// of view (orthographic: the height covers the box). An invalid box is ignored.
            /// </summary>
            void focus(const math::aabb &bounds);

            /// <summary>
            /// Rotates around the target by yaw and pitch (radians); pitch is clamped short of the poles.
            /// </summary>
            void orbit(f32 yawRadians, f32 pitchRadians);

            /// <summary>
            /// Moves position and target by an offset in view space (right, up).
            /// </summary>
            void pan(const math::vec2 &viewOffset);

            /// <summary>
            /// Scales the distance to the target (factor &gt; 1 moves away); orthographic scales the height.
            /// </summary>
            void zoom(f32 factor);

            void set_vertical_fov(f32 radians) noexcept;
            void set_orthographic(bool orthographic) noexcept;
            void set_orthographic_height(f32 height) noexcept;
            void set_planes(f32 nearPlane, f32 farPlane) noexcept;

            [[nodiscard]] const math::vec3 &position() const noexcept {
                return mPosition;
            }

            [[nodiscard]] const math::vec3 &target() const noexcept {
                return mTarget;
            }

            [[nodiscard]] math::vec3 forward() const noexcept;

            [[nodiscard]] f32 vertical_fov() const noexcept {
                return mVerticalFovRadians;
            }

            [[nodiscard]] bool is_orthographic() const noexcept {
                return mOrthographic;
            }

            [[nodiscard]] f32 orthographic_height() const noexcept {
                return mOrthographicHeight;
            }

            [[nodiscard]] f32 near_plane() const noexcept {
                return mNearPlane;
            }

            [[nodiscard]] f32 far_plane() const noexcept {
                return mFarPlane;
            }

            /// <summary>
            /// View matrix (world to view).
            /// </summary>
            [[nodiscard]] math::mat4 view_matrix() const noexcept;

            /// <summary>
            /// World-from-view matrix (the inverse of view_matrix()).
            /// </summary>
            [[nodiscard]] math::mat4 world_from_view() const noexcept;

            /// <summary>
            /// Reverse-Z projection for a viewport aspect ratio (width / height); a non-positive aspect uses 1.
            /// </summary>
            [[nodiscard]] math::mat4 projection_matrix(f32 aspectRatio) const noexcept;

            /// <summary>
            /// The world-space ray through a normalised viewport position (origin top-left, y down).
            /// </summary>
            [[nodiscard]] math::ray screen_ray(const math::vec2 &viewportUv, f32 aspectRatio) const noexcept;

            /// <summary>
            /// {position, target, fov, orthographic, orthographicHeight, near, far} as view.get_camera returns;
            /// fov is the vertical field of view in degrees (the wire unit; the camera stores radians).
            /// </summary>
            [[nodiscard]] core::json::document to_json() const;

            /// <summary>
            /// Applies the members present in the object (view.set_camera); fov in degrees, 0 &lt; fov &lt; 180.
            /// INVALID_ARGS for a malformed member.
            /// </summary>
            [[nodiscard]] core::result<void> apply_json(const core::json::value &value);
    };
}
