#pragma once

// Umbrella header of the platform module (docs/design/11_platform.md 4): window, events, input and
// the backend queries. Include this rather than the individual headers unless compile time matters.

#include "conjure/platform/backend.h"
#include "conjure/platform/button_action.h"
#include "conjure/platform/event.h"
#include "conjure/platform/gamepad.h"
#include "conjure/platform/input.h"
#include "conjure/platform/input_state.h"
#include "conjure/platform/key.h"
#include "conjure/platform/log_category.h"
#include "conjure/platform/monitor.h"
#include "conjure/platform/mouse_button.h"
#include "conjure/platform/native_handle.h"
#include "conjure/platform/null_backend.h"
#include "conjure/platform/platform_error.h"
#include "conjure/platform/window.h"

#include "conjure/core/api.h"

#include <string_view>

namespace conjure::platform {

    /// <summary>
    /// The version string of a backend: glfwGetVersionString() for GLFW (when compiled, otherwise
    /// "glfw (not compiled)"), "null" for the null backend, "" for unknown kinds.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view backend_version_string(backend_kind kind) noexcept;
}
