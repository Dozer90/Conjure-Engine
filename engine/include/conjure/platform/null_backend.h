#pragma once

#include "conjure/platform/event.h"
#include "conjure/platform/monitor.h"
#include "conjure/platform/window.h"

#include "conjure/core/api.h"
#include "conjure/core/image.h"
#include "conjure/core/math.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The virtual window (docs/design/11_platform.md 3.5, 7): no OS call of any kind, constructible
// without a display server, deterministic. Tests and headless_engine drive it through push_event()
// and the simulate_*() controls, which queue events for the next pump_events(); every setter updates
// state so the getters round-trip (R57). One virtual monitor: "Null Monitor", 1920 x 1080 @ 60 Hz,
// content scale 1. Error contracts match the glfw backend so a caller tested here sees the same
// codes there: CLIPBOARD_UNAVAILABLE until text (even empty text) has been copied, INVALID_ARGUMENT
// for null pointers passed to create_vulkan_surface before VULKAN_UNSUPPORTED.

namespace conjure::platform {

    class CONJURE_API null_backend final : public window {
        public:
            static constexpr u32 VIRTUAL_MONITOR_WIDTH = 1920;
            static constexpr u32 VIRTUAL_MONITOR_HEIGHT = 1080;
            static constexpr u32 VIRTUAL_MONITOR_REFRESH_HZ = 60;

        private:
            struct drop_range {
                usize offset = 0;
                usize count = 0;
            };

            std::string mTitle = {};
            std::vector<core::image> mIcons = {};           // copies of the last set_icon images
            math::uvec2 mLogicalSize = math::uvec2(0);
            math::uvec2 mWindowedSize = math::uvec2(0);     // restored when leaving fullscreen
            math::vec2 mContentScale = math::vec2(1.0f);
            math::ivec2 mPosition = math::ivec2(0);
            math::vec2 mCursorPosition = math::vec2(0.0f);
            bool mFocused = true;
            bool mMinimised = false;
            bool mVisible = true;
            bool mFullscreen = false;
            bool mShouldClose = false;
            bool mRawMouseMotion = false;
            cursor_mode mCursorMode = cursor_mode::NORMAL;
            std::string mClipboard = {};
            bool mClipboardHasText = false;                 // nothing copied yet: CLIPBOARD_UNAVAILABLE
            std::vector<monitor_info> mMonitors = {};
            std::vector<video_mode> mVideoModes = {};
            std::array<bool, MAX_GAMEPADS> mGamepadConnected = {};
            std::array<std::string, MAX_GAMEPADS> mGamepadNames = {};

            std::vector<event> mPending = {};               // queued for the next pump
            std::vector<event> mDelivered = {};             // returned by the last pump
            std::vector<std::string> mPendingDropPaths = {};
            std::vector<std::string> mDeliveredDropPaths = {};
            std::vector<drop_range> mPendingDropRanges = {};
            std::vector<drop_range> mDeliveredDropRanges = {};
            std::optional<math::uvec2> mPendingResize = std::nullopt;   // coalesced framebuffer size (P10)

        public:
            explicit null_backend(const window_desc &desc);
            ~null_backend() override;

        public:
            /// <summary>
            /// Builds a null window from a description (what create_window does for NULL_BACKEND).
            /// The description is expected to pass validate_window_desc (asserted).
            /// </summary>
            [[nodiscard]] static std::unique_ptr<null_backend> create(const window_desc &desc);

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
            [[nodiscard]] core::result<void> set_icon(std::span<const core::image> images) override;
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
            [[nodiscard]] null_backend *as_null() noexcept override;

            /*---------------------------------------------------------------*/
            // Simulation controls (main thread; everything is delivered by the next pump_events())

            /// <summary>
            /// Queues any event verbatim. A gamepad_connection_event also updates gamepad_name() when
            /// it is delivered.
            /// </summary>
            void push_event(const event &value);

            /// <summary>
            /// Changes the logical size; the next pump ends with one framebuffer_resized_event carrying
            /// round(size * content_scale) in pixels, however many resizes were simulated.
            /// </summary>
            void simulate_resize(u32 width, u32 height);

            /// <summary>
            /// Changes the content scale: queues a content_scale_changed_event and, when the
            /// framebuffer size changes with it, the coalesced framebuffer_resized_event.
            /// </summary>
            void simulate_content_scale(math::vec2 scale);

            /// <summary>
            /// Changes the focus state and queues a focus_event.
            /// </summary>
            void simulate_focus(bool focused);

            /// <summary>
            /// Changes the minimised state and queues a minimise_event; the framebuffer becomes 0 x 0
            /// while minimised (delivered as a coalesced resize, P10) and comes back on restore.
            /// </summary>
            void simulate_minimise(bool minimised);

            /// <summary>
            /// Queues a close_event; should_close() is true immediately.
            /// </summary>
            void simulate_close();

            /// <summary>
            /// Copies the paths and queues one drop_files_event spanning them; the span is valid until
            /// the pump after the one that delivered it.
            /// </summary>
            void simulate_drop_files(std::span<const std::string> paths);

            /// <summary>
            /// Events the next pump_events() will deliver, the coalesced resize included.
            /// </summary>
            [[nodiscard]] usize pending_event_count() const noexcept;

            /// <summary>
            /// Where set_cursor_position() last warped the cursor (logical coordinates).
            /// </summary>
            [[nodiscard]] math::vec2 cursor_position() const noexcept;

            /// <summary>
            /// The raw mouse motion flag set_raw_mouse_motion() stored.
            /// </summary>
            [[nodiscard]] bool raw_mouse_motion() const noexcept;

            /// <summary>
            /// Copies of the images the last successful set_icon() received; empty for the default icon.
            /// </summary>
            [[nodiscard]] std::span<const core::image> icons() const noexcept;

        private:
            [[nodiscard]] math::uvec2 compute_framebuffer_size() const noexcept;
            void queue_resize() noexcept;
            [[nodiscard]] math::ivec2 centred_position() const noexcept;
    };
}
