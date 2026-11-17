#pragma once

#include "renderer/import/image_loader.h"

#include "conjure/renderer/texture_asset.h"

#include "conjure/core/jobs/job_system.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <span>
#include <vector>

// JPEG XL (ISO/IEC 18181) behind engine types (docs/design/02_renderer.md 3.13, 4.2): libjxl decodes and encodes,
// skcms (libjxl's default colour management system) converts colour profiles. Only jpeg_xl_decoder.cpp and
// jpeg_xl_encoder.cpp include libjxl; image_loader.cpp routes a file here when its signature is JPEG XL, so the
// texture importer, glTF images, texture_asset::decode and the environment importer share one decoder.
//
// Decoding keeps the first displayed frame of an animation (and logs a warning naming how many were dropped),
// applies the file's orientation, unpremultiplies premultiplied alpha in the encoding the file stores (before any
// colour conversion) and expands grey to RGB. It is deterministic: the same file gives the same bytes on every run
// and with any number of job system workers. Its memory limit (below) does not depend on the workers either; a
// decode on workers only holds a few MB more per thread that has work, so only a file needing within that much of
// its limit, far beyond what its image size explains, can decode inline and be refused on workers.
//
// PQ and HLG (an encoded colour profile with either transfer function) are display light in nits. libjxl decodes
// them with 1.0 at the file's intensity target (10000 nits for PQ and 1000 for HLG unless the file says otherwise);
// the decoder rescales them so that 1.0 is JPEG_XL_REFERENCE_WHITE_NITS, the reference white of ITU-R BT.2408, which
// is where a .hdr or float file keeps its 1.0. HLG is first taken to display light by libjxl's OOTF for a display of
// the intensity target, so 75 % HLG on a 1000-nit display (203 nits) becomes 1.0.
//
// Header data never sizes an allocation unchecked. Before a frame is read: the image size against
// texture_asset::MAX_DIMENSION, the extra channels against JPEG_XL_MAX_EXTRA_CHANNELS, and every layer's size
// against MAX_DIMENSION (a header-only pass over the frame headers). While it decodes, libjxl's allocations count
// against a budget derived from the image size (JPEG_XL_BUDGET_*), which bounds what no header check sees:
// reference-only frames, per-frame state and the ICC profile (libjxl itself caps a profile at 256 MiB and grows its
// buffers only as the profile's data arrives). Container box sizes size no allocation: no box or JPEG
// reconstruction event is asked for, and a codestream split across boxes is copied only as far as the file's bytes
// reach. Every such refusal is INVALID_ARGUMENT naming the limit.

namespace conjure::renderer::detail {

    /// <summary>
    /// Nits of 1.0 in a decoded PQ or HLG file: BT.2408's reference (diffuse) white for both.
    /// </summary>
    inline constexpr f32 JPEG_XL_REFERENCE_WHITE_NITS = 203.0f;

    /// <summary>
    /// Most extra channels (alpha, depth, spot colours, a CMYK black, ...) a decoded file may declare; libjxl would
    /// decode every one into a plane of the image's size although only alpha is output.
    /// </summary>
    inline constexpr u32 JPEG_XL_MAX_EXTRA_CHANNELS = 16;

    /// <summary>
    /// Most threads libjxl runs one decode or encode on: the job system runner never splits a range into more
    /// tasks, however many workers there are, so libjxl never keeps per-thread storage for more.
    /// </summary>
    inline constexpr u32 JPEG_XL_MAX_THREADS = 16;

    /// <summary>
    /// libjxl's memory budget for one decode: a fixed allowance, one per thread libjxl may run on, counted for
    /// JPEG_XL_MAX_THREADS whether the decode runs inline or on any number of workers (so whether a file decodes
    /// does not depend on them), and one per sample of the image (width x height x (colour + extra channels)).
    /// Measured peaks it must stay above: under 5 MB for a small image on one thread, about 5 MB more per thread
    /// that has a group to decode, and about 14 bytes per sample for a 1024 x 1024 lossless RGBA file on nine
    /// threads; reference frames of an animation hold up to four more 4-byte planes per sample.
    /// </summary>
    inline constexpr u64 JPEG_XL_BUDGET_BASE_BYTES = 32ull * 1024 * 1024;
    inline constexpr u64 JPEG_XL_BUDGET_THREAD_BYTES = 8ull * 1024 * 1024;
    inline constexpr u64 JPEG_XL_BUDGET_THREADS_BYTES = JPEG_XL_BUDGET_THREAD_BYTES * JPEG_XL_MAX_THREADS;
    inline constexpr u64 JPEG_XL_BUDGET_SAMPLE_BYTES = 64;

    /// <summary>
    /// Decodes a JPEG XL file to RGBA8, rows top to bottom (alpha 255 when the file has none).
    ///
    /// SRGB converts the file's colour encoding (an ICC profile or an encoded one: Display P3, linear light, ...)
    /// to sRGB through the colour management system. A PQ or HLG file is taken to linear light in reference-white
    /// units (see above), clipped at 1 (reference white is white, brighter highlights clip) and encoded with the
    /// sRGB curve. LINEAR keeps the samples in the file's own encoding, because a linear texture holds data
    /// (normals, masks) whose numbers must not change. Samples of more than 8 bits (16-bit integers, half or single
    /// floats) keep their high byte, which is how a 16-bit PNG reaches 8 bits through stb_image; colour and alpha
    /// are judged separately, so a 16-bit alpha keeps its high byte next to 8-bit colour, and that colour, when it is
    /// converted, is rounded to the nearest byte (libjxl's own 8-bit output, used when no channel is deeper, rounds
    /// after an ordered dither, at most one step from the nearest).
    ///
    /// Fails with INVALID_ARGUMENT for empty data, data above 2 GiB or any limit above, with
    /// renderer_error::DECODE_FAILED naming "truncated" for a file that ends early and "corrupt" for one libjxl
    /// rejects otherwise.
    /// </summary>
    [[nodiscard]] core::result<decoded_image> decode_jpeg_xl_rgba8(
      std::span<const std::byte> encoded,
      texture_colour_space colourSpace,
      core::jobs::job_system *jobs
    );

    /// <summary>
    /// Decodes a JPEG XL file to linear-light RGB floats with sRGB (Rec. 709) primaries, converting the file's
    /// colour encoding through the colour management system; values above 1 (HDR) are kept, and PQ and HLG come
    /// out in reference-white units. The samples are as libjxl produces them: a float file may hold NaN,
    /// infinities or negative values, and a wide-gamut colour has negative components in sRGB primaries
    /// (equirect_image::decode_hdr sanitises them). Fails like decode_jpeg_xl_rgba8.
    /// </summary>
    [[nodiscard]] core::result<decoded_hdr_image> decode_jpeg_xl_rgb_float(
      std::span<const std::byte> encoded,
      core::jobs::job_system *jobs
    );

    /// <summary>
    /// Sample type of the pixels handed to encode_jpeg_xl, which is also what the file stores.
    /// </summary>
    enum class jpeg_xl_sample_type : u8 {
        UINT8 = 0,
        UINT16,
        FLOAT16,                                        // IEEE half, native byte order
        FLOAT32
    };

    /// <summary>
    /// The colour encoding a file is tagged with (an encoded profile, no ICC).
    /// </summary>
    enum class jpeg_xl_colour_encoding : u8 {
        SRGB = 0,
        LINEAR_SRGB,
        DISPLAY_P3,                                     // P3 primaries, D65 white, sRGB transfer function
        REC2100_PQ,                                     // Rec. 2020 primaries, D65 white, PQ (SMPTE ST 2084)
        REC2100_HLG                                     // Rec. 2020 primaries, D65 white, HLG (BT.2100)
    };

    /// <summary>
    /// What encode_jpeg_xl writes: every layout the decoder reads, for tools and for the tests' fixtures (the
    /// public encode_jpeg_xl writes RGBA8 through it). Samples are interleaved per pixel, rows top to bottom, no
    /// row padding.
    /// </summary>
    struct jpeg_xl_encode_desc {
        u32 width = 0;
        u32 height = 0;
        u32 channelCount = 4;                           // 1 grey, 2 grey + alpha, 3 RGB, 4 RGBA
        jpeg_xl_sample_type sampleType = jpeg_xl_sample_type::UINT8;
        jpeg_xl_colour_encoding colourEncoding = jpeg_xl_colour_encoding::SRGB;
        std::span<const std::byte> iccProfile = {};     // when given, tags the file instead of colourEncoding
        f32 intensityTarget = 0.0f;                     // nits of 1.0; 0: libjxl's (10000 PQ, 1000 HLG, else 255)
        u32 colourBits = 0;                             // integer samples: bits stored for colour (0: all of
                                                        // them); alpha keeps the sample type's
        u32 orientation = 1;                            // the EXIF orientation 1..8 the decoder applies
        bool premultipliedAlpha = false;                // colour samples are multiplied by alpha
        bool lossless = true;
        f32 distance = 1.0f;                            // Butteraugli distance of a lossy encode
        u32 effort = 7;                                 // libjxl's effort, 1 (fastest) .. 10
        std::vector<std::span<const std::byte>> frames = {};   // one image, or the frames of an animation
        u32 frameDurationMilliseconds = 100;            // per frame of an animation
        u32 layerWidth = 0;                             // non-zero: every frame is a layer of this size at the
        u32 layerHeight = 0;                            // image's origin, which may reach past the image
    };

    /// <summary>
    /// Encodes 'desc' as a JPEG XL file, on 'jobs' when it has workers. A lossy file keeps its alpha lossless.
    /// Fails with INVALID_ARGUMENT for a zero size, a channel count outside 1..4, an orientation outside 1..8, no
    /// frame, several frames of no duration, a frame whose byte count does not match the size (the layer's, when
    /// one is set), channel count and sample type, a layer with one side zero, premultiplied alpha without an alpha
    /// channel, a negative or NaN intensity target, or colour bits on float samples or above the sample type's;
    /// with BACKEND_FAILURE naming the step when
    /// libjxl refuses a value it checks itself (an effort outside 1..10, a distance outside 0..25, an ICC profile
    /// it cannot parse) or fails.
    /// </summary>
    [[nodiscard]] core::result<std::vector<std::byte>> encode_jpeg_xl(
      const jpeg_xl_encode_desc &desc,
      core::jobs::job_system *jobs
    );
}
