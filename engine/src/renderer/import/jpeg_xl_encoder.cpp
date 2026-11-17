#include "renderer/import/jpeg_xl_codec.h"
#include "renderer/import/jpeg_xl_hooks.h"

#include "conjure/renderer/jpeg_xl.h"
#include "conjure/renderer/texture_asset.h"

#include "conjure/core/error.h"
#include "conjure/core/image.h"
#include "conjure/core/result.h"

#include <jxl/codestream_header.h>
#include <jxl/color_encoding.h>
#include <jxl/encode.h>
#include <jxl/types.h>

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::renderer;

namespace {
    struct encoder_deleter {
        void operator()(JxlEncoder *encoder) const noexcept {
            JxlEncoderDestroy(encoder);
        }
    };

    /// <summary>
    /// Bits per sample and exponent bits of a sample type, as JxlBasicInfo names them.
    /// </summary>
    struct sample_bits {
        uint32_t bits = 8;
        uint32_t exponentBits = 0;
        usize bytes = 1;
        JxlDataType dataType = JXL_TYPE_UINT8;
    };

    // The output grows from this size; a small image fits in one step.
    constexpr usize INITIAL_OUTPUT_BYTES = 64 * 1024;

    // Animation timing: ticks of one millisecond.
    constexpr uint32_t TICKS_PER_SECOND = 1000;

    [[nodiscard]] sample_bits bits_of(detail::jpeg_xl_sample_type type) noexcept {
        sample_bits result = {};
        switch (type) {
            case detail::jpeg_xl_sample_type::UINT8: {
                break;
            }
            case detail::jpeg_xl_sample_type::UINT16: {
                result.bits = 16;
                result.bytes = 2;
                result.dataType = JXL_TYPE_UINT16;
                break;
            }
            case detail::jpeg_xl_sample_type::FLOAT16: {
                result.bits = 16;
                result.exponentBits = 5;
                result.bytes = 2;
                result.dataType = JXL_TYPE_FLOAT16;
                break;
            }
            case detail::jpeg_xl_sample_type::FLOAT32: {
                result.bits = 32;
                result.exponentBits = 8;
                result.bytes = 4;
                result.dataType = JXL_TYPE_FLOAT;
                break;
            }
        }
        return result;
    }

    [[nodiscard]] JxlColorEncoding encoding_of(detail::jpeg_xl_colour_encoding tag, bool grey) noexcept {
        JxlColorEncoding encoding = {};
        encoding.color_space = grey ? JXL_COLOR_SPACE_GRAY : JXL_COLOR_SPACE_RGB;
        encoding.white_point = JXL_WHITE_POINT_D65;
        encoding.primaries = JXL_PRIMARIES_SRGB;
        encoding.transfer_function = JXL_TRANSFER_FUNCTION_SRGB;
        encoding.rendering_intent = JXL_RENDERING_INTENT_RELATIVE;
        switch (tag) {
            case detail::jpeg_xl_colour_encoding::SRGB: {
                break;
            }
            case detail::jpeg_xl_colour_encoding::LINEAR_SRGB: {
                encoding.transfer_function = JXL_TRANSFER_FUNCTION_LINEAR;
                break;
            }
            case detail::jpeg_xl_colour_encoding::DISPLAY_P3: {
                encoding.primaries = JXL_PRIMARIES_P3;
                break;
            }
            case detail::jpeg_xl_colour_encoding::REC2100_PQ: {
                encoding.primaries = JXL_PRIMARIES_2100;
                encoding.transfer_function = JXL_TRANSFER_FUNCTION_PQ;
                break;
            }
            case detail::jpeg_xl_colour_encoding::REC2100_HLG: {
                encoding.primaries = JXL_PRIMARIES_2100;
                encoding.transfer_function = JXL_TRANSFER_FUNCTION_HLG;
                break;
            }
        }
        return encoding;
    }

    [[nodiscard]] core::error libjxl_failure(JxlEncoder &encoder, std::string_view step) {
        return core::make_error(
          core::error_code::BACKEND_FAILURE,
          "encode_jpeg_xl: libjxl refused {} (encoder error {})",
          step,
          static_cast<int>(JxlEncoderGetError(&encoder))
        );
    }

    [[nodiscard]] core::result<void> validate(const detail::jpeg_xl_encode_desc &desc) {
        if (desc.width == 0 || desc.height == 0) {
            return core::make_error(core::error_code::INVALID_ARGUMENT, "encode_jpeg_xl: zero size");
        }
        if (desc.channelCount < 1 || desc.channelCount > 4) {
            return core::make_error(
              core::error_code::INVALID_ARGUMENT,
              "encode_jpeg_xl: {} channels, 1 to 4 expected",
              desc.channelCount
            );
        }
        if (desc.frames.empty()) {
            return core::make_error(core::error_code::INVALID_ARGUMENT, "encode_jpeg_xl: no frame");
        }
        // Frames without a duration are layers of one still image, not an animation.
        if (desc.frames.size() > 1 && desc.frameDurationMilliseconds == 0) {
            return core::make_error(
              core::error_code::INVALID_ARGUMENT,
              "encode_jpeg_xl: animation frames need a duration"
            );
        }
        if (desc.orientation < 1 || desc.orientation > 8) {
            return core::make_error(
              core::error_code::INVALID_ARGUMENT,
              "encode_jpeg_xl: orientation {} is outside 1..8",
              desc.orientation
            );
        }
        const bool hasAlpha = desc.channelCount == 2 || desc.channelCount == 4;
        if (desc.premultipliedAlpha && !hasAlpha) {
            return core::make_error(
              core::error_code::INVALID_ARGUMENT,
              "encode_jpeg_xl: premultiplied alpha without an alpha channel"
            );
        }
        // Written so that NaN fails as well.
        if (!(desc.intensityTarget >= 0.0f)) {
            return core::make_error(
              core::error_code::INVALID_ARGUMENT,
              "encode_jpeg_xl: intensity target {} is negative or NaN",
              desc.intensityTarget
            );
        }
        const sample_bits bits = bits_of(desc.sampleType);
        if (desc.colourBits != 0 && (bits.exponentBits > 0 || desc.colourBits > bits.bits)) {
            return core::make_error(
              core::error_code::INVALID_ARGUMENT,
              "encode_jpeg_xl: {} colour bits need integer samples of at least that many bits",
              desc.colourBits
            );
        }
        if ((desc.layerWidth == 0) != (desc.layerHeight == 0)) {
            return core::make_error(
              core::error_code::INVALID_ARGUMENT,
              "encode_jpeg_xl: a {}x{} layer has a side of zero",
              desc.layerWidth,
              desc.layerHeight
            );
        }
        const bool layered = desc.layerWidth != 0;
        const usize frameWidth = layered ? desc.layerWidth : desc.width;
        const usize frameHeight = layered ? desc.layerHeight : desc.height;
        const usize expected = frameWidth * frameHeight * desc.channelCount * bits.bytes;
        for (usize index = 0; index < desc.frames.size(); ++index) {
            if (desc.frames[index].size() != expected) {
                return core::make_error(
                  core::error_code::INVALID_ARGUMENT,
                  "encode_jpeg_xl: frame {} has {} bytes, {} expected",
                  index,
                  desc.frames[index].size(),
                  expected
                );
            }
        }
        return core::result<void>();
    }

    /// <summary>
    /// The global header: size, sample type, channels, orientation and, for several frames, the animation.
    /// A lossless file keeps its original colour space (no XYB), which bit-exact samples need.
    /// </summary>
    [[nodiscard]] JxlBasicInfo basic_info_of(const detail::jpeg_xl_encode_desc &desc) noexcept {
        const sample_bits bits = bits_of(desc.sampleType);
        const bool hasAlpha = desc.channelCount == 2 || desc.channelCount == 4;
        JxlBasicInfo info = {};
        JxlEncoderInitBasicInfo(&info);
        info.xsize = desc.width;
        info.ysize = desc.height;
        info.bits_per_sample = desc.colourBits != 0 ? desc.colourBits : bits.bits;
        info.exponent_bits_per_sample = bits.exponentBits;
        info.intensity_target = desc.intensityTarget;
        info.num_color_channels = desc.channelCount >= 3 ? 3 : 1;
        info.num_extra_channels = hasAlpha ? 1 : 0;
        info.alpha_bits = hasAlpha ? bits.bits : 0;
        info.alpha_exponent_bits = hasAlpha ? bits.exponentBits : 0;
        info.alpha_premultiplied = desc.premultipliedAlpha ? JXL_TRUE : JXL_FALSE;
        info.uses_original_profile = desc.lossless ? JXL_TRUE : JXL_FALSE;
        info.orientation = static_cast<JxlOrientation>(desc.orientation);
        if (desc.frames.size() > 1) {
            info.have_animation = JXL_TRUE;
            info.animation.tps_numerator = TICKS_PER_SECOND;
            info.animation.tps_denominator = 1;
            info.animation.num_loops = 0;
        }
        return info;
    }

    /// <summary>
    /// Effort and quality of every frame. A lossy file keeps its alpha lossless: cut-out edges and masks stay
    /// exact while the colour is compressed.
    /// </summary>
    [[nodiscard]] core::result<void> configure_frames(
      JxlEncoder &encoder,
      JxlEncoderFrameSettings &settings,
      const detail::jpeg_xl_encode_desc &desc
    ) {
        if (JxlEncoderFrameSettingsSetOption(&settings, JXL_ENC_FRAME_SETTING_EFFORT, desc.effort) != JXL_ENC_SUCCESS) {
            return libjxl_failure(encoder, "the effort");
        }
        if (desc.lossless) {
            if (JxlEncoderSetFrameLossless(&settings, JXL_TRUE) != JXL_ENC_SUCCESS) {
                return libjxl_failure(encoder, "lossless mode");
            }
            return core::result<void>();
        }
        if (JxlEncoderSetFrameDistance(&settings, desc.distance) != JXL_ENC_SUCCESS) {
            return libjxl_failure(encoder, "the distance");
        }
        const bool hasAlpha = desc.channelCount == 2 || desc.channelCount == 4;
        if (hasAlpha && JxlEncoderSetExtraChannelDistance(&settings, 0, 0.0f) != JXL_ENC_SUCCESS) {
            return libjxl_failure(encoder, "lossless alpha");
        }
        return core::result<void>();
    }

    /// <summary>
    /// Drains the encoder into a growing buffer once every frame is in.
    /// </summary>
    [[nodiscard]] core::result<std::vector<std::byte>> collect_output(JxlEncoder &encoder) {
        std::vector<std::byte> output(INITIAL_OUTPUT_BYTES);
        usize written = 0;
        while (true) {
            uint8_t *next = reinterpret_cast<uint8_t *>(output.data() + written);
            size_t available = output.size() - written;
            const JxlEncoderStatus status = JxlEncoderProcessOutput(&encoder, &next, &available);
            written = output.size() - available;
            if (status == JXL_ENC_SUCCESS) {
                output.resize(written);
                return output;
            }
            if (status != JXL_ENC_NEED_MORE_OUTPUT) {
                return libjxl_failure(encoder, "the image data");
            }
            output.resize(output.size() * 2);
        }
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Module and public functions

core::result<std::vector<std::byte>> conjure::renderer::detail::encode_jpeg_xl(
  const jpeg_xl_encode_desc &desc,
  core::jobs::job_system *jobs
) {
    CONJURE_TRY(validate(desc));

    const JxlMemoryManager memory = jpeg_xl_memory_manager();
    const std::unique_ptr<JxlEncoder, encoder_deleter> encoder(JxlEncoderCreate(&memory));
    if (encoder == nullptr) {
        return core::make_error(core::error_code::BACKEND_FAILURE, "encode_jpeg_xl: the encoder could not be created");
    }
    if (can_run_jpeg_xl_in_parallel(jobs) &&
      JxlEncoderSetParallelRunner(encoder.get(), &run_jpeg_xl_on_jobs, jobs) != JXL_ENC_SUCCESS) {
        return libjxl_failure(*encoder, "the job system runner");
    }

    const JxlBasicInfo info = basic_info_of(desc);
    if (JxlEncoderSetBasicInfo(encoder.get(), &info) != JXL_ENC_SUCCESS) {
        return libjxl_failure(*encoder, "the basic info");
    }
    if (!desc.iccProfile.empty()) {
        const uint8_t *icc = reinterpret_cast<const uint8_t *>(desc.iccProfile.data());
        if (JxlEncoderSetICCProfile(encoder.get(), icc, desc.iccProfile.size()) != JXL_ENC_SUCCESS) {
            return libjxl_failure(*encoder, "the ICC profile");
        }
    } else {
        const JxlColorEncoding colour = encoding_of(desc.colourEncoding, info.num_color_channels == 1);
        if (JxlEncoderSetColorEncoding(encoder.get(), &colour) != JXL_ENC_SUCCESS) {
            return libjxl_failure(*encoder, "the colour encoding");
        }
    }
    JxlEncoderFrameSettings *settings = JxlEncoderFrameSettingsCreate(encoder.get(), nullptr);
    if (settings == nullptr) {
        return libjxl_failure(*encoder, "the frame settings");
    }
    CONJURE_TRY(configure_frames(*encoder, *settings, desc));

    JxlPixelFormat format = {};
    format.num_channels = desc.channelCount;
    format.data_type = bits_of(desc.sampleType).dataType;
    format.endianness = JXL_NATIVE_ENDIAN;
    format.align = 0;
    for (const std::span<const std::byte> &frame : desc.frames) {
        const bool layered = desc.layerWidth != 0;
        if (info.have_animation == JXL_TRUE || layered) {
            JxlFrameHeader header = {};
            JxlEncoderInitFrameHeader(&header);
            header.duration = info.have_animation == JXL_TRUE ? desc.frameDurationMilliseconds : 0;
            header.layer_info.have_crop = layered ? JXL_TRUE : JXL_FALSE;
            header.layer_info.xsize = desc.layerWidth;
            header.layer_info.ysize = desc.layerHeight;
            if (JxlEncoderSetFrameHeader(settings, &header) != JXL_ENC_SUCCESS) {
                return libjxl_failure(*encoder, "the frame header");
            }
        }
        if (JxlEncoderAddImageFrame(settings, &format, frame.data(), frame.size()) != JXL_ENC_SUCCESS) {
            return libjxl_failure(*encoder, "a frame");
        }
    }
    JxlEncoderCloseInput(encoder.get());
    return collect_output(*encoder);
}

core::result<std::vector<std::byte>> conjure::renderer::encode_jpeg_xl(
  const core::image &image,
  const jpeg_xl_encode_options &options,
  core::jobs::job_system *jobs
) {
    if (image.is_empty()) {
        return core::make_error(core::error_code::INVALID_ARGUMENT, "encode_jpeg_xl: the image is empty");
    }
    if (options.effort < 1 || options.effort > 10) {
        return core::make_error(
          core::error_code::INVALID_ARGUMENT,
          "encode_jpeg_xl: effort {} is outside 1..10",
          options.effort
        );
    }
    // Written so that NaN fails as well.
    const bool distanceInRange = options.distance > 0.0f && options.distance <= 25.0f;
    if (!options.lossless && !distanceInRange) {
        return core::make_error(
          core::error_code::INVALID_ARGUMENT,
          "encode_jpeg_xl: distance {} is outside (0, 25]",
          options.distance
        );
    }

    detail::jpeg_xl_encode_desc desc = {};
    desc.width = image.width();
    desc.height = image.height();
    desc.channelCount = 4;
    desc.sampleType = detail::jpeg_xl_sample_type::UINT8;
    desc.colourEncoding = options.colourSpace == texture_colour_space::SRGB ? detail::jpeg_xl_colour_encoding::SRGB
      : detail::jpeg_xl_colour_encoding::LINEAR_SRGB;
    desc.lossless = options.lossless;
    desc.distance = options.distance;
    desc.effort = options.effort;
    desc.frames.push_back(image.pixels());
    return detail::encode_jpeg_xl(desc, jobs);
}
