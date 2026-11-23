#pragma once

#include "conjure/platform/button_action.h"
#include "conjure/platform/gamepad.h"
#include "conjure/platform/key.h"
#include "conjure/platform/mouse_button.h"

#include "conjure/core/api.h"
#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <variant>

// The event stream a window produces once per frame (docs/design/11_platform.md 2.2, P2). Every
// alternative is a trivially copyable struct; the one payload with external data (drop_files_event)
// points into a window-owned buffer that stays valid until the next pump_events(). input_event lists
// the alternatives input::apply consumes; event adds the window-level ones. Both list the input
// alternatives first and in the same order, so the conversions below are index remaps.

namespace conjure::platform {

    struct key_event {
        key keyCode = key::UNKNOWN;
        i32 scancode = 0;
        button_action action = button_action::RELEASE;
        key_modifier_flags modifiers = key_modifier_flags::NONE;
    };

    struct text_event {
        u32 codepoint = 0;      // UTF-32, from glfwSetCharCallback
    };

    struct mouse_move_event {
        math::vec2 position = math::vec2(0.0f);     // logical window coordinates
    };

    struct mouse_button_event {
        mouse_button button = mouse_button::LEFT;
        button_action action = button_action::RELEASE;
        key_modifier_flags modifiers = key_modifier_flags::NONE;
    };

    struct mouse_scroll_event {
        math::vec2 offset = math::vec2(0.0f);       // +y = away from the user
    };

    struct cursor_enter_event {
        bool entered = false;
    };

    struct gamepad_connection_event {
        u8 gamepadIndex = 0;
        bool connected = false;
    };

    struct gamepad_button_event {
        u8 gamepadIndex = 0;
        gamepad_button button = gamepad_button::A;
        button_action action = button_action::RELEASE;
    };

    struct gamepad_axis_event {
        u8 gamepadIndex = 0;
        gamepad_axis axis = gamepad_axis::LEFT_X;
        f32 value = 0.0f;       // sticks [-1, 1], triggers [0, 1]
    };

    struct framebuffer_resized_event {
        u32 width = 0;          // pixels; 0 x 0 while minimised
        u32 height = 0;
    };

    // window::content_scale() changed since the scale last announced, more than a fractional scale's rounding of
    // the framebuffer: with window_flags::SCALE_FRAMEBUFFER, a macOS window went between a Retina and a standard
    // display, or a Wayland window got a new output scale (it moved to another output, the output changed its
    // scale, or the compositor sent the first scale just after creation, since GLFW starts a Wayland window at
    // 1). Without that flag no OS scales the framebuffer, so none announces a scale. Windows and X11 keep the
    // framebuffer as large as the window, so a DPI change there announces no scale; with SCALE_TO_MONITOR,
    // Windows resizes the window instead (a framebuffer_resized_event). The null backend queues one per
    // simulate_content_scale call.
    struct content_scale_changed_event {
        math::vec2 scale = math::vec2(1.0f);    // the new content_scale(), framebuffer / window per axis
    };

    struct focus_event {
        bool focused = false;
    };

    struct minimise_event {
        bool minimised = false;
    };

    struct close_event {};

    struct drop_files_event {
        std::span<const std::string> paths = {};    // valid until the next pump_events()
    };

    using input_event = std::variant<
      key_event,
      text_event,
      mouse_move_event,
      mouse_button_event,
      mouse_scroll_event,
      cursor_enter_event,
      gamepad_connection_event,
      gamepad_button_event,
      gamepad_axis_event>;

    using event = std::variant<
      key_event,
      text_event,
      mouse_move_event,
      mouse_button_event,
      mouse_scroll_event,
      cursor_enter_event,
      gamepad_connection_event,
      gamepad_button_event,
      gamepad_axis_event,
      framebuffer_resized_event,
      content_scale_changed_event,
      focus_event,
      minimise_event,
      close_event,
      drop_files_event>;

    static_assert(std::is_trivially_copyable_v<input_event>);
    static_assert(std::is_trivially_copyable_v<event>);

    /// <summary>
    /// Number of input alternatives; they occupy the first INPUT_EVENT_COUNT indices of event.
    /// </summary>
    inline constexpr usize INPUT_EVENT_COUNT = std::variant_size_v<input_event>;

    /// <summary>
    /// Number of alternatives of event.
    /// </summary>
    inline constexpr usize EVENT_COUNT = std::variant_size_v<event>;

    /// <summary>
    /// True when the event is one of the input alternatives (what input::apply consumes).
    /// </summary>
    [[nodiscard]] CONJURE_API bool is_input_event(const event &value) noexcept;

    /// <summary>
    /// The event as an input_event, or nullopt for a window event.
    /// </summary>
    [[nodiscard]] CONJURE_API std::optional<input_event> to_input_event(const event &value) noexcept;

    /// <summary>
    /// Widens an input_event to an event (never fails).
    /// </summary>
    [[nodiscard]] CONJURE_API event to_event(const input_event &value) noexcept;

    /// <summary>
    /// The payload when the event holds EventType, otherwise nullptr (std::get_if).
    /// </summary>
    template <typename EventType>
    [[nodiscard]] const EventType *event_as(const event &value) noexcept {
        return std::get_if<EventType>(&value);
    }

    /// <summary>
    /// The alternative name of an event ("key_event", "close_event", ...).
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view event_name(const event &value) noexcept;

    /// <summary>
    /// The alternative name and payload, e.g. "key_event{key=SPACE, scancode=57, action=PRESS,
    /// modifiers=SHIFT}", for the editor console and window diagnostics.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string to_string(const event &value);
}
