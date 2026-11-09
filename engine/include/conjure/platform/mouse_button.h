#pragma once

#include "conjure/core/api.h"
#include "conjure/core/types.h"

#include <optional>
#include <string_view>

// Mouse buttons (docs/design/11_platform.md 2.1, P5): GLFW_MOUSE_BUTTON_* numbering, eight buttons.

namespace conjure::platform {

    inline constexpr u32 MOUSE_BUTTON_COUNT = 8;

    enum class mouse_button : u8 {
        LEFT = 0,
        RIGHT = 1,
        MIDDLE = 2,
        BUTTON_4,
        BUTTON_5,
        BUTTON_6,
        BUTTON_7,
        BUTTON_8,
        COUNT
    };

    static_assert(static_cast<u32>(mouse_button::COUNT) == MOUSE_BUTTON_COUNT);

    /// <summary>
    /// True when the value names one of the eight buttons.
    /// </summary>
    [[nodiscard]] constexpr bool is_valid_mouse_button(mouse_button button) noexcept {
        return static_cast<u32>(button) < MOUSE_BUTTON_COUNT;
    }

    /// <summary>
    /// The enumerator name ("MIDDLE"), or "" for unknown values.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(mouse_button button) noexcept;

    /// <summary>
    /// Parses an enumerator name case-insensitively; nullopt for anything else.
    /// </summary>
    [[nodiscard]] CONJURE_API std::optional<mouse_button> parse_mouse_button(std::string_view name) noexcept;
}
