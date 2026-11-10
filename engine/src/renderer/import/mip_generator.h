#pragma once

#include "conjure/renderer/texture_asset.h"

#include "conjure/core/types.h"

#include <cstddef>
#include <span>
#include <vector>

// CPU mip generation (docs/design/02_renderer.md 2.6): a 2x2 box filter per level, averaging in
// linear space for sRGB textures (decode, average, re-encode) so a grey checker stays mid-grey in
// linear terms. Odd dimensions clamp the second tap to the edge.

namespace conjure::renderer::detail {

    /// <summary>
    /// The full chain (mip 0 included) for RGBA8 pixels.
    /// </summary>
    [[nodiscard]] std::vector<texture_mip> generate_rgba8_mips(
      u32 width,
      u32 height,
      std::span<const std::byte> pixels,
      bool srgb
    );

    /// <summary>
    /// One level down from an RGBA8 mip.
    /// </summary>
    [[nodiscard]] texture_mip downsample_rgba8(const texture_mip &source, bool srgb);

    /// <summary>
    /// sRGB electro-optical transfer function (encoded [0, 1] -> linear).
    /// </summary>
    [[nodiscard]] f32 srgb_to_linear(f32 encoded) noexcept;

    /// <summary>
    /// sRGB opto-electronic transfer function (linear -> encoded [0, 1]).
    /// </summary>
    [[nodiscard]] f32 linear_to_srgb(f32 linear) noexcept;
}
