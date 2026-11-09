#pragma once

#include "conjure/platform/backend.h"
#include "conjure/platform/event.h"
#include "conjure/platform/monitor.h"
#include "conjure/platform/native_handle.h"

#include "conjure/core/api.h"
#include "conjure/core/enum_flags.h"
#include "conjure/core/math.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

// The abstract window (docs/design/11_platform.md 2.4, 4.2). One live window per process in v1;
// created through create_window() for the backend named in window_desc. Every call is main thread
// only (asserted in Debug and Development); no call at frame time can fail.

namespace conjure::platform {

    class null_backend;

    enum class window_flags : u8 {
        NONE = 0,
        RESIZABLE = 1 << 0,
        DECORATED = 1 << 1,
        VISIBLE = 1 << 2,
        FLOATING = 1 << 3,
        MAXIMISED = 1 << 4,
        SCALE_TO_MONITOR = 1 << 5,
        SCALE_FRAMEBUFFER = 1 << 6,
        DEFAULT = RESIZABLE | DECORATED | VISIBLE | SCALE_TO_MONITOR | SCALE_FRAMEBUFFER
    };
    CONJURE_ENUM_FLAGS(window_flags);

    enum class display_server_hint : u8 {   // Linux only; the GLFW_PLATFORM init hint
        ANY = 0,
        X11,
        WAYLAND
    };

    enum class cursor_mode : u8 {           // GLFW_CURSOR_* (CAPTURED is 3.4)
        NORMAL = 0,
        HIDDEN,
        DISABLED,
        CAPTURED
    };

    /// <summary>
    /// The enumerator name of a cursor mode ("DISABLED"), or "" for unknown values.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(cursor_mode mode) noexcept;

    /// <summary>
    /// Parses a cursor mode name case-insensitively; nullopt for anything else.
    /// </summary>
    [[nodiscard]] CONJURE_API std::optional<cursor_mode> parse_cursor_mode(std::string_view name) noexcept;

    /// <summary>
    /// Longest title create_window accepts, in bytes.
    /// </summary>
    inline constexpr usize MAX_WINDOW_TITLE_BYTES = 255;

    struct window_desc {
        backend_kind backend = backend_kind::GLFW;
        std::string title = "Conjure";
        u32 width = 1600;                                   // logical size; the engine passes window_settings
        u32 height = 900;
        window_flags flags = window_flags::DEFAULT;
        bool fullscreen = false;                            // windowed fullscreen on monitorIndex (P12)
        u32 monitorIndex = 0;
        std::optional<math::ivec2> position = std::nullopt; // nullopt = let the OS / centre on the monitor
        display_server_hint displayServer = display_server_hint::ANY;
        std::string_view gamepadMappings = {};              // extra SDL_GameControllerDB lines
        math::vec2 nullContentScale = math::vec2(1.0f);     // null backend only: reported content scale
    };

    class CONJURE_API window {
        public:
            window();
            virtual ~window();

            window(const window &) = delete;
            window &operator=(const window &) = delete;

        public:
            /// <summary>
            /// The backend behind this window.
            /// </summary>
            [[nodiscard]] virtual backend_kind backend() const noexcept = 0;

            /// <summary>
            /// Processes OS events and returns this frame's batch; the span is valid until the next
            /// call. Several framebuffer size changes in one pump are coalesced into one
            /// framebuffer_resized_event at the end of the batch (P10).
            /// </summary>
            [[nodiscard]] virtual std::span<const event> pump_events() = 0;

            /// <summary>
            /// True once the user or request_close() asked the window to close.
            /// </summary>
            [[nodiscard]] virtual bool should_close() const noexcept = 0;

            /// <summary>
            /// Marks the window as asked to close; should_close() is true afterwards.
            /// </summary>
            virtual void request_close() noexcept = 0;

            /// <summary>
            /// Drawable size in pixels; 0 x 0 while minimised.
            /// </summary>
            [[nodiscard]] virtual math::uvec2 framebuffer_size() const noexcept = 0;

            /// <summary>
            /// Size in logical screen coordinates.
            /// </summary>
            [[nodiscard]] virtual math::uvec2 window_size() const noexcept = 0;

            /// <summary>
            /// framebuffer_size() / window_size() per axis.
            /// </summary>
            [[nodiscard]] virtual math::vec2 content_scale() const noexcept = 0;

            [[nodiscard]] virtual bool is_minimised() const noexcept = 0;
            [[nodiscard]] virtual bool is_focused() const noexcept = 0;
            [[nodiscard]] virtual bool is_visible() const noexcept = 0;
            [[nodiscard]] virtual bool is_fullscreen() const noexcept = 0;

            /// <summary>
            /// Changes the title shown by the OS (UTF-8).
            /// </summary>
            virtual void set_title(std::string_view title) = 0;

            /// <summary>
            /// The current title.
            /// </summary>
            [[nodiscard]] virtual std::string_view title() const noexcept = 0;

            /// <summary>
            /// Resizes the window in logical coordinates; the framebuffer change arrives as an event.
            /// </summary>
            virtual void set_size(math::uvec2 logicalSize) = 0;

            /// <summary>
            /// Moves the window on the virtual desktop. FEATURE_UNAVAILABLE where the display server
            /// does not allow it (Wayland).
            /// </summary>
            [[nodiscard]] virtual core::result<void> set_position(math::ivec2 position) = 0;

            /// <summary>
            /// Position of the client area on the virtual desktop in logical coordinates.
            /// </summary>
            [[nodiscard]] virtual math::ivec2 position() const noexcept = 0;

            virtual void show() = 0;
            virtual void hide() = 0;

            /// <summary>
            /// Windowed fullscreen on monitors()[monitorIndex] with its current video mode (P12), or
            /// back to the previous windowed size. MONITOR_NOT_FOUND leaves the window unchanged.
            /// </summary>
            [[nodiscard]] virtual core::result<void> set_fullscreen(bool fullscreen, u32 monitorIndex = 0) = 0;

            /// <summary>
            /// The connected monitors, primary first; refreshed by pump_events().
            /// </summary>
            [[nodiscard]] virtual std::span<const monitor_info> monitors() const noexcept = 0;

            /// <summary>
            /// The video modes a monitor supports; empty for an unknown index.
            /// </summary>
            [[nodiscard]] virtual std::span<const video_mode> video_modes(u32 monitorIndex) const noexcept = 0;

            /// <summary>
            /// Changes how the cursor behaves over the window. FEATURE_UNAVAILABLE (state unchanged)
            /// where the mode is not supported, e.g. CAPTURED on Wayland.
            /// </summary>
            [[nodiscard]] virtual core::result<void> set_cursor_mode(cursor_mode mode) = 0;

            [[nodiscard]] virtual cursor_mode current_cursor_mode() const noexcept = 0;

            /// <summary>
            /// Raw (unaccelerated) mouse motion while the cursor is DISABLED. FEATURE_UNAVAILABLE
            /// when the system does not provide it.
            /// </summary>
            [[nodiscard]] virtual core::result<void> set_raw_mouse_motion(bool enabled) = 0;

            /// <summary>
            /// Warps the cursor to a position in logical window coordinates.
            /// </summary>
            virtual void set_cursor_position(math::vec2 logicalPosition) = 0;

            /// <summary>
            /// Human-readable name of a connected gamepad slot (glfwGetGamepadName); empty when the
            /// slot is not connected or the index is out of range (R57).
            /// </summary>
            [[nodiscard]] virtual std::string_view gamepad_name(u32 index) const noexcept = 0;

            /// <summary>
            /// Opaque per-OS handles for rhi (D3D12 swapchain, MoltenVK). kind == NONE on the null backend.
            /// </summary>
            [[nodiscard]] virtual native_window_handle native_handle() const noexcept = 0;

            /// <summary>
            /// Instance extensions Vulkan needs to present to this window; empty on the null backend
            /// and when Vulkan is unavailable.
            /// </summary>
            [[nodiscard]] virtual std::span<const char *const> required_vulkan_instance_extensions() const noexcept = 0;

            /// <summary>
            /// Creates a VkSurfaceKHR for VkInstance 'instance' and stores it in *surface (both are
            /// 64-bit handles passed as opaque pointers; no Vulkan header is involved).
            /// VULKAN_UNSUPPORTED on the null backend or without a Vulkan loader; SURFACE_CREATE_FAILED
            /// with the VkResult in the message otherwise.
            /// </summary>
            [[nodiscard]] virtual core::result<void> create_vulkan_surface(void *instance, void **surface) const = 0;

            /// <summary>
            /// The clipboard's text (UTF-8). CLIPBOARD_UNAVAILABLE when it holds no text or the
            /// platform refused.
            /// </summary>
            [[nodiscard]] virtual core::result<std::string> clipboard_text() const = 0;

            /// <summary>
            /// Replaces the clipboard's text (UTF-8).
            /// </summary>
            [[nodiscard]] virtual core::result<void> set_clipboard_text(std::string_view text) = 0;

            /// <summary>
            /// This window as the null backend, or nullptr for a real window.
            /// </summary>
            [[nodiscard]] virtual null_backend *as_null() noexcept;
    };

    /// <summary>
    /// Checks a description the way create_window does: a non-fullscreen window needs a non-zero
    /// size, the title is at most MAX_WINDOW_TITLE_BYTES bytes and the backend is a known kind.
    /// Fails with INVALID_DESC naming the field.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<void> validate_window_desc(const window_desc &desc);

    /// <summary>
    /// Creates the window for desc.backend. Main thread only. Exactly one live window per process
    /// in v1: a second call asserts and returns INVALID_DESC. BACKEND_NOT_COMPILED when the glfw
    /// backend was not built; INIT_FAILED / WINDOW_CREATE_FAILED / MONITOR_NOT_FOUND from the glfw
    /// backend as described in 11_platform.md 6.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<std::unique_ptr<window>> create_window(const window_desc &desc);

    /// <summary>
    /// Number of windows alive in this process (0 or 1 in v1).
    /// </summary>
    [[nodiscard]] CONJURE_API u32 live_window_count() noexcept;
}
