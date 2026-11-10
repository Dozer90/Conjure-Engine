#pragma once

#include "conjure/core/image.h"
#include "conjure/core/types.h"
#include "conjure/rhi/texture_format.h"

#include <cstddef>

// Texel decoding of renderer::read_back (docs/design/02_renderer.md 4.2, R12): every render target
// colour format is turned into the 8-bit sRGB-encoded RGBA of core::image. Pure functions so the
// tests can check the float formats without a device that returns real pixels.

namespace conjure::renderer {

    /// <summary>
    /// Decodes one R11G11B10_FLOAT channel: 'mantissaBits' is 6 for the red and green channels, 5 for
    /// blue; denormals and the exponent bias follow the format's definition (no sign bit).
    /// </summary>
    [[nodiscard]] f32 decode_r11g11b10_channel(u32 bits, u32 mantissaBits) noexcept;

    /// <summary>
    /// Turns a linear value into the 8-bit sRGB encoding (clamped to [0, 1]).
    /// </summary>
    [[nodiscard]] u8 encode_srgb8(f32 linear) noexcept;

    /// <summary>
    /// Decodes the texel at 'texel' (bytes_per_pixel(format) bytes) of a render target colour format
    /// into sRGB-encoded 8-bit RGBA: the 8-bit formats are copied (BGRA swizzled), the float formats
    /// are sRGB-encoded with the alpha clamped. Unsupported formats decode to transparent black.
    /// </summary>
    [[nodiscard]] core::rgba8 decode_read_back_texel(rhi::texture_format format, const std::byte *texel) noexcept;
}
