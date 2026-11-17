#pragma once

#include "conjure/renderer/texture_asset.h"

#include "conjure/core/api.h"
#include "conjure/core/image.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <span>
#include <vector>

// JPEG XL (ISO/IEC 18181) in the renderer (docs/design/02_renderer.md 3.13, 4.2): the signature check the image
// decoder routes by and an RGBA8 encoder for screenshots and tools. Decoding needs no call of its own:
// texture_asset::decode, the texture importer (.jxl), glTF images and equirect_image::decode_hdr recognise a JPEG XL
// file by its signature. libjxl does the work in engine/src; this header carries only engine types.

namespace conjure::renderer {

    /// <summary>
    /// How encode_jpeg_xl compresses an image. A lossy encode targets a Butteraugli distance in (0, 25], where 1
    /// is visually lossless. The colour space is what the file is tagged as: sRGB, or linear sRGB for data.
    /// </summary>
    struct jpeg_xl_encode_options {
        bool lossless = true;                           // bit-exact; otherwise lossy at 'distance'
        f32 distance = 1.0f;                            // lossy only
        u32 effort = 7;                                 // 1 (fastest) .. 10 (smallest file)
        texture_colour_space colourSpace = texture_colour_space::SRGB;

        [[nodiscard]] bool operator==(const jpeg_xl_encode_options &other) const noexcept = default;
    };

    /// <summary>
    /// True when 'bytes' start with a JPEG XL signature: a bare codestream (FF 0A) or the container's signature
    /// box. Two bytes decide a codestream and twelve a container; fewer read as false.
    /// </summary>
    [[nodiscard]] CONJURE_API bool is_jpeg_xl(std::span<const std::byte> bytes) noexcept;

    /// <summary>
    /// Encodes an RGBA8 image as a JPEG XL file tagged sRGB or linear sRGB ('options.colourSpace'), on 'jobs'
    /// when it has workers (the bytes are the same either way). A file of an opaque image still stores its alpha
    /// channel, so it decodes back to the same RGBA. Fails with INVALID_ARGUMENT for an empty image, an effort
    /// outside 1..10 or a lossy distance outside (0, 25], and with BACKEND_FAILURE when libjxl fails.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<std::vector<std::byte>> encode_jpeg_xl(
      const core::image &image,
      const jpeg_xl_encode_options &options = {},
      core::jobs::job_system *jobs = nullptr
    );
}
