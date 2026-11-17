#include "renderer/import/jpeg_xl_codec.h"
#include "renderer/import/jpeg_xl_hooks.h"
#include "renderer/import/mip_generator.h"

#include "conjure/renderer/jpeg_xl.h"
#include "conjure/renderer/log_category.h"
#include "conjure/renderer/renderer_error.h"
#include "conjure/renderer/texture_asset.h"

#include "conjure/core/error.h"
#include "conjure/core/log.h"
#include "conjure/core/result.h"

#include <jxl/cms.h>
#include <jxl/cms_interface.h>
#include <jxl/codestream_header.h>
#include <jxl/color_encoding.h>
#include <jxl/decode.h>
#include <jxl/types.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::renderer;

namespace {
    /// <summary>
    /// What the caller wants out of the decoder: RGBA8 in sRGB, RGBA8 in the file's own encoding, or linear RGB
    /// floats.
    /// </summary>
    enum class output_kind : u8 {
        RGBA8_SRGB = 0,
        RGBA8_AS_STORED,
        RGB_LINEAR_FLOAT
    };

    struct decoder_deleter {
        void operator()(JxlDecoder *decoder) const noexcept {
            JxlDecoderDestroy(decoder);
        }
    };

    /// <summary>
    /// The first displayed frame: RGBA8 bytes, or floats (RGB for the HDR output, RGBA on their way through the
    /// engine's own steps).
    /// </summary>
    struct decoded_frame {
        u32 width = 0;
        u32 height = 0;
        std::vector<std::byte> rgba = {};
        std::vector<f32> floats = {};
    };

    /// <summary>
    /// How the output reaches the form the caller asked for. Usually libjxl produces it directly, dividing
    /// premultiplied alpha itself when no colour conversion follows the division (plan_premultiplied). When it
    /// cannot, libjxl outputs RGBA floats and the engine finishes them, in this order: converts them from
    /// 'decodedIcc' to the file's own profile 'originalIcc' (libjxl renders a lossy (XYB) image into an encoded
    /// profile only; asked for an ICC profile it stops on an internal check), divides the colour by alpha
    /// (premultiplied alpha belongs to the encoding the file stores, and libjxl would divide only after
    /// converting), converts to 'wantedIcc', scales PQ and HLG to reference-white units, and for an SRGB texture of
    /// PQ or HLG applies the sRGB curve. An empty profile skips its conversion.
    /// </summary>
    struct colour_plan {
        bool engineFinishes = false;
        std::vector<uint8_t> decodedIcc = {};
        std::vector<uint8_t> originalIcc = {};
        bool unpremultiply = false;
        std::vector<uint8_t> wantedIcc = {};
        f32 luminanceScale = 1.0f;
        bool encodeSrgb = false;
    };

    /// <summary>
    /// The colour encodings a decode works between, read once the file's own is known (JXL_DEC_COLOR_ENCODING).
    /// </summary>
    struct colour_target {
        JxlColorEncoding original = {};                 // valid when 'encoded'
        bool encoded = false;                           // the file's own profile is an encoded one, not only ICC
        bool renderable = false;                        // ... with a known transfer function libjxl renders into
        bool rescales = false;                          // PQ or HLG, brought to reference-white units
        JxlColorEncoding wanted = {};                   // what the output kind is converted to
    };

    /// <summary>
    /// What the header-only pass learns for the decode. libjxl takes its unpremultiply option only before it reads
    /// the file, while whether it may divide depends on the colour encoding, which the main decode reads later.
    /// </summary>
    struct header_facts {
        bool libjxlUnpremultiplies = false;
    };

    /// <summary>
    /// Where the float scanlines of a file with a channel deeper than 8 bits land: an RGBA8 image, each channel
    /// quantised by its own depth (quantise).
    /// </summary>
    struct quantised_target {
        std::byte *rgba = nullptr;
        usize width = 0;
        bool colourDeep = false;
        bool alphaDeep = false;
    };

    // libjxl's floor for the alpha it divides premultiplied colour by (lib/jxl/alpha.h kSmallAlpha).
    constexpr f32 SMALLEST_ALPHA = 1.0f / static_cast<f32>(1u << 26);

    // A codestream of headers only: FF 0A, then an 8 x 8 SizeHeader and all-default ImageMetadata and transform
    // data (sRGB, 8 bits, XYB-encoded), least significant bit first.
    constexpr std::array<uint8_t, 4> PROFILE_ONLY_CODESTREAM = {0xFF, 0x0A, 0x41, 0x06};

    constexpr u64 BYTES_PER_MEBIBYTE = 1024ull * 1024;

    [[nodiscard]] core::error decode_failure(std::string_view caller, std::string_view reason) {
        return core::make_error(renderer_error::DECODE_FAILED, "{}: JPEG XL: {}", caller, reason);
    }

    [[nodiscard]] bool colour_is_deep(const JxlBasicInfo &info) noexcept {
        return info.exponent_bits_per_sample > 0 || info.bits_per_sample > 8;
    }

    [[nodiscard]] bool alpha_is_deep(const JxlBasicInfo &info) noexcept {
        return info.alpha_exponent_bits > 0 || info.alpha_bits > 8;
    }

    [[nodiscard]] bool has_premultiplied_alpha(const JxlBasicInfo &info) noexcept {
        return info.alpha_bits > 0 && info.alpha_premultiplied == JXL_TRUE;
    }

    /// <summary>
    /// An output sample in [0, 1] as a byte: through 16 bits for a deep channel (the high byte, like every other
    /// deep sample), rounded for an 8-bit one, whose value libjxl may have converted to anywhere between two bytes.
    /// NaN reads as 0.
    /// </summary>
    [[nodiscard]] std::byte quantise(f32 value, bool deep) noexcept {
        const f32 clamped = value > 0.0f ? std::min(value, 1.0f) : 0.0f;
        if (deep) {
            return static_cast<std::byte>(static_cast<u32>(std::lround(clamped * 65535.0f)) >> 8);
        }
        return static_cast<std::byte>(std::lround(clamped * 255.0f));
    }

    /// <summary>
    /// sRGB, or linear light with sRGB primaries, as an encoded profile; grey for a grey image (one colour
    /// channel), which the decoder then copies into R, G and B.
    /// </summary>
    [[nodiscard]] JxlColorEncoding standard_encoding(bool grey, bool linear) noexcept {
        JxlColorEncoding encoding = {};
        encoding.color_space = grey ? JXL_COLOR_SPACE_GRAY : JXL_COLOR_SPACE_RGB;
        encoding.white_point = JXL_WHITE_POINT_D65;
        encoding.primaries = JXL_PRIMARIES_SRGB;
        encoding.transfer_function = linear ? JXL_TRANSFER_FUNCTION_LINEAR : JXL_TRANSFER_FUNCTION_SRGB;
        encoding.rendering_intent = JXL_RENDERING_INTENT_RELATIVE;
        return encoding;
    }

    /// <summary>
    /// True when samples in 'current' are already samples in 'wanted'. Primaries do not apply to grey.
    /// </summary>
    [[nodiscard]] bool same_encoding(const JxlColorEncoding &current, const JxlColorEncoding &wanted) noexcept {
        const bool greyOrSamePrimaries =
          wanted.color_space == JXL_COLOR_SPACE_GRAY || current.primaries == wanted.primaries;
        return current.color_space == wanted.color_space && current.white_point == wanted.white_point &&
          greyOrSamePrimaries && current.transfer_function == wanted.transfer_function;
    }

    /// <summary>
    /// True when the pixels libjxl would output are already in 'wanted', so no conversion has to run (and an
    /// 8-bit sRGB file decodes bit-exact).
    /// </summary>
    [[nodiscard]] bool outputs_already(const JxlDecoder &decoder, const JxlColorEncoding &wanted) noexcept {
        JxlColorEncoding current = {};
        if (JxlDecoderGetColorAsEncodedProfile(&decoder, JXL_COLOR_PROFILE_TARGET_DATA, &current) != JXL_DEC_SUCCESS) {
            return false;
        }
        return same_encoding(current, wanted);
    }

    /// <summary>
    /// True when an encoded profile has a known transfer function libjxl can render into.
    /// </summary>
    [[nodiscard]] bool is_renderable(const JxlColorEncoding &encoding) noexcept {
        return encoding.color_space != JXL_COLOR_SPACE_UNKNOWN && encoding.color_space != JXL_COLOR_SPACE_XYB &&
          encoding.transfer_function != JXL_TRANSFER_FUNCTION_UNKNOWN;
    }

    /// <summary>
    /// The file's own encoding and the one the output kind wants: linear light for the HDR output, and for an SRGB
    /// texture of PQ or HLG until it is scaled (display light: libjxl's 1.0 is the intensity target, the engine's is
    /// reference white); sRGB for any other SRGB texture.
    /// </summary>
    [[nodiscard]] colour_target colour_target_of(
      const JxlDecoder &decoder,
      const JxlBasicInfo &info,
      output_kind kind
    ) noexcept {
        colour_target target = {};
        const JxlDecoderStatus status =
          JxlDecoderGetColorAsEncodedProfile(&decoder, JXL_COLOR_PROFILE_TARGET_ORIGINAL, &target.original);
        target.encoded = status == JXL_DEC_SUCCESS;
        target.renderable = target.encoded && is_renderable(target.original);
        const bool displayLight = target.encoded &&
          (target.original.transfer_function == JXL_TRANSFER_FUNCTION_PQ ||
           target.original.transfer_function == JXL_TRANSFER_FUNCTION_HLG);
        target.rescales = displayLight && kind != output_kind::RGBA8_AS_STORED;
        const bool linear = kind == output_kind::RGB_LINEAR_FLOAT || target.rescales;
        target.wanted = standard_encoding(info.num_color_channels == 1, linear);
        return target;
    }

    /// <summary>
    /// True when a colour conversion has to follow the division of premultiplied alpha: the output is neither the
    /// stored encoding (LINEAR) nor a wanted encoding the file is already in.
    /// </summary>
    [[nodiscard]] bool converts_after_division(output_kind kind, const colour_target &target) noexcept {
        if (kind == output_kind::RGBA8_AS_STORED) {
            return false;
        }
        return !(target.renderable && same_encoding(target.original, target.wanted));
    }

    /// <summary>
    /// True when libjxl's own unpremultiply is right for a premultiplied file: libjxl divides in its output stage,
    /// after every colour stage, so only when nothing converts afterwards and libjxl can produce the stored encoding
    /// (a lossless file, or a lossy one rendered back into an encoded profile). It divides only in an output with
    /// an alpha channel, which the HDR output (RGB floats) is not.
    /// </summary>
    [[nodiscard]] bool libjxl_can_unpremultiply(
      const JxlBasicInfo &info,
      output_kind kind,
      const colour_target &target
    ) noexcept {
        if (kind == output_kind::RGB_LINEAR_FLOAT || converts_after_division(kind, target)) {
            return false;
        }
        return info.uses_original_profile == JXL_TRUE || target.renderable;
    }

    [[nodiscard]] core::result<std::vector<uint8_t>> read_icc(
      const JxlDecoder &decoder,
      JxlColorProfileTarget target,
      std::string_view caller
    ) {
        size_t size = 0;
        if (JxlDecoderGetICCProfileSize(&decoder, target, &size) != JXL_DEC_SUCCESS || size == 0) {
            return decode_failure(caller, "the colour profile has no ICC form");
        }
        std::vector<uint8_t> icc(size);
        if (JxlDecoderGetColorAsICCProfile(&decoder, target, icc.data(), icc.size()) != JXL_DEC_SUCCESS) {
            return decode_failure(caller, "the ICC profile is unreadable");
        }
        return icc;
    }

    /// <summary>
    /// The ICC profile libjxl writes for an encoded colour space, for the colour management system. libjxl hands
    /// such a profile out only as a decoder's output profile, and only an XYB-encoded image's output follows the
    /// one requested, so a decoder over a header-only XYB codestream is asked.
    /// </summary>
    [[nodiscard]] core::result<std::vector<uint8_t>> profile_icc(
      const JxlColorEncoding &encoding,
      std::string_view caller
    ) {
        const JxlMemoryManager memory = detail::jpeg_xl_memory_manager();
        const std::unique_ptr<JxlDecoder, decoder_deleter> decoder(JxlDecoderCreate(&memory));
        if (decoder == nullptr) {
            return decode_failure(caller, "the decoder could not be created");
        }
        const bool configured = JxlDecoderSubscribeEvents(decoder.get(), JXL_DEC_COLOR_ENCODING) == JXL_DEC_SUCCESS &&
          JxlDecoderSetInput(decoder.get(), PROFILE_ONLY_CODESTREAM.data(), PROFILE_ONLY_CODESTREAM.size()) ==
            JXL_DEC_SUCCESS;
        if (!configured) {
            return decode_failure(caller, "the decoder refused its settings");
        }
        JxlDecoderCloseInput(decoder.get());
        if (JxlDecoderProcessInput(decoder.get()) != JXL_DEC_COLOR_ENCODING ||
          JxlDecoderSetOutputColorProfile(decoder.get(), &encoding, nullptr, 0) != JXL_DEC_SUCCESS) {
            return decode_failure(caller, "libjxl wrote no ICC profile for the output colour space");
        }
        return read_icc(*decoder, JXL_COLOR_PROFILE_TARGET_DATA, caller);
    }

    /*-----------------------------------------------------------------------*/
    // Limits on what the header asks for

    /// <summary>
    /// The size and extra channels of the basic info, checked before libjxl reads a frame.
    /// </summary>
    [[nodiscard]] core::result<void> check_basic_info(const JxlBasicInfo &info, std::string_view caller) {
        // The orientation is applied, so xsize x ysize is already the size of the output.
        if (info.xsize > texture_asset::MAX_DIMENSION || info.ysize > texture_asset::MAX_DIMENSION) {
            return core::make_error(
              core::error_code::INVALID_ARGUMENT,
              "{}: {}x{} exceeds the {} texel limit",
              caller,
              info.xsize,
              info.ysize,
              texture_asset::MAX_DIMENSION
            );
        }
        if (info.num_extra_channels > detail::JPEG_XL_MAX_EXTRA_CHANNELS) {
            return core::make_error(
              core::error_code::INVALID_ARGUMENT,
              "{}: JPEG XL: {} extra channels exceed the limit of {}",
              caller,
              info.num_extra_channels,
              detail::JPEG_XL_MAX_EXTRA_CHANNELS
            );
        }
        return core::result<void>();
    }

    /// <summary>
    /// libjxl's memory budget for decoding the image of 'info'. The thread term counts JPEG_XL_MAX_THREADS whatever
    /// the job system has, so the limit, and with it whether a file decodes, is the same inline and on workers.
    /// </summary>
    [[nodiscard]] u64 memory_budget_for(const JxlBasicInfo &info) noexcept {
        const u64 channels = static_cast<u64>(info.num_color_channels) + info.num_extra_channels;
        const u64 samples = static_cast<u64>(info.xsize) * info.ysize * channels;
        return detail::JPEG_XL_BUDGET_BASE_BYTES + detail::JPEG_XL_BUDGET_THREADS_BYTES +
          samples * detail::JPEG_XL_BUDGET_SAMPLE_BYTES;
    }

    [[nodiscard]] core::error budget_failure(const detail::jpeg_xl_memory_budget &budget, std::string_view caller) {
        return core::make_error(
          core::error_code::INVALID_ARGUMENT,
          "{}: JPEG XL: decoding needs more than the {} MiB memory limit of this image",
          caller,
          budget.limitBytes.load(std::memory_order_relaxed) / BYTES_PER_MEBIBYTE
        );
    }

    /// <summary>
    /// Reads the basic info, the colour encoding and every frame header without decoding a pixel: coalescing off,
    /// so each layer reports its own size, and no image event, so libjxl allocates no frame state and skips the
    /// frames' data. Refuses a layer wider or taller than MAX_DIMENSION, and decides whether libjxl may divide a
    /// premultiplied file for 'kind'. A truncated or corrupt file ends the pass quietly, leaving the division to
    /// the engine; the decode itself reports it. Reference-only and DC frames are never reported; the decode's
    /// memory budget bounds them.
    /// </summary>
    [[nodiscard]] core::result<header_facts> read_headers(
      std::span<const std::byte> encoded,
      output_kind kind,
      std::string_view caller
    ) {
        detail::jpeg_xl_memory_budget budget = {};
        budget.limitBytes.store(
          detail::JPEG_XL_BUDGET_BASE_BYTES + detail::JPEG_XL_BUDGET_THREADS_BYTES,
          std::memory_order_relaxed
        );
        const JxlMemoryManager memory = detail::jpeg_xl_memory_manager(&budget);
        const std::unique_ptr<JxlDecoder, decoder_deleter> decoder(JxlDecoderCreate(&memory));
        if (decoder == nullptr) {
            return decode_failure(caller, "the decoder could not be created");
        }
        constexpr int EVENTS = JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING | JXL_DEC_FRAME;
        const bool configured = JxlDecoderSubscribeEvents(decoder.get(), EVENTS) == JXL_DEC_SUCCESS &&
          JxlDecoderSetCoalescing(decoder.get(), JXL_FALSE) == JXL_DEC_SUCCESS &&
          JxlDecoderSetInput(decoder.get(), reinterpret_cast<const uint8_t *>(encoded.data()), encoded.size()) ==
            JXL_DEC_SUCCESS;
        if (!configured) {
            return decode_failure(caller, "the decoder refused its settings");
        }

        header_facts facts = {};
        JxlBasicInfo info = {};
        while (true) {
            const JxlDecoderStatus status = JxlDecoderProcessInput(decoder.get());
            if (status == JXL_DEC_BASIC_INFO) {
                if (JxlDecoderGetBasicInfo(decoder.get(), &info) != JXL_DEC_SUCCESS) {
                    return facts;
                }
                CONJURE_TRY(check_basic_info(info, caller));
                budget.limitBytes.store(memory_budget_for(info), std::memory_order_relaxed);
                continue;
            }
            if (status == JXL_DEC_COLOR_ENCODING) {
                const colour_target target = colour_target_of(*decoder, info, kind);
                facts.libjxlUnpremultiplies =
                  has_premultiplied_alpha(info) && libjxl_can_unpremultiply(info, kind, target);
                continue;
            }
            if (status == JXL_DEC_FRAME) {
                JxlFrameHeader header = {};
                if (JxlDecoderGetFrameHeader(decoder.get(), &header) != JXL_DEC_SUCCESS) {
                    return facts;
                }
                const JxlLayerInfo &layer = header.layer_info;
                if (layer.xsize > texture_asset::MAX_DIMENSION || layer.ysize > texture_asset::MAX_DIMENSION) {
                    return core::make_error(
                      core::error_code::INVALID_ARGUMENT,
                      "{}: JPEG XL: a {}x{} layer exceeds the {} texel limit",
                      caller,
                      layer.xsize,
                      layer.ysize,
                      texture_asset::MAX_DIMENSION
                    );
                }
                continue;
            }
            if (budget.exceeded.load(std::memory_order_relaxed)) {
                return budget_failure(budget, caller);
            }
            return facts;
        }
    }

    /*-----------------------------------------------------------------------*/
    // The colour plan

    /// <summary>
    /// A lossy (XYB) file has to be converted to some RGB space; for a texture whose samples are data, that is the
    /// space the file was made from. libjxl renders into an encoded profile with a known transfer function; any
    /// other profile is reached by the engine's conversion afterwards (colour_plan).
    /// </summary>
    [[nodiscard]] core::result<void> request_original_encoding(
      JxlDecoder &decoder,
      const JxlBasicInfo &info,
      colour_plan &plan,
      std::string_view caller
    ) {
        JxlColorEncoding original = {};
        const bool encoded =
          JxlDecoderGetColorAsEncodedProfile(&decoder, JXL_COLOR_PROFILE_TARGET_ORIGINAL, &original) == JXL_DEC_SUCCESS;
        if (encoded && is_renderable(original)) {
            if (outputs_already(decoder, original)) {
                return core::result<void>();
            }
            if (JxlDecoderSetOutputColorProfile(&decoder, &original, nullptr, 0) != JXL_DEC_SUCCESS) {
                return decode_failure(caller, "the file's own colour encoding cannot be reproduced");
            }
            return core::result<void>();
        }

        CONJURE_TRY_ASSIGN(plan.originalIcc, read_icc(decoder, JXL_COLOR_PROFILE_TARGET_ORIGINAL, caller));
        const JxlColorEncoding linear = standard_encoding(info.num_color_channels == 1, true);
        if (JxlDecoderSetOutputColorProfile(&decoder, &linear, nullptr, 0) != JXL_DEC_SUCCESS) {
            return decode_failure(caller, "the colour profile cannot be converted");
        }
        CONJURE_TRY_ASSIGN(plan.decodedIcc, read_icc(decoder, JXL_COLOR_PROFILE_TARGET_DATA, caller));
        plan.engineFinishes = true;
        return core::result<void>();
    }

    /// <summary>
    /// Premultiplied alpha belongs to the colour as the file stores it, so it is divided out in that encoding.
    /// When nothing converts after the division (libjxl_can_unpremultiply, decided by the header pass because
    /// libjxl takes the option only before it starts), libjxl renders a lossy file back into the stored encoding,
    /// divides and writes the output itself, integers included. Otherwise libjxl outputs that encoding untouched as
    /// floats (a lossy file rendered back into it, or into linear sRGB and converted to its ICC profile) and the
    /// engine divides before converting to 'wanted'. A LINEAR texture stays in the file's encoding.
    /// </summary>
    [[nodiscard]] core::result<void> plan_premultiplied(
      JxlDecoder &decoder,
      const JxlBasicInfo &info,
      output_kind kind,
      const colour_target &target,
      bool libjxlUnpremultiplies,
      colour_plan &plan,
      std::string_view caller
    ) {
        const bool lossless = info.uses_original_profile == JXL_TRUE;
        if (libjxlUnpremultiplies) {
            CONJURE_ASSERT(
              libjxl_can_unpremultiply(info, kind, target),
              "the header pass and the decode see the same colour encoding"
            );
            const bool rendered = lossless || outputs_already(decoder, target.original) ||
              JxlDecoderSetOutputColorProfile(&decoder, &target.original, nullptr, 0) == JXL_DEC_SUCCESS;
            if (!rendered) {
                return decode_failure(caller, "the file's own colour encoding cannot be reproduced");
            }
            return core::result<void>();
        }

        plan.engineFinishes = true;
        plan.unpremultiply = true;
        if (!lossless) {
            if (target.renderable) {
                const bool rendered = outputs_already(decoder, target.original) ||
                  JxlDecoderSetOutputColorProfile(&decoder, &target.original, nullptr, 0) == JXL_DEC_SUCCESS;
                if (!rendered) {
                    return decode_failure(caller, "the file's own colour encoding cannot be reproduced");
                }
            } else {
                const JxlColorEncoding linear = standard_encoding(info.num_color_channels == 1, true);
                if (JxlDecoderSetOutputColorProfile(&decoder, &linear, nullptr, 0) != JXL_DEC_SUCCESS) {
                    return decode_failure(caller, "the colour profile cannot be converted");
                }
                CONJURE_TRY_ASSIGN(plan.decodedIcc, read_icc(decoder, JXL_COLOR_PROFILE_TARGET_DATA, caller));
            }
        }

        const bool convertsToWanted = converts_after_division(kind, target);
        if (!plan.decodedIcc.empty() || convertsToWanted) {
            CONJURE_TRY_ASSIGN(plan.originalIcc, read_icc(decoder, JXL_COLOR_PROFILE_TARGET_ORIGINAL, caller));
        }
        if (convertsToWanted) {
            CONJURE_TRY_ASSIGN(plan.wantedIcc, profile_icc(target.wanted, caller));
        }
        return core::result<void>();
    }

    /// <summary>
    /// Chooses the colour encoding of the output once the file's own is known (JXL_DEC_COLOR_ENCODING).
    /// 'libjxlUnpremultiplies' is what the decoder was configured with (header_facts).
    /// </summary>
    [[nodiscard]] core::result<void> choose_output_encoding(
      JxlDecoder &decoder,
      const JxlBasicInfo &info,
      output_kind kind,
      bool libjxlUnpremultiplies,
      colour_plan &plan,
      std::string_view caller
    ) {
        const colour_target target = colour_target_of(decoder, info, kind);
        if (target.rescales) {
            plan.luminanceScale = info.intensity_target / detail::JPEG_XL_REFERENCE_WHITE_NITS;
            plan.encodeSrgb = kind == output_kind::RGBA8_SRGB;
            plan.engineFinishes = kind == output_kind::RGBA8_SRGB;
        }
        if (has_premultiplied_alpha(info)) {
            return plan_premultiplied(decoder, info, kind, target, libjxlUnpremultiplies, plan, caller);
        }
        if (kind == output_kind::RGBA8_AS_STORED) {
            if (info.uses_original_profile == JXL_TRUE) {
                return core::result<void>();
            }
            return request_original_encoding(decoder, info, plan, caller);
        }
        if (outputs_already(decoder, target.wanted)) {
            return core::result<void>();
        }
        if (JxlDecoderSetOutputColorProfile(&decoder, &target.wanted, nullptr, 0) != JXL_DEC_SUCCESS) {
            return decode_failure(caller, "the colour profile cannot be converted");
        }
        return core::result<void>();
    }

    /*-----------------------------------------------------------------------*/
    // Output

    /// <summary>
    /// True when libjxl's RGBA8 output goes through quantise_scanline: a channel deeper than 8 bits keeps its high
    /// byte, which libjxl's own 8-bit output (rounded after an ordered dither) would not give.
    /// </summary>
    [[nodiscard]] bool quantises_scanlines(
      const JxlBasicInfo &info,
      output_kind kind,
      const colour_plan &plan
    ) noexcept {
        if (plan.engineFinishes || kind == output_kind::RGB_LINEAR_FLOAT) {
            return false;
        }
        return colour_is_deep(info) || alpha_is_deep(info);
    }

    [[nodiscard]] JxlPixelFormat output_format(const JxlBasicInfo &info, output_kind kind, const colour_plan &plan) {
        JxlPixelFormat format = {};
        format.endianness = JXL_NATIVE_ENDIAN;
        format.align = 0;
        format.num_channels = 4;
        format.data_type = JXL_TYPE_FLOAT;
        if (plan.engineFinishes || quantises_scanlines(info, kind, plan)) {
            return format;
        }
        if (kind == output_kind::RGB_LINEAR_FLOAT) {
            format.num_channels = 3;
            return format;
        }
        format.data_type = JXL_TYPE_UINT8;
        return format;
    }

    /// <summary>
    /// Image-out callback of a file with a channel deeper than 8 bits: writes RGBA8, colour and alpha each
    /// quantised by its own depth, as finish_in_engine does (the high byte of a deep channel, stb_image's reduction
    /// of a 16-bit PNG; an 8-bit channel rounded, also where libjxl converted it). libjxl may call it from several
    /// threads at once, each time for different pixels.
    /// </summary>
    void quantise_scanline(void *opaque, size_t x, size_t y, size_t pixelCount, const void *pixels) {
        const quantised_target &target = *static_cast<const quantised_target *>(opaque);
        const f32 *source = static_cast<const f32 *>(pixels);
        std::byte *destination = target.rgba + (y * target.width + x) * 4;
        for (usize pixel = 0; pixel < pixelCount; ++pixel) {
            for (usize channel = 0; channel < 3; ++channel) {
                destination[pixel * 4 + channel] = quantise(source[pixel * 4 + channel], target.colourDeep);
            }
            destination[pixel * 4 + 3] = quantise(source[pixel * 4 + 3], target.alphaDeep);
        }
    }

    /// <summary>
    /// Hands libjxl the buffer of the first frame (JXL_DEC_NEED_IMAGE_OUT_BUFFER), or the scanline callback that
    /// writes into it. The pixels are allocated only here, after the size check on the basic info.
    /// </summary>
    [[nodiscard]] core::result<void> provide_output(
      JxlDecoder &decoder,
      const JxlBasicInfo &info,
      output_kind kind,
      const colour_plan &plan,
      decoded_frame &frame,
      quantised_target &quantised,
      std::string_view caller
    ) {
        const JxlPixelFormat format = output_format(info, kind, plan);
        const usize sampleCount = static_cast<usize>(info.xsize) * info.ysize * format.num_channels;
        frame.width = info.xsize;
        frame.height = info.ysize;
        if (quantises_scanlines(info, kind, plan)) {
            frame.rgba.resize(sampleCount);
            quantised.rgba = frame.rgba.data();
            quantised.width = info.xsize;
            quantised.colourDeep = colour_is_deep(info);
            quantised.alphaDeep = alpha_is_deep(info);
            if (JxlDecoderSetImageOutCallback(&decoder, &format, &quantise_scanline, &quantised) != JXL_DEC_SUCCESS) {
                return decode_failure(caller, "the output callback was refused");
            }
            return core::result<void>();
        }
        if (format.data_type == JXL_TYPE_FLOAT) {
            frame.floats.resize(sampleCount);
            const usize bytes = frame.floats.size() * sizeof(f32);
            if (JxlDecoderSetImageOutBuffer(&decoder, &format, frame.floats.data(), bytes) != JXL_DEC_SUCCESS) {
                return decode_failure(caller, "the output buffer was refused");
            }
            return core::result<void>();
        }
        frame.rgba.resize(sampleCount);
        if (JxlDecoderSetImageOutBuffer(&decoder, &format, frame.rgba.data(), frame.rgba.size()) != JXL_DEC_SUCCESS) {
            return decode_failure(caller, "the output buffer was refused");
        }
        return core::result<void>();
    }

    /*-----------------------------------------------------------------------*/
    // The engine's steps (colour_plan)

    struct cms_state_deleter {
        jpegxl_cms_destroy_func destroy = nullptr;

        void operator()(void *state) const noexcept {
            destroy(state);
        }
    };

    /// <summary>
    /// One conversion between two ICC profiles through the default colour management system (skcms), a row at a
    /// time.
    /// </summary>
    struct cms_conversion {
        std::unique_ptr<void, cms_state_deleter> state = {nullptr, cms_state_deleter{}};
        usize channels = 0;
    };

    [[nodiscard]] core::result<JxlColorProfile> describe_profile(
      const std::vector<uint8_t> &icc,
      usize channels,
      std::string_view caller
    ) {
        const JxlCmsInterface &cms = *JxlGetDefaultCms();
        JxlColorProfile profile = {};
        profile.icc.data = icc.data();
        profile.icc.size = icc.size();
        profile.num_channels = channels;
        JXL_BOOL cmyk = JXL_FALSE;
        const JXL_BOOL parsed =
          cms.set_fields_from_icc(cms.set_fields_data, icc.data(), icc.size(), &profile.color_encoding, &cmyk);
        if (parsed != JXL_TRUE || cmyk == JXL_TRUE) {
            return decode_failure(caller, "the ICC profile cannot be converted");
        }
        return profile;
    }

    [[nodiscard]] core::result<cms_conversion> start_conversion(
      const std::vector<uint8_t> &fromIcc,
      const std::vector<uint8_t> &toIcc,
      usize channels,
      usize width,
      f32 intensityTarget,
      std::string_view caller
    ) {
        const JxlCmsInterface &cms = *JxlGetDefaultCms();
        JxlColorProfile input = {};
        CONJURE_TRY_ASSIGN(input, describe_profile(fromIcc, channels, caller));
        JxlColorProfile output = {};
        CONJURE_TRY_ASSIGN(output, describe_profile(toIcc, channels, caller));
        cms_conversion conversion = {};
        conversion.state = std::unique_ptr<void, cms_state_deleter>(
          cms.init(cms.init_data, 1, width, &input, &output, intensityTarget),
          cms_state_deleter{cms.destroy}
        );
        if (conversion.state == nullptr) {
            return decode_failure(caller, "the colour management system refused the ICC profile");
        }
        conversion.channels = channels;
        return conversion;
    }

    /// <summary>
    /// Converts the colour of one row of RGBA floats in place (grey: R in, R, G and B out); alpha is untouched.
    /// </summary>
    [[nodiscard]] core::result<void> convert_row(
      const cms_conversion &conversion,
      f32 *row,
      usize width,
      std::string_view caller
    ) {
        const JxlCmsInterface &cms = *JxlGetDefaultCms();
        f32 *source = cms.get_src_buf(conversion.state.get(), 0);
        f32 *converted = cms.get_dst_buf(conversion.state.get(), 0);
        const usize channels = conversion.channels;
        for (usize x = 0; x < width; ++x) {
            for (usize channel = 0; channel < channels; ++channel) {
                source[x * channels + channel] = row[x * 4 + channel];
            }
        }
        if (cms.run(conversion.state.get(), 0, source, converted, width) != JXL_TRUE) {
            return decode_failure(caller, "the colour management system failed");
        }
        for (usize x = 0; x < width; ++x) {
            for (usize channel = 0; channel < 3; ++channel) {
                row[x * 4 + channel] = converted[x * channels + (channels == 1 ? 0 : channel)];
            }
        }
        return core::result<void>();
    }

    /// <summary>
    /// Straight colour from premultiplied colour, as libjxl's own output stage divides.
    /// </summary>
    void unpremultiply_row(f32 *row, usize width) noexcept {
        for (usize x = 0; x < width; ++x) {
            const f32 multiplier = 1.0f / std::max(SMALLEST_ALPHA, row[x * 4 + 3]);
            for (usize channel = 0; channel < 3; ++channel) {
                row[x * 4 + channel] *= multiplier;
            }
        }
    }

    /// <summary>
    /// Runs the engine's steps of 'plan' over the RGBA floats libjxl output, then leaves RGB floats (HDR) or RGBA8
    /// bytes in 'frame'.
    /// </summary>
    [[nodiscard]] core::result<void> finish_in_engine(
      decoded_frame &frame,
      const JxlBasicInfo &info,
      const colour_plan &plan,
      output_kind kind,
      std::string_view caller
    ) {
        const usize width = frame.width;
        const usize pixelCount = width * frame.height;
        const usize channels = info.num_color_channels == 1 ? 1 : 3;
        std::optional<cms_conversion> toOriginal = std::nullopt;
        if (!plan.decodedIcc.empty()) {
            CONJURE_TRY_ASSIGN(
              toOriginal,
              start_conversion(plan.decodedIcc, plan.originalIcc, channels, width, info.intensity_target, caller)
            );
        }
        std::optional<cms_conversion> toWanted = std::nullopt;
        if (!plan.wantedIcc.empty()) {
            CONJURE_TRY_ASSIGN(
              toWanted,
              start_conversion(plan.originalIcc, plan.wantedIcc, channels, width, info.intensity_target, caller)
            );
        }

        for (usize y = 0; y < frame.height; ++y) {
            f32 *row = frame.floats.data() + y * width * 4;
            if (toOriginal.has_value()) {
                CONJURE_TRY(convert_row(*toOriginal, row, width, caller));
            }
            if (plan.unpremultiply) {
                unpremultiply_row(row, width);
            }
            if (toWanted.has_value()) {
                CONJURE_TRY(convert_row(*toWanted, row, width, caller));
            }
            for (usize x = 0; x < width; ++x) {
                for (usize channel = 0; channel < 3; ++channel) {
                    row[x * 4 + channel] *= plan.luminanceScale;
                }
            }
        }

        if (kind == output_kind::RGB_LINEAR_FLOAT) {
            // In place: pixel p's three floats move down from 4p to 3p, below every pixel still to be read.
            for (usize pixel = 0; pixel < pixelCount; ++pixel) {
                for (usize channel = 0; channel < 3; ++channel) {
                    frame.floats[pixel * 3 + channel] = frame.floats[pixel * 4 + channel];
                }
            }
            frame.floats.resize(pixelCount * 3);
            frame.floats.shrink_to_fit();
            return core::result<void>();
        }

        const bool colourDeep = colour_is_deep(info);
        const bool alphaDeep = alpha_is_deep(info);
        frame.rgba.resize(pixelCount * 4);
        for (usize pixel = 0; pixel < pixelCount; ++pixel) {
            for (usize channel = 0; channel < 3; ++channel) {
                const f32 value = frame.floats[pixel * 4 + channel];
                const f32 encoded = plan.encodeSrgb ? detail::linear_to_srgb(value) : value;
                frame.rgba[pixel * 4 + channel] = quantise(encoded, colourDeep);
            }
            frame.rgba[pixel * 4 + 3] = quantise(frame.floats[pixel * 4 + 3], alphaDeep);
        }
        frame.floats.clear();
        frame.floats.shrink_to_fit();
        return core::result<void>();
    }

    /*-----------------------------------------------------------------------*/
    // The decode

    /// <summary>
    /// Runs libjxl over the whole file and returns its first displayed frame in the output kind's form. Later
    /// frames of an animation are skipped without being output, and counted for the warning.
    /// </summary>
    [[nodiscard]] core::result<decoded_frame> decode_first_frame(
      std::span<const std::byte> encoded,
      output_kind kind,
      core::jobs::job_system *jobs,
      std::string_view caller
    ) {
        if (encoded.empty()) {
            return core::make_error(core::error_code::INVALID_ARGUMENT, "{}: no data", caller);
        }
        if (encoded.size() > static_cast<usize>(std::numeric_limits<int>::max())) {
            return core::make_error(core::error_code::INVALID_ARGUMENT, "{}: file larger than 2 GiB", caller);
        }
        header_facts facts = {};
        CONJURE_TRY_ASSIGN(facts, read_headers(encoded, kind, caller));

        const bool parallel = detail::can_run_jpeg_xl_in_parallel(jobs);
        detail::jpeg_xl_memory_budget budget = {};
        budget.limitBytes.store(
          detail::JPEG_XL_BUDGET_BASE_BYTES + detail::JPEG_XL_BUDGET_THREADS_BYTES,
          std::memory_order_relaxed
        );
        const JxlMemoryManager memory = detail::jpeg_xl_memory_manager(&budget);
        const std::unique_ptr<JxlDecoder, decoder_deleter> decoder(JxlDecoderCreate(&memory));
        if (decoder == nullptr) {
            return decode_failure(caller, "the decoder could not be created");
        }
        const bool runnerSet = !parallel ||
          JxlDecoderSetParallelRunner(decoder.get(), &detail::run_jpeg_xl_on_jobs, jobs) == JXL_DEC_SUCCESS;
        if (!runnerSet) {
            return decode_failure(caller, "the job system runner was refused");
        }
        // libjxl divides premultiplied alpha only when the header pass found that nothing converts afterwards;
        // otherwise the engine does (colour_plan). The input stays open: the whole file is already in it, so libjxl
        // asking for more means the file ends early.
        constexpr int EVENTS = JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING | JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE;
        const JXL_BOOL unpremultiply = facts.libjxlUnpremultiplies ? JXL_TRUE : JXL_FALSE;
        const bool configured = JxlDecoderSubscribeEvents(decoder.get(), EVENTS) == JXL_DEC_SUCCESS &&
          JxlDecoderSetUnpremultiplyAlpha(decoder.get(), unpremultiply) == JXL_DEC_SUCCESS &&
          JxlDecoderSetCms(decoder.get(), *JxlGetDefaultCms()) == JXL_DEC_SUCCESS &&
          JxlDecoderSetInput(decoder.get(), reinterpret_cast<const uint8_t *>(encoded.data()), encoded.size()) ==
            JXL_DEC_SUCCESS;
        if (!configured) {
            return decode_failure(caller, "the decoder refused its settings");
        }

        JxlBasicInfo info = {};
        colour_plan plan = {};
        decoded_frame frame = {};
        quantised_target quantised = {};
        u32 framesSeen = 0;
        while (true) {
            const JxlDecoderStatus status = JxlDecoderProcessInput(decoder.get());
            if (status == JXL_DEC_BASIC_INFO) {
                if (JxlDecoderGetBasicInfo(decoder.get(), &info) != JXL_DEC_SUCCESS) {
                    return decode_failure(caller, "the basic info is unreadable");
                }
                CONJURE_TRY(check_basic_info(info, caller));
                budget.limitBytes.store(memory_budget_for(info), std::memory_order_relaxed);
                continue;
            }
            if (status == JXL_DEC_COLOR_ENCODING) {
                CONJURE_TRY(choose_output_encoding(*decoder, info, kind, facts.libjxlUnpremultiplies, plan, caller));
                continue;
            }
            if (status == JXL_DEC_FRAME) {
                ++framesSeen;
                continue;
            }
            if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
                if (framesSeen > 1) {
                    if (JxlDecoderSkipCurrentFrame(decoder.get()) != JXL_DEC_SUCCESS) {
                        return decode_failure(caller, "a later animation frame could not be skipped");
                    }
                    continue;
                }
                CONJURE_TRY(provide_output(*decoder, info, kind, plan, frame, quantised, caller));
                continue;
            }
            if (status == JXL_DEC_FULL_IMAGE) {
                continue;
            }
            if (status == JXL_DEC_SUCCESS) {
                break;
            }
            if (status == JXL_DEC_NEED_MORE_INPUT) {
                return decode_failure(caller, "the file is truncated");
            }
            if (budget.exceeded.load(std::memory_order_relaxed)) {
                return budget_failure(budget, caller);
            }
            return decode_failure(caller, "the file is corrupt or uses a feature libjxl refuses");
        }

        // A file can end cleanly after its headers when it holds no displayed frame at all.
        if (framesSeen == 0 || (frame.rgba.empty() && frame.floats.empty())) {
            return decode_failure(caller, "the file holds no image");
        }
        if (plan.engineFinishes) {
            CONJURE_TRY(finish_in_engine(frame, info, plan, kind, caller));
        } else if (plan.luminanceScale != 1.0f) {
            for (f32 &value : frame.floats) {
                value *= plan.luminanceScale;
            }
        }
        if (framesSeen > 1) {
            CONJURE_LOG_WARN(
              LOG_RENDERER,
              "{}: animated JPEG XL: kept the first frame, dropped {} later frame(s)",
              caller,
              framesSeen - 1
            );
        }
        return frame;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Public and module functions

bool conjure::renderer::is_jpeg_xl(std::span<const std::byte> bytes) noexcept {
    if (bytes.empty()) {
        return false;
    }
    const JxlSignature signature = JxlSignatureCheck(reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size());
    return signature == JXL_SIG_CODESTREAM || signature == JXL_SIG_CONTAINER;
}

core::result<detail::decoded_image> conjure::renderer::detail::decode_jpeg_xl_rgba8(
  std::span<const std::byte> encoded,
  texture_colour_space colourSpace,
  core::jobs::job_system *jobs
) {
    const output_kind kind =
      colourSpace == texture_colour_space::SRGB ? output_kind::RGBA8_SRGB : output_kind::RGBA8_AS_STORED;
    decoded_frame frame = {};
    CONJURE_TRY_ASSIGN(frame, decode_first_frame(encoded, kind, jobs, "decode_rgba8"));
    decoded_image image = {};
    image.width = frame.width;
    image.height = frame.height;
    image.rgba = std::move(frame.rgba);
    return image;
}

core::result<detail::decoded_hdr_image> conjure::renderer::detail::decode_jpeg_xl_rgb_float(
  std::span<const std::byte> encoded,
  core::jobs::job_system *jobs
) {
    decoded_frame frame = {};
    CONJURE_TRY_ASSIGN(frame, decode_first_frame(encoded, output_kind::RGB_LINEAR_FLOAT, jobs, "decode_hdr_rgb"));
    decoded_hdr_image image = {};
    image.width = frame.width;
    image.height = frame.height;
    image.rgb = std::move(frame.floats);
    return image;
}
