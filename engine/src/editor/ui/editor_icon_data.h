#pragma once

#include "conjure/core/types.h"

#include <span>

// The PNG files of the editor's window icon. tools/generate_brand_icons.py writes their bytes into
// editor_icon_data.cpp from the drawings in docs/assets/brand/icon; editor_icon.cpp decodes them.

namespace conjure::editor::ui::icon_data {

    struct icon_png {
        u32 size = 0;                                   // width and height in pixels
        std::span<const u8> png = {};                   // a complete PNG file
    };

    /// <summary>
    /// One PNG per icon size, smallest first.
    /// </summary>
    [[nodiscard]] std::span<const icon_png> files() noexcept;
}
