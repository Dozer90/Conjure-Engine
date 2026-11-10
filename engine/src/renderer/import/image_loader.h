#pragma once

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <span>
#include <vector>

// stb_image behind an engine interface (docs/design/02_renderer.md 3.6, 4.2). Only this translation
// unit and the environment baker include stb; everything else sees RGBA8 or RGB float pixels.

namespace conjure::renderer::detail {

    struct decoded_image {
        u32 width = 0;
        u32 height = 0;
        std::vector<std::byte> rgba = {};       // 4 bytes per pixel, rows top to bottom
    };

    struct decoded_hdr_image {
        u32 width = 0;
        u32 height = 0;
        std::vector<f32> rgb = {};              // 3 floats per pixel, rows top to bottom
    };

    /// <summary>
    /// Decodes PNG, JPEG, TGA, BMP (and GIF / HDR as 8-bit) to RGBA8. Fails with INVALID_ARGUMENT for
    /// empty data and renderer_error::DECODE_FAILED with stb's reason otherwise.
    /// </summary>
    [[nodiscard]] core::result<decoded_image> decode_rgba8(std::span<const std::byte> encoded);

    /// <summary>
    /// Decodes a Radiance .hdr (or any stb format, converted) to linear RGB floats.
    /// </summary>
    [[nodiscard]] core::result<decoded_hdr_image> decode_hdr_rgb(std::span<const std::byte> encoded);
}
