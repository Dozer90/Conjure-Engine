#pragma once

#include "conjure/core/api.h"
#include "conjure/core/types.h"

#include <optional>
#include <string_view>

// What happened to a key or button (docs/design/11_platform.md 2.1). The values are GLFW's
// GLFW_RELEASE / GLFW_PRESS / GLFW_REPEAT and travel unchanged through the C ABI
// (conjure_button_action, R56).

namespace conjure::platform {

    enum class button_action : u8 {
        RELEASE = 0,
        PRESS = 1,
        REPEAT = 2
    };

    /// <summary>
    /// The enumerator name ("PRESS"), or "" for unknown values.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(button_action action) noexcept;

    /// <summary>
    /// Parses an enumerator name case-insensitively; nullopt for anything else.
    /// </summary>
    [[nodiscard]] CONJURE_API std::optional<button_action> parse_button_action(std::string_view name) noexcept;
}
