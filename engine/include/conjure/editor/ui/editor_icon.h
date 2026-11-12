#pragma once

#include "conjure/core/api.h"
#include "conjure/core/image.h"
#include "conjure/core/result.h"

#include <vector>

// The editor's window icon (docs/handbook/brand.md): the official Conjure logo in the Midnight accent, compiled
// into editor_ui as one PNG per size by tools/generate_brand_icons.py. On Windows the executable also carries the
// icon as its GLFW_ICON resource, so Explorer, the taskbar and the first frame show it before the layer attaches.

namespace conjure::platform {
    class window;
}

namespace conjure::editor::ui {

    /// <summary>
    /// Decodes the icon: one square RGBA8 image per size, smallest first, from 16 to 256 px. PARSE_FAILURE
    /// naming the size when an embedded PNG does not decode, which only a hand-edited generated file can cause.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<std::vector<core::image>> editor_icon_images();

    /// <summary>
    /// Decodes the icon and sets it on a window (window::set_icon). FEATURE_UNAVAILABLE where windows have no
    /// icon of their own (macOS, Wayland); on any error the window keeps the icon it had.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<void> apply_editor_icon(platform::window &window);
}
