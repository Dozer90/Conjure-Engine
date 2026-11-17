#pragma once

#include "conjure/renderer/texture_asset.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <span>
#include <vector>

// The image decoders behind an engine interface (docs/design/02_renderer.md 3.13, 4.2). A file is routed by its
// signature, never by its name: JPEG XL goes to libjxl (renderer/import/jpeg_xl_codec.h), everything else to
// stb_image. Only this translation unit and the environment baker include stb, only the JPEG XL codec includes
// libjxl; everything else sees RGBA8 or RGB float pixels.

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
    /// Decodes JPEG XL, PNG, JPEG, TGA, BMP (and GIF / HDR as 8-bit) to RGBA8. stb ignores colour profiles, so
    /// only a JPEG XL file is converted to 'colourSpace' (decode_jpeg_xl_rgba8); 'jobs' (optional) decodes a
    /// JPEG XL file on several threads. Fails with INVALID_ARGUMENT for empty data, data above 2 GiB or a size
    /// above texture_asset::MAX_DIMENSION, and renderer_error::DECODE_FAILED with the decoder's reason otherwise.
    /// </summary>
    [[nodiscard]] core::result<decoded_image> decode_rgba8(
      std::span<const std::byte> encoded,
      texture_colour_space colourSpace,
      core::jobs::job_system *jobs
    );

    /// <summary>
    /// Decodes a Radiance .hdr or a JPEG XL file (any stb format too, converted) to linear RGB floats.
    /// </summary>
    [[nodiscard]] core::result<decoded_hdr_image> decode_hdr_rgb(
      std::span<const std::byte> encoded,
      core::jobs::job_system *jobs
    );
}
