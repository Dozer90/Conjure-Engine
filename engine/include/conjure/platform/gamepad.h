#pragma once

#include "conjure/core/api.h"
#include "conjure/core/types.h"

#include <optional>
#include <string_view>

// Gamepad buttons and axes (docs/design/11_platform.md 2.1, P5, P11): the SDL game controller
// layout GLFW exposes as GLFW_GAMEPAD_BUTTON_* / GLFW_GAMEPAD_AXIS_*. Up to four gamepads are
// tracked; joysticks without a gamepad mapping are ignored.

namespace conjure::platform {

    inline constexpr u32 MAX_GAMEPADS = 4;
    inline constexpr u32 GAMEPAD_BUTTON_COUNT = 15;
    inline constexpr u32 GAMEPAD_AXIS_COUNT = 6;

    enum class gamepad_button : u8 {
        A = 0,
        B,
        X,
        Y,
        LEFT_BUMPER,
        RIGHT_BUMPER,
        BACK,
        START,
        GUIDE,
        LEFT_THUMB,
        RIGHT_THUMB,
        DPAD_UP,
        DPAD_RIGHT,
        DPAD_DOWN,
        DPAD_LEFT,
        COUNT
    };

    enum class gamepad_axis : u8 {
        LEFT_X = 0,
        LEFT_Y,
        RIGHT_X,
        RIGHT_Y,
        LEFT_TRIGGER,       // reported in [0, 1] (GLFW's [-1, 1] is remapped by the backend)
        RIGHT_TRIGGER,      // reported in [0, 1]
        COUNT
    };

    static_assert(static_cast<u32>(gamepad_button::COUNT) == GAMEPAD_BUTTON_COUNT);
    static_assert(static_cast<u32>(gamepad_axis::COUNT) == GAMEPAD_AXIS_COUNT);

    /// <summary>
    /// True when the value names one of the fifteen buttons.
    /// </summary>
    [[nodiscard]] constexpr bool is_valid_gamepad_button(gamepad_button button) noexcept {
        return static_cast<u32>(button) < GAMEPAD_BUTTON_COUNT;
    }

    /// <summary>
    /// True when the value names one of the six axes.
    /// </summary>
    [[nodiscard]] constexpr bool is_valid_gamepad_axis(gamepad_axis axis) noexcept {
        return static_cast<u32>(axis) < GAMEPAD_AXIS_COUNT;
    }

    /// <summary>
    /// The enumerator name ("LEFT_BUMPER"), or "" for unknown values.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(gamepad_button button) noexcept;

    /// <summary>
    /// The enumerator name ("RIGHT_TRIGGER"), or "" for unknown values.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(gamepad_axis axis) noexcept;

    /// <summary>
    /// Parses a button name case-insensitively; nullopt for anything else.
    /// </summary>
    [[nodiscard]] CONJURE_API std::optional<gamepad_button> parse_gamepad_button(std::string_view name) noexcept;

    /// <summary>
    /// Parses an axis name case-insensitively; nullopt for anything else.
    /// </summary>
    [[nodiscard]] CONJURE_API std::optional<gamepad_axis> parse_gamepad_axis(std::string_view name) noexcept;
}
