#pragma once

#include "conjure/platform/event.h"
#include "conjure/platform/monitor.h"
#include "conjure/platform/window.h"

#include "conjure/core/math.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <GLFW/glfw3.h>

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The real window (docs/design/11_platform.md 3.3, 3.4): a GLFWwindow whose callbacks append to a
// pending event vector, a gamepad poll that diffs against the previous frame, and resize coalescing.
// pump_events swaps the pending vector into the delivered one, exactly like the null backend: GLFW
// raises resize, focus and iconify callbacks synchronously inside setters on Win32 and Cocoa, so a
// pending buffer that is never cleared at pump start is the only way those events survive until the
// next pump. Everything runs on the main thread inside pump_events or a setter.

namespace conjure::platform::glfw {

    class glfw_window final : public window {
        private:
            struct gamepad_slot {
                int joystickId = -1;                // GLFW_JOYSTICK_*; -1 = slot free
                GLFWgamepadstate previous = {};     // raw GLFW state of the last poll
                std::string name = {};
            };

            struct drop_range {
                usize offset = 0;
                usize count = 0;
            };

            enum class optional_feature : u8 {
                CURSOR_NORMAL = 0,
                CURSOR_HIDDEN,
                CURSOR_DISABLED,
                CURSOR_CAPTURED,
                RAW_MOUSE_MOTION,
                WINDOW_POSITION,
                CURSOR_POSITION,
                COUNT
            };

            GLFWwindow *mWindow = nullptr;
            std::string mTitle = {};
            std::vector<event> mPending = {};               // fed by the callbacks and the gamepad poll
            std::vector<event> mDelivered = {};             // returned by the last pump
            std::vector<std::string> mPendingDropPaths = {};
            std::vector<std::string> mDeliveredDropPaths = {};
            std::vector<drop_range> mPendingDropRanges = {};
            std::vector<drop_range> mDeliveredDropRanges = {};
            std::optional<math::uvec2> mPendingResize = std::nullopt;   // coalesced framebuffer size (P10)
            bool mMinimised = false;
            bool mFullscreen = false;
            bool mPositionSupported = true;         // false on Wayland: clients never learn where they are
            bool mCursorWarpSupported = true;       // false on Wayland: glfwSetCursorPos is unavailable
            math::ivec2 mWindowedPosition = math::ivec2(0);
            math::uvec2 mWindowedSize = math::uvec2(0);     // last size the OS confirmed while windowed
            cursor_mode mCursorMode = cursor_mode::NORMAL;
            bool mMonitorsDirty = true;
            std::vector<monitor_info> mMonitors = {};
            std::vector<std::vector<video_mode>> mVideoModes = {};
            std::array<gamepad_slot, MAX_GAMEPADS> mGamepads = {};
            std::span<const char *const> mVulkanExtensions = {};
            std::array<bool, static_cast<usize>(optional_feature::COUNT)> mFeatureWarned = {};

        public:
            glfw_window();
            ~glfw_window() override;

        public:
            /// <summary>
            /// Acquires the GLFW context, creates the OS window and installs the callbacks (3.3).
            /// INIT_FAILED, MONITOR_NOT_FOUND or WINDOW_CREATE_FAILED on failure, with the context
            /// reference released again.
            /// </summary>
            [[nodiscard]] static core::result<std::unique_ptr<window>> create(const window_desc &desc);

            /*---------------------------------------------------------------*/
            // window

            [[nodiscard]] backend_kind backend() const noexcept override;
            [[nodiscard]] std::span<const event> pump_events() override;
            [[nodiscard]] bool should_close() const noexcept override;
            void request_close() noexcept override;
            [[nodiscard]] math::uvec2 framebuffer_size() const noexcept override;
            [[nodiscard]] math::uvec2 window_size() const noexcept override;
            [[nodiscard]] math::vec2 content_scale() const noexcept override;
            [[nodiscard]] bool is_minimised() const noexcept override;
            [[nodiscard]] bool is_focused() const noexcept override;
            [[nodiscard]] bool is_visible() const noexcept override;
            [[nodiscard]] bool is_fullscreen() const noexcept override;
            void set_title(std::string_view title) override;
            [[nodiscard]] std::string_view title() const noexcept override;
            void set_size(math::uvec2 logicalSize) override;
            [[nodiscard]] core::result<void> set_position(math::ivec2 position) override;
            [[nodiscard]] math::ivec2 position() const noexcept override;
            void show() override;
            void hide() override;
            [[nodiscard]] core::result<void> set_fullscreen(bool fullscreen, u32 monitorIndex = 0) override;
            [[nodiscard]] std::span<const monitor_info> monitors() const noexcept override;
            [[nodiscard]] std::span<const video_mode> video_modes(u32 monitorIndex) const noexcept override;
            [[nodiscard]] core::result<void> set_cursor_mode(cursor_mode mode) override;
            [[nodiscard]] cursor_mode current_cursor_mode() const noexcept override;
            [[nodiscard]] core::result<void> set_raw_mouse_motion(bool enabled) override;
            void set_cursor_position(math::vec2 logicalPosition) override;
            [[nodiscard]] std::string_view gamepad_name(u32 index) const noexcept override;
            [[nodiscard]] native_window_handle native_handle() const noexcept override;
            [[nodiscard]] std::span<const char *const> required_vulkan_instance_extensions() const noexcept override;
            [[nodiscard]] core::result<void> create_vulkan_surface(void *instance, void **surface) const override;
            [[nodiscard]] core::result<std::string> clipboard_text() const override;
            [[nodiscard]] core::result<void> set_clipboard_text(std::string_view text) override;

            /*---------------------------------------------------------------*/
            // Process-wide callbacks dispatched by glfw_context

            /// <summary>
            /// A joystick connected or disconnected: assigns or frees a gamepad slot and queues the
            /// gamepad_connection_event. Joysticks without a gamepad mapping are ignored (P11).
            /// </summary>
            void on_joystick(int joystickId, bool connected);

            /// <summary>
            /// A monitor was connected or disconnected; monitors() is rebuilt by the next pump.
            /// </summary>
            void on_monitors_changed() noexcept;

        private:
            [[nodiscard]] core::result<void> initialise(const window_desc &desc);
            void install_callbacks() noexcept;
            void scan_joysticks();
            void poll_gamepads();
            void rebuild_monitors();
            void patch_drop_spans() noexcept;
            void warn_feature_unavailable(optional_feature feature, std::string_view what);
            [[nodiscard]] core::result<void> feature_unavailable(optional_feature feature, std::string_view what);
            [[nodiscard]] static glfw_window *self_of(GLFWwindow *window) noexcept;

            static void on_key(GLFWwindow *window, int keyCode, int scancode, int action, int modifiers);
            static void on_char(GLFWwindow *window, unsigned int codepoint);
            static void on_cursor_position(GLFWwindow *window, double x, double y);
            static void on_cursor_enter(GLFWwindow *window, int entered);
            static void on_mouse_button(GLFWwindow *window, int button, int action, int modifiers);
            static void on_scroll(GLFWwindow *window, double xOffset, double yOffset);
            static void on_drop(GLFWwindow *window, int count, const char **paths);
            static void on_window_size(GLFWwindow *window, int width, int height);
            static void on_framebuffer_size(GLFWwindow *window, int width, int height);
            static void on_content_scale(GLFWwindow *window, float xScale, float yScale);
            static void on_focus(GLFWwindow *window, int focused);
            static void on_iconify(GLFWwindow *window, int iconified);
            static void on_close(GLFWwindow *window);
    };
}
