#include "conjure/testing/doctest.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/temp_directory.h"

#include "renderer/import/jpeg_xl_codec.h"
#include "renderer/import/mip_generator.h"

#include "conjure/renderer/environment_asset.h"
#include "conjure/renderer/jpeg_xl.h"
#include "conjure/renderer/renderer_error.h"
#include "conjure/renderer/texture_asset.h"

#include "conjure/core/error.h"
#include "conjure/core/image.h"
#include "conjure/core/jobs/job_system.h"
#include "conjure/core/log.h"
#include "conjure/core/memory/allocation_counters.h"
#include "conjure/core/types.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// jpeg_xl.h (docs/design/02_renderer.md 3.13): the JPEG XL signature check and encoder, and the decoder behind
// texture_asset::decode and equirect_image::decode_hdr (renderer/import/jpeg_xl_codec.h). Every file is encoded
// in-test, through the public encoder or the private one that writes every pixel layout; no binary fixture.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;

// The renderer's private codec; conjure::testing has a detail namespace of its own.
namespace codec = conjure::renderer::detail;

namespace {

    /*-----------------------------------------------------------------------*/
    // Fixtures

    /// <summary>
    /// The 8-bit value of a channel of pixel (x, y) in the test pattern: neighbouring pixels and channels differ.
    /// </summary>
    [[nodiscard]] u8 pattern_value(u32 x, u32 y, u32 channel) noexcept {
        return static_cast<u8>((x * 37u + y * 101u + channel * 67u + 13u) % 256u);
    }

    /// <summary>
    /// A width x height RGBA8 image of the test pattern, alpha included.
    /// </summary>
    [[nodiscard]] core::image pattern_image(u32 width, u32 height) {
        core::image image(width, height);
        for (u32 y = 0; y < height; ++y) {
            for (u32 x = 0; x < width; ++x) {
                const core::rgba8 colour = {
                  pattern_value(x, y, 0), pattern_value(x, y, 1), pattern_value(x, y, 2), pattern_value(x, y, 3)
                };
                image.set_pixel(x, y, colour);
            }
        }
        return image;
    }

    constexpr std::array<texture_colour_space, 2> BOTH_COLOUR_SPACES = {
      texture_colour_space::SRGB, texture_colour_space::LINEAR
    };

    /// <summary>
    /// A smooth gradient, the kind of content a lossy encode keeps close.
    /// </summary>
    [[nodiscard]] core::image gradient_image(u32 width, u32 height) {
        core::image image(width, height);
        for (u32 y = 0; y < height; ++y) {
            for (u32 x = 0; x < width; ++x) {
                const u8 red = static_cast<u8>(x * 255 / std::max<u32>(width - 1, 1));
                const u8 green = static_cast<u8>(y * 255 / std::max<u32>(height - 1, 1));
                const u8 alpha = x < width / 2 ? 255 : 128;
                image.set_pixel(x, y, core::rgba8{red, green, 96, alpha});
            }
        }
        return image;
    }

    template <typename ValueType>
    [[nodiscard]] std::vector<std::byte> bytes_of_values(const std::vector<ValueType> &values) {
        std::vector<std::byte> bytes(values.size() * sizeof(ValueType));
        std::memcpy(bytes.data(), values.data(), bytes.size());
        return bytes;
    }

    /// <summary>
    /// IEEE half bits of a positive normal value that half represents exactly (the fixtures only use those).
    /// </summary>
    [[nodiscard]] u16 half_bits(f32 value) noexcept {
        if (value == 0.0f) {
            return 0;
        }
        int exponent = 0;
        const f32 fraction = std::frexp(value, &exponent);          // value = fraction * 2^exponent, [0.5, 1)
        const u32 biased = static_cast<u32>(exponent - 1 + 15);
        const u32 mantissa = static_cast<u32>(std::lround((fraction * 2.0f - 1.0f) * 1024.0f));
        return static_cast<u16>((biased << 10) | mantissa);
    }

    [[nodiscard]] codec::jpeg_xl_encode_desc rgba8_desc(const core::image &image) {
        codec::jpeg_xl_encode_desc desc = {};
        desc.width = image.width();
        desc.height = image.height();
        desc.channelCount = 4;
        desc.frames.push_back(image.pixels());
        return desc;
    }

    [[nodiscard]] std::vector<std::byte> encode_or_fail(
      const codec::jpeg_xl_encode_desc &desc,
      core::jobs::job_system *jobs = nullptr
    ) {
        core::result<std::vector<std::byte>> encoded = codec::encode_jpeg_xl(desc, jobs);
        CONJURE_REQUIRE_OK(encoded);
        return std::move(encoded).value();
    }

    [[nodiscard]] std::vector<std::byte> encode_or_fail(
      const core::image &image,
      const jpeg_xl_encode_options &options = {},
      core::jobs::job_system *jobs = nullptr
    ) {
        core::result<std::vector<std::byte>> encoded = encode_jpeg_xl(image, options, jobs);
        CONJURE_REQUIRE_OK(encoded);
        return std::move(encoded).value();
    }

    [[nodiscard]] texture_asset decode_or_fail(
      std::span<const std::byte> encoded,
      texture_colour_space colourSpace,
      core::jobs::job_system *jobs = nullptr
    ) {
        core::result<texture_asset> decoded = texture_asset::decode(encoded, colourSpace, false, jobs);
        CONJURE_REQUIRE_OK(decoded);
        return std::move(decoded).value();
    }

    [[nodiscard]] std::vector<std::byte> bytes_of(std::string_view text) {
        std::vector<std::byte> bytes = {};
        for (const char character : text) {
            bytes.push_back(static_cast<std::byte>(character));
        }
        return bytes;
    }

    /// <summary>
    /// Largest per-channel difference between two RGBA8 images of the same size, and the mean one.
    /// </summary>
    struct image_difference {
        u32 largest = 0;
        f64 mean = 0.0;
    };

    [[nodiscard]] image_difference difference(const core::image &expected, const texture_asset &actual) {
        image_difference result = {};
        u64 total = 0;
        for (u32 y = 0; y < expected.height(); ++y) {
            for (u32 x = 0; x < expected.width(); ++x) {
                const core::rgba8 wanted = expected.pixel(x, y);
                const core::rgba8 got = actual.pixel(x, y);
                const std::array<u32, 4> deltas = {
                  static_cast<u32>(std::abs(static_cast<int>(wanted.r) - static_cast<int>(got.r))),
                  static_cast<u32>(std::abs(static_cast<int>(wanted.g) - static_cast<int>(got.g))),
                  static_cast<u32>(std::abs(static_cast<int>(wanted.b) - static_cast<int>(got.b))),
                  static_cast<u32>(std::abs(static_cast<int>(wanted.a) - static_cast<int>(got.a)))
                };
                for (const u32 delta : deltas) {
                    result.largest = std::max(result.largest, delta);
                    total += delta;
                }
            }
        }
        const u64 samples = static_cast<u64>(expected.width()) * expected.height() * 4;
        result.mean = static_cast<f64>(total) / static_cast<f64>(samples);
        return result;
    }

    [[nodiscard]] bool same_pixels(const core::image &expected, const texture_asset &actual) {
        if (actual.width() != expected.width() || actual.height() != expected.height()) {
            return false;
        }
        return difference(expected, actual).largest == 0;
    }

    /*-----------------------------------------------------------------------*/
    // A 16-bit PNG, written by hand (core::image writes 8-bit only): stored deflate blocks, real CRCs.

    [[nodiscard]] u32 crc32_of(std::span<const std::byte> bytes) noexcept {
        u32 crc = 0xFFFFFFFFu;
        for (const std::byte value : bytes) {
            crc ^= static_cast<u32>(value);
            for (u32 bit = 0; bit < 8; ++bit) {
                const u32 mask = 0u - (crc & 1u);
                crc = (crc >> 1) ^ (0xEDB88320u & mask);
            }
        }
        return ~crc;
    }

    void append_big_endian(std::vector<std::byte> &bytes, u32 value, u32 byteCount) {
        for (u32 index = byteCount; index > 0; --index) {
            bytes.push_back(static_cast<std::byte>((value >> ((index - 1) * 8)) & 0xFFu));
        }
    }

    void append_chunk(std::vector<std::byte> &png, std::string_view type, std::span<const std::byte> data) {
        append_big_endian(png, static_cast<u32>(data.size()), 4);
        std::vector<std::byte> typed = bytes_of(type);
        typed.insert(typed.end(), data.begin(), data.end());
        png.insert(png.end(), typed.begin(), typed.end());
        append_big_endian(png, crc32_of(typed), 4);
    }

    /// <summary>
    /// An RGBA PNG of 16 bits per channel; 'samples' holds width * height * 4 values, rows top to bottom.
    /// </summary>
    [[nodiscard]] std::vector<std::byte> png16_of(u32 width, u32 height, std::span<const u16> samples) {
        std::vector<std::byte> png = {};
        for (const u8 value : std::array<u8, 8>{0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A}) {
            png.push_back(static_cast<std::byte>(value));
        }
        std::vector<std::byte> header = {};
        append_big_endian(header, width, 4);
        append_big_endian(header, height, 4);
        for (const u8 value : std::array<u8, 5>{16, 6, 0, 0, 0}) {  // 16 bits, RGBA, deflate, no filter, no interlace
            header.push_back(static_cast<std::byte>(value));
        }
        append_chunk(png, "IHDR", header);

        std::vector<std::byte> raw = {};
        for (u32 y = 0; y < height; ++y) {
            raw.push_back(std::byte{0});                                // filter: none
            for (u32 index = 0; index < width * 4; ++index) {
                append_big_endian(raw, samples[static_cast<usize>(y) * width * 4 + index], 2);
            }
        }
        std::vector<std::byte> zlib = {std::byte{0x78}, std::byte{0x01}};
        usize offset = 0;
        while (offset < raw.size()) {
            const usize length = std::min<usize>(raw.size() - offset, 65535);
            const bool last = offset + length == raw.size();
            zlib.push_back(last ? std::byte{1} : std::byte{0});
            zlib.push_back(static_cast<std::byte>(length & 0xFFu));
            zlib.push_back(static_cast<std::byte>(length >> 8));
            zlib.push_back(static_cast<std::byte>(~length & 0xFFu));
            zlib.push_back(static_cast<std::byte>((~length >> 8) & 0xFFu));
            const auto first = raw.begin() + static_cast<std::ptrdiff_t>(offset);
            zlib.insert(zlib.end(), first, first + static_cast<std::ptrdiff_t>(length));
            offset += length;
        }
        u32 adlerLow = 1;
        u32 adlerHigh = 0;
        for (const std::byte value : raw) {
            adlerLow = (adlerLow + static_cast<u32>(value)) % 65521u;
            adlerHigh = (adlerHigh + adlerLow) % 65521u;
        }
        append_big_endian(zlib, (adlerHigh << 16) | adlerLow, 4);
        append_chunk(png, "IDAT", zlib);
        append_chunk(png, "IEND", {});
        return png;
    }

    /*-----------------------------------------------------------------------*/
    // A bare JPEG XL codestream header (ISO/IEC 18181-1 SizeHeader and an all-default ImageMetadata), written
    // bit by bit, least significant bit first, so a file can claim any size without holding its pixels.

    class bit_writer {
        private:
            std::vector<std::byte> mBytes = {};
            u32 mBitCount = 0;

        public:
            void write(u32 value, u32 bitCount) {
                for (u32 bit = 0; bit < bitCount; ++bit) {
                    if (mBitCount % 8 == 0) {
                        mBytes.push_back(std::byte{0});
                    }
                    const u32 set = (value >> bit) & 1u;
                    mBytes.back() |= static_cast<std::byte>(set << (mBitCount % 8));
                    ++mBitCount;
                }
            }

            void pad_to_byte() {
                while (mBitCount % 8 != 0) {
                    write(0, 1);
                }
            }

            [[nodiscard]] std::vector<std::byte> finish() const {
                return mBytes;
            }
    };

    /// <summary>
    /// A SizeHeader dimension: U32(BitsOffset(9, 1), BitsOffset(13, 1), BitsOffset(18, 1), BitsOffset(30, 1)).
    /// </summary>
    void write_dimension(bit_writer &writer, u32 value) {
        const u32 stored = value - 1;
        if (stored < (1u << 9)) {
            writer.write(0, 2);
            writer.write(stored, 9);
            return;
        }
        if (stored < (1u << 13)) {
            writer.write(1, 2);
            writer.write(stored, 13);
            return;
        }
        if (stored < (1u << 18)) {
            writer.write(2, 2);
            writer.write(stored, 18);
            return;
        }
        writer.write(3, 2);
        writer.write(stored, 30);
    }

    void write_size_header(bit_writer &writer, u32 width, u32 height) {
        writer.write(0xFF, 8);                          // the codestream signature FF 0A
        writer.write(0x0A, 8);
        writer.write(0, 1);                             // SizeHeader.small: no
        write_dimension(writer, height);
        writer.write(0, 3);                             // ratio 0: the width follows
        write_dimension(writer, width);
    }

    [[nodiscard]] std::vector<std::byte> jpeg_xl_header_only(u32 width, u32 height) {
        bit_writer writer = {};
        write_size_header(writer, width, height);
        writer.write(1, 1);                             // ImageMetadata.all_default
        std::vector<std::byte> bytes = writer.finish();
        bytes.insert(bytes.end(), 64, std::byte{0});    // the start of a frame that never comes
        return bytes;
    }

    /// <summary>
    /// A header whose ImageMetadata declares 'extraChannels' (1..4096) default extra channels (8-bit alpha) on an
    /// 8-bit, XYB-encoded sRGB image, followed by zeros.
    /// </summary>
    [[nodiscard]] std::vector<std::byte> jpeg_xl_header_with_extra_channels(u32 width, u32 height, u32 extraChannels) {
        bit_writer writer = {};
        write_size_header(writer, width, height);
        writer.write(0, 1);                             // ImageMetadata.all_default: no
        writer.write(0, 1);                             // extra_fields: none
        writer.write(0, 1);                             // integer samples
        writer.write(0, 2);                             // of 8 bits
        writer.write(1, 1);                             // modular_16_bit_buffer_sufficient
        writer.write(3, 2);                             // num_extra_channels: BitsOffset(12, 1)
        writer.write(extraChannels - 1, 12);
        for (u32 channel = 0; channel < extraChannels; ++channel) {
            writer.write(1, 1);                         // ExtraChannelInfo.all_default
        }
        writer.write(1, 1);                             // xyb_encoded
        writer.write(1, 1);                             // ColorEncoding.all_default: sRGB
        writer.write(0, 2);                             // no extensions
        writer.write(1, 1);                             // CustomTransformData.all_default
        std::vector<std::byte> bytes = writer.finish();
        bytes.insert(bytes.end(), 64, std::byte{0});
        return bytes;
    }

    /// <summary>
    /// A header whose colour encoding is an ICC profile and whose ICC stream announces 2^28 - 1 encoded bytes, the
    /// most libjxl accepts, then ends.
    /// </summary>
    [[nodiscard]] std::vector<std::byte> jpeg_xl_header_with_icc_size(u32 width, u32 height) {
        bit_writer writer = {};
        write_size_header(writer, width, height);
        writer.write(0, 1);                             // ImageMetadata.all_default: no
        writer.write(0, 1);                             // extra_fields: none
        writer.write(0, 1);                             // integer samples
        writer.write(0, 2);                             // of 8 bits
        writer.write(1, 1);                             // modular_16_bit_buffer_sufficient
        writer.write(0, 2);                             // no extra channels
        writer.write(1, 1);                             // xyb_encoded
        writer.write(0, 1);                             // ColorEncoding.all_default: no
        writer.write(1, 1);                             // want_icc
        writer.write(0, 2);                             // colour space: RGB
        writer.write(0, 2);                             // no extensions
        writer.write(1, 1);                             // CustomTransformData.all_default
        writer.write(3, 2);                             // the ICC stream's size: U64 in 12 + 8 + 8 bits
        writer.write(0xFFF, 12);
        writer.write(1, 1);
        writer.write(0xFF, 8);
        writer.write(1, 1);
        writer.write(0xFF, 8);
        writer.write(0, 1);
        writer.pad_to_byte();
        return writer.finish();
    }

    /// <summary>
    /// A frame's origin or size: U32(Bits(8), BitsOffset(11, 256), BitsOffset(14, 2304), BitsOffset(30, 18688)).
    /// </summary>
    void write_frame_dimension(bit_writer &writer, u32 value) {
        if (value < 256) {
            writer.write(0, 2);
            writer.write(value, 8);
            return;
        }
        if (value < 2304) {
            writer.write(1, 2);
            writer.write(value - 256, 11);
            return;
        }
        if (value < 18688) {
            writer.write(2, 2);
            writer.write(value - 2304, 14);
            return;
        }
        writer.write(3, 2);
        writer.write(value - 18688, 30);
    }

    /// <summary>
    /// The kind of frame jpeg_xl_with_frame_size writes: a regular frame, which the decoder reports, or a
    /// reference-only one, which it decodes for later frames to refer to and never reports.
    /// </summary>
    enum class frame_kind : u8 {
        REGULAR = 0,
        REFERENCE_ONLY
    };

    /// <summary>
    /// A codestream whose header says width x height (all-default metadata) and whose only frame, modular with
    /// 1024 x 1024 groups, claims frameWidth x frameHeight, at least the image's size (a partial frame has fields
    /// this writer leaves out); a regular frame sits at the image's origin. Every section of its TOC is empty, so the
    /// file is a few hundred bytes whatever the frame claims.
    /// </summary>
    [[nodiscard]] std::vector<std::byte> jpeg_xl_with_frame_size(
      u32 width,
      u32 height,
      u32 frameWidth,
      u32 frameHeight,
      frame_kind kind = frame_kind::REGULAR
    ) {
        const bool regular = kind == frame_kind::REGULAR;
        bit_writer writer = {};
        write_size_header(writer, width, height);
        writer.write(1, 1);                             // ImageMetadata.all_default
        writer.write(1, 1);                             // CustomTransformData.all_default
        writer.pad_to_byte();
        writer.write(0, 1);                             // FrameHeader.all_default: no
        writer.write(regular ? 0 : 2, 2);               // a regular or a reference-only frame
        writer.write(1, 1);                             // modular
        writer.write(0, 2);                             // no flags
        writer.write(0, 2);                             // no upsampling
        writer.write(3, 2);                             // group_size_shift 3: 1024 x 1024 groups
        if (regular) {
            writer.write(0, 2);                         // one pass
        }
        writer.write(1, 1);                             // custom_size_or_origin
        if (regular) {
            write_frame_dimension(writer, 0);           // x0
            write_frame_dimension(writer, 0);           // y0
        }
        write_frame_dimension(writer, frameWidth);
        write_frame_dimension(writer, frameHeight);
        if (regular) {
            writer.write(0, 2);                         // blend mode: replace
            writer.write(1, 1);                         // is_last
        } else {
            writer.write(0, 2);                         // saved as reference 0
            writer.write(1, 1);                         // ... before the colour transform
        }
        writer.write(0, 2);                             // no name
        writer.write(1, 1);                             // LoopFilter.all_default
        writer.write(0, 2);                             // no extensions
        writer.write(0, 1);                             // the TOC is not permuted
        writer.pad_to_byte();
        constexpr u32 GROUP = 1024;
        constexpr u32 DC_GROUP = GROUP * 8;
        const u32 groups = ((frameWidth + GROUP - 1) / GROUP) * ((frameHeight + GROUP - 1) / GROUP);
        const u32 dcGroups = ((frameWidth + DC_GROUP - 1) / DC_GROUP) * ((frameHeight + DC_GROUP - 1) / DC_GROUP);
        const u32 entries = groups == 1 ? 1 : 1 + dcGroups + 1 + groups;
        for (u32 entry = 0; entry < entries; ++entry) {
            writer.write(0, 2);                         // Bits(10): an empty section
            writer.write(0, 10);
        }
        writer.pad_to_byte();
        return writer.finish();
    }

    /// <summary>
    /// The JPEG XL container of 'codestream' (signature box, ftyp, jxlc) followed by an Exif box whose 64-bit size
    /// claims 2^62 bytes while 16 follow.
    /// </summary>
    [[nodiscard]] std::vector<std::byte> container_with_huge_box(std::span<const std::byte> codestream) {
        std::vector<std::byte> bytes = {};
        append_big_endian(bytes, 12, 4);
        for (const char character : std::string_view("JXL ")) {
            bytes.push_back(static_cast<std::byte>(character));
        }
        append_big_endian(bytes, 0x0D0A870Au, 4);
        append_big_endian(bytes, 20, 4);
        for (const char character : std::string_view("ftypjxl ")) {
            bytes.push_back(static_cast<std::byte>(character));
        }
        append_big_endian(bytes, 0, 4);
        for (const char character : std::string_view("jxl ")) {
            bytes.push_back(static_cast<std::byte>(character));
        }
        append_big_endian(bytes, static_cast<u32>(8 + codestream.size()), 4);
        for (const char character : std::string_view("jxlc")) {
            bytes.push_back(static_cast<std::byte>(character));
        }
        bytes.insert(bytes.end(), codestream.begin(), codestream.end());
        append_big_endian(bytes, 1, 4);                 // the size follows as 64 bits
        for (const char character : std::string_view("Exif")) {
            bytes.push_back(static_cast<std::byte>(character));
        }
        append_big_endian(bytes, 0x40000000u, 4);
        append_big_endian(bytes, 0, 4);
        bytes.insert(bytes.end(), 16, std::byte{0});
        return bytes;
    }

    /// <summary>
    /// Appends the box 'type' around 'payload': its 32-bit size, the four type characters, the payload.
    /// </summary>
    void append_box(std::vector<std::byte> &bytes, std::string_view type, std::span<const std::byte> payload) {
        append_big_endian(bytes, static_cast<u32>(8 + payload.size()), 4);
        const std::vector<std::byte> typeBytes = bytes_of(type);
        bytes.insert(bytes.end(), typeBytes.begin(), typeBytes.end());
        bytes.insert(bytes.end(), payload.begin(), payload.end());
    }

    /// <summary>
    /// The JPEG XL container of 'codestream' as a converting tool may write it: the signature box, ftyp, an Exif box
    /// ahead of the image, and the codestream split across two jxlp boxes, each led by its 4-byte index (the high
    /// bit of the last one set).
    /// </summary>
    [[nodiscard]] std::vector<std::byte> container_with_split_codestream(std::span<const std::byte> codestream) {
        constexpr std::array<u8, 4> SIGNATURE = {0x0D, 0x0A, 0x87, 0x0A};
        constexpr std::array<u8, 12> FILE_TYPE = {'j', 'x', 'l', ' ', 0, 0, 0, 0, 'j', 'x', 'l', ' '};
        // The offset of the TIFF header (0), then a big-endian TIFF header whose one IFD has no entry.
        constexpr std::array<u8, 18> EXIF = {0, 0, 0, 0, 'M', 'M', 0, 42, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0};
        std::vector<std::byte> bytes = {};
        append_box(bytes, "JXL ", std::as_bytes(std::span(SIGNATURE)));
        append_box(bytes, "ftyp", std::as_bytes(std::span(FILE_TYPE)));
        append_box(bytes, "Exif", std::as_bytes(std::span(EXIF)));
        const usize half = codestream.size() / 2;
        const std::array<std::span<const std::byte>, 2> parts = {codestream.first(half), codestream.subspan(half)};
        for (u32 index = 0; index < parts.size(); ++index) {
            std::vector<std::byte> payload = {};
            const u32 last = index + 1 == parts.size() ? 0x80000000u : 0u;
            append_big_endian(payload, index | last, 4);
            payload.insert(payload.end(), parts[index].begin(), parts[index].end());
            append_box(bytes, "jxlp", payload);
        }
        return bytes;
    }

    /*-----------------------------------------------------------------------*/
    // HDR transfer functions of the expected values

    /// <summary>
    /// The PQ signal (SMPTE ST 2084) of a luminance in nits.
    /// </summary>
    [[nodiscard]] f64 pq_signal(f64 nits) noexcept {
        constexpr f64 M1 = 2610.0 / 16384.0;
        constexpr f64 M2 = 2523.0 / 4096.0 * 128.0;
        constexpr f64 C1 = 3424.0 / 4096.0;
        constexpr f64 C2 = 2413.0 / 4096.0 * 32.0;
        constexpr f64 C3 = 2392.0 / 4096.0 * 32.0;
        const f64 power = std::pow(nits / 10000.0, M1);
        return std::pow((C1 + C2 * power) / (1.0 + C3 * power), M2);
    }

    /// <summary>
    /// The HLG signal (ITU-R BT.2100) of a grey that a 1000-nit display (system gamma 1.2) shows at 'nits'.
    /// </summary>
    [[nodiscard]] f64 hlg_signal(f64 nits) noexcept {
        constexpr f64 A = 0.17883277;
        constexpr f64 B = 1.0 - 4.0 * A;
        const f64 c = 0.5 - A * std::log(4.0 * A);
        const f64 scene = std::pow(nits / 1000.0, 1.0 / 1.2);
        return scene <= 1.0 / 12.0 ? std::sqrt(3.0 * scene) : A * std::log(12.0 * scene - B) + c;
    }

    /// <summary>
    /// A grey file 'width' pixels wide of 'signals' (one per pixel, rows top to bottom, RGB equal) as 32-bit floats
    /// tagged 'encoding', lossless or lossy (Butteraugli distance 1).
    /// </summary>
    [[nodiscard]] std::vector<std::byte> grey_float_file(
      u32 width,
      std::span<const f64> signals,
      codec::jpeg_xl_colour_encoding encoding,
      f32 intensityTarget,
      bool lossless = true
    ) {
        std::vector<f32> samples = {};
        for (const f64 signal : signals) {
            samples.insert(samples.end(), 3, static_cast<f32>(signal));
        }
        const std::vector<std::byte> sampleBytes = bytes_of_values(samples);
        codec::jpeg_xl_encode_desc desc = {};
        desc.width = width;
        desc.height = static_cast<u32>(signals.size()) / width;
        desc.channelCount = 3;
        desc.sampleType = codec::jpeg_xl_sample_type::FLOAT32;
        desc.colourEncoding = encoding;
        desc.intensityTarget = intensityTarget;
        desc.lossless = lossless;
        desc.frames.push_back(sampleBytes);
        return encode_or_fail(desc);
    }

    /// <summary>
    /// The signals of grey patches 'patchSize' pixels square side by side, one row of patches, for grey_float_file:
    /// a lossy encode keeps the middle of a flat patch close to its value.
    /// </summary>
    [[nodiscard]] std::vector<f64> grey_patches(std::span<const f64> patchSignals, u32 patchSize) {
        std::vector<f64> signals = {};
        for (u32 y = 0; y < patchSize; ++y) {
            for (const f64 signal : patchSignals) {
                signals.insert(signals.end(), patchSize, signal);
            }
        }
        return signals;
    }

    /*-----------------------------------------------------------------------*/
    // Colour science of the expected values (CIE 1931 xy chromaticities, D65 white for both spaces)

    using matrix3 = std::array<std::array<f64, 3>, 3>;

    [[nodiscard]] matrix3 multiply(const matrix3 &left, const matrix3 &right) noexcept {
        matrix3 product = {};
        for (usize row = 0; row < 3; ++row) {
            for (usize column = 0; column < 3; ++column) {
                for (usize inner = 0; inner < 3; ++inner) {
                    product[row][column] += left[row][inner] * right[inner][column];
                }
            }
        }
        return product;
    }

    [[nodiscard]] matrix3 inverse(const matrix3 &m) noexcept {
        const f64 determinant = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
          m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
        matrix3 result = {};
        result[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) / determinant;
        result[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) / determinant;
        result[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) / determinant;
        result[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) / determinant;
        result[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) / determinant;
        result[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) / determinant;
        result[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) / determinant;
        result[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) / determinant;
        result[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) / determinant;
        return result;
    }

    /// <summary>
    /// Linear RGB to XYZ for primaries given as {red, green, blue} xy and a D65 white.
    /// </summary>
    [[nodiscard]] matrix3 rgb_to_xyz(const std::array<std::array<f64, 2>, 3> &primaries) noexcept {
        constexpr std::array<f64, 2> WHITE = {0.3127, 0.3290};
        matrix3 columns = {};
        for (usize primary = 0; primary < 3; ++primary) {
            const f64 x = primaries[primary][0];
            const f64 y = primaries[primary][1];
            columns[0][primary] = x / y;
            columns[1][primary] = 1.0;
            columns[2][primary] = (1.0 - x - y) / y;
        }
        const std::array<f64, 3> white = {WHITE[0] / WHITE[1], 1.0, (1.0 - WHITE[0] - WHITE[1]) / WHITE[1]};
        const matrix3 inverted = inverse(columns);
        matrix3 scaled = columns;
        for (usize primary = 0; primary < 3; ++primary) {
            const f64 scale = inverted[primary][0] * white[0] + inverted[primary][1] * white[1] +
              inverted[primary][2] * white[2];
            for (usize row = 0; row < 3; ++row) {
                scaled[row][primary] *= scale;
            }
        }
        return scaled;
    }

    [[nodiscard]] f64 srgb_decode(f64 encoded) noexcept {
        return encoded <= 0.04045 ? encoded / 12.92 : std::pow((encoded + 0.055) / 1.055, 2.4);
    }

    [[nodiscard]] f64 srgb_encode(f64 linear) noexcept {
        return linear <= 0.0031308 ? linear * 12.92 : 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
    }

    /// <summary>
    /// The sRGB bytes of a Display P3 colour given as encoded values in [0, 1] (same transfer function, same white,
    /// other primaries).
    /// </summary>
    [[nodiscard]] std::array<int, 3> display_p3_to_srgb(const std::array<f64, 3> &encoded) noexcept {
        const matrix3 p3 = rgb_to_xyz({{{0.680, 0.320}, {0.265, 0.690}, {0.150, 0.060}}});
        const matrix3 srgb = rgb_to_xyz({{{0.640, 0.330}, {0.300, 0.600}, {0.150, 0.060}}});
        const matrix3 convert = multiply(inverse(srgb), p3);
        const std::array<f64, 3> linear = {srgb_decode(encoded[0]), srgb_decode(encoded[1]), srgb_decode(encoded[2])};
        std::array<int, 3> result = {};
        for (usize row = 0; row < 3; ++row) {
            const f64 value = convert[row][0] * linear[0] + convert[row][1] * linear[1] + convert[row][2] * linear[2];
            result[row] = static_cast<int>(std::lround(srgb_encode(std::clamp(value, 0.0, 1.0)) * 255.0));
        }
        return result;
    }

    /// <summary>
    /// The same for a Display P3 colour of bytes.
    /// </summary>
    [[nodiscard]] std::array<int, 3> display_p3_to_srgb(u8 red, u8 green, u8 blue) noexcept {
        return display_p3_to_srgb(std::array<f64, 3>{red / 255.0, green / 255.0, blue / 255.0});
    }

    [[nodiscard]] bool within(int expected, u8 actual, int tolerance) noexcept {
        return std::abs(expected - static_cast<int>(actual)) <= tolerance;
    }

    /*-----------------------------------------------------------------------*/
    // ICC v2 profiles as an editor embeds them: RGB (matrix/TRC, sRGB's D50-adapted colorants) or grey, with a
    // three-point tone curve (0, 0x1000, 0xFFFF, interpolated linearly) that no encoded JPEG XL profile can name,
    // so the file keeps the ICC profile itself.

    constexpr f64 ICC_CURVE_MIDDLE = 4096.0 / 65535.0;

    /// <summary>
    /// The linear light of an encoded value under the profile's tone curve.
    /// </summary>
    [[nodiscard]] f64 icc_curve(f64 encoded) noexcept {
        if (encoded < 0.5) {
            return encoded / 0.5 * ICC_CURVE_MIDDLE;
        }
        return ICC_CURVE_MIDDLE + (encoded - 0.5) / 0.5 * (1.0 - ICC_CURVE_MIDDLE);
    }

    void append_s15_fixed16(std::vector<std::byte> &bytes, f64 value) {
        append_big_endian(bytes, static_cast<u32>(static_cast<i32>(std::lround(value * 65536.0))), 4);
    }

    void append_signature(std::vector<std::byte> &bytes, std::string_view signature) {
        for (const char character : signature) {
            bytes.push_back(static_cast<std::byte>(character));
        }
    }

    [[nodiscard]] std::vector<std::byte> tabulated_curve_icc(bool grey) {
        constexpr std::array<std::array<f64, 3>, 4> XYZ_TAGS = {{
          {0.9642, 1.0, 0.8249},                            // wtpt (D50)
          {0.4360747, 0.2225045, 0.0139322},                // rXYZ
          {0.3850649, 0.7168786, 0.0971045},                // gXYZ
          {0.1430804, 0.0606169, 0.7141733}                 // bXYZ
        }};
        constexpr std::array<std::string_view, 4> XYZ_NAMES = {"wtpt", "rXYZ", "gXYZ", "bXYZ"};
        constexpr std::array<std::string_view, 3> TRC_NAMES = {"rTRC", "gTRC", "bTRC"};
        constexpr u32 HEADER_SIZE = 128;
        constexpr u32 XYZ_SIZE = 20;
        constexpr u32 CURVE_SIZE = 18;
        const u32 xyzCount = grey ? 1 : 4;
        const u32 tagCount = grey ? 2 : 7;
        const u32 dataStart = HEADER_SIZE + 4 + tagCount * 12;
        const u32 curveOffset = dataStart + xyzCount * XYZ_SIZE;
        const u32 profileSize = curveOffset + 20;           // the curve padded to four bytes

        std::vector<std::byte> icc = {};
        append_big_endian(icc, profileSize, 4);
        append_big_endian(icc, 0, 4);                       // preferred CMM
        append_big_endian(icc, 0x02100000u, 4);             // version 2.1
        append_signature(icc, grey ? "mntrGRAYXYZ " : "mntrRGB XYZ ");
        icc.insert(icc.end(), 12, std::byte{0});            // date
        append_signature(icc, "acsp");
        icc.insert(icc.end(), 24, std::byte{0});            // platform, flags, manufacturer, model, attributes
        append_big_endian(icc, 0, 4);                       // perceptual intent
        append_s15_fixed16(icc, 0.9642);                    // PCS illuminant D50
        append_s15_fixed16(icc, 1.0);
        append_s15_fixed16(icc, 0.8249);
        icc.insert(icc.end(), HEADER_SIZE - icc.size(), std::byte{0});

        append_big_endian(icc, tagCount, 4);
        for (u32 index = 0; index < xyzCount; ++index) {
            append_signature(icc, XYZ_NAMES[index]);
            append_big_endian(icc, dataStart + index * XYZ_SIZE, 4);
            append_big_endian(icc, XYZ_SIZE, 4);
        }
        if (grey) {
            append_signature(icc, "kTRC");
            append_big_endian(icc, curveOffset, 4);
            append_big_endian(icc, CURVE_SIZE, 4);
        } else {
            for (const std::string_view name : TRC_NAMES) {
                append_signature(icc, name);                // the three curves share one tag
                append_big_endian(icc, curveOffset, 4);
                append_big_endian(icc, CURVE_SIZE, 4);
            }
        }
        for (u32 index = 0; index < xyzCount; ++index) {
            append_signature(icc, "XYZ ");
            append_big_endian(icc, 0, 4);
            for (const f64 component : XYZ_TAGS[index]) {
                append_s15_fixed16(icc, component);
            }
        }
        append_signature(icc, "curv");
        append_big_endian(icc, 0, 4);
        append_big_endian(icc, 3, 4);
        for (const u32 entry : {0u, 0x1000u, 0xFFFFu}) {
            append_big_endian(icc, entry, 2);
        }
        append_big_endian(icc, 0, 2);
        return icc;
    }

    /*-----------------------------------------------------------------------*/
    // Orientation

    struct position {
        u32 x = 0;
        u32 y = 0;
    };

    /// <summary>
    /// Where pixel (x, y) of the displayed image comes from in the stored width x height image, for an EXIF
    /// orientation: 2 mirrors, 3 turns 180 degrees, 4 flips, 5 transposes, 6 turns 90 degrees clockwise, 7
    /// transposes across the other diagonal, 8 turns 90 degrees counter-clockwise.
    /// </summary>
    [[nodiscard]] position stored_position(u32 orientation, u32 x, u32 y, u32 width, u32 height) noexcept {
        switch (orientation) {
            case 2: {
                return position{width - 1 - x, y};
            }
            case 3: {
                return position{width - 1 - x, height - 1 - y};
            }
            case 4: {
                return position{x, height - 1 - y};
            }
            case 5: {
                return position{y, x};
            }
            case 6: {
                return position{y, height - 1 - x};
            }
            case 7: {
                return position{width - 1 - y, height - 1 - x};
            }
            case 8: {
                return position{width - 1 - y, x};
            }
            default: {
                return position{x, y};
            }
        }
    }

    /*-----------------------------------------------------------------------*/
    // Job systems of the determinism cases

    [[nodiscard]] core::jobs::job_system_desc workers(u32 count) noexcept {
        core::jobs::job_system_desc desc = {};
        desc.workerCount = count;
        return desc;
    }
}

TEST_SUITE("renderer/jpeg_xl") {

    TEST_CASE("is_jpeg_xl - recognises the codestream and container signatures and nothing else") {
        const std::vector<std::byte> encoded = encode_or_fail(pattern_image(3, 2));
        CHECK(is_jpeg_xl(encoded));
        CHECK(is_jpeg_xl(std::vector<std::byte>{std::byte{0xFF}, std::byte{0x0A}}));
        const std::array<u8, 12> containerSignature = {0, 0, 0, 0x0C, 'J', 'X', 'L', ' ', 0x0D, 0x0A, 0x87, 0x0A};
        std::vector<std::byte> container = {};
        for (const u8 value : containerSignature) {
            container.push_back(static_cast<std::byte>(value));
        }
        CHECK(is_jpeg_xl(container));

        core::image png(2, 2);
        core::result<std::vector<std::byte>> pngBytes = png.encode_png();
        CONJURE_REQUIRE_OK(pngBytes);
        CHECK_FALSE(is_jpeg_xl(pngBytes.value()));
        CHECK_FALSE(is_jpeg_xl({}));
        CHECK_FALSE(is_jpeg_xl(std::vector<std::byte>{std::byte{0xFF}}));               // too short to decide
        CHECK_FALSE(is_jpeg_xl(std::vector<std::byte>{std::byte{0xFF}, std::byte{0xD8}}));  // a JPEG
        CHECK_FALSE(is_jpeg_xl(std::span<const std::byte>(container).first(6)));
        CHECK_FALSE(is_jpeg_xl(bytes_of("not an image")));
    }

    TEST_CASE("encode_jpeg_xl - a lossless round trip is bit-exact, tagged sRGB or linear") {
        const core::image image = pattern_image(7, 5);
        const std::vector<std::byte> srgbFile = encode_or_fail(image);
        CHECK(same_pixels(image, decode_or_fail(srgbFile, texture_colour_space::SRGB)));
        CHECK(same_pixels(image, decode_or_fail(srgbFile, texture_colour_space::LINEAR)));

        jpeg_xl_encode_options linearOptions = {};
        linearOptions.colourSpace = texture_colour_space::LINEAR;
        const std::vector<std::byte> linearFile = encode_or_fail(image, linearOptions);
        CHECK(linearFile != srgbFile);
        CHECK(same_pixels(image, decode_or_fail(linearFile, texture_colour_space::LINEAR)));

        // Every effort is lossless.
        for (const u32 effort : {1u, 4u, 9u}) {
            CAPTURE(effort);
            jpeg_xl_encode_options options = {};
            options.effort = effort;
            CHECK(same_pixels(image, decode_or_fail(encode_or_fail(image, options), texture_colour_space::SRGB)));
        }
    }

    TEST_CASE("encode_jpeg_xl - a lossy round trip stays within tolerance and keeps alpha exact") {
        const core::image image = gradient_image(48, 32);
        jpeg_xl_encode_options options = {};
        options.lossless = false;
        options.distance = 1.0f;
        const std::vector<std::byte> lossy = encode_or_fail(image, options);
        const texture_asset decoded = decode_or_fail(lossy, texture_colour_space::SRGB);
        REQUIRE(decoded.width() == image.width());
        REQUIRE(decoded.height() == image.height());
        const image_difference error = difference(image, decoded);
        // Butteraugli is perceptual: dark red beside bright green moves furthest, the average stays small.
        CHECK(error.mean < 2.0);
        CHECK(error.largest <= 48);
        for (u32 y = 0; y < image.height(); ++y) {
            for (u32 x = 0; x < image.width(); ++x) {
                CAPTURE(x);
                CAPTURE(y);
                CHECK(decoded.pixel(x, y).a == image.pixel(x, y).a);
            }
        }

        // A larger distance trades quality for size.
        options.distance = 8.0f;
        const std::vector<std::byte> rough = encode_or_fail(image, options);
        CHECK(rough.size() < lossy.size());
        CHECK(difference(image, decode_or_fail(rough, texture_colour_space::SRGB)).mean > error.mean);
    }

    TEST_CASE("encode_jpeg_xl - an empty image, an effort outside 1..10 and a lossy distance outside 0..25 fail") {
        CONJURE_CHECK_ERROR(encode_jpeg_xl(core::image()), core::error_code::INVALID_ARGUMENT);
        const core::image image = pattern_image(2, 2);
        for (const u32 effort : {0u, 11u}) {
            CAPTURE(effort);
            jpeg_xl_encode_options options = {};
            options.effort = effort;
            CONJURE_CHECK_ERROR(encode_jpeg_xl(image, options), core::error_code::INVALID_ARGUMENT);
        }
        for (const f32 distance : {0.0f, -1.0f, 25.5f, std::numeric_limits<f32>::quiet_NaN()}) {
            CAPTURE(distance);
            jpeg_xl_encode_options options = {};
            options.lossless = false;
            options.distance = distance;
            CONJURE_CHECK_ERROR(encode_jpeg_xl(image, options), core::error_code::INVALID_ARGUMENT);
        }
        // The distance means nothing to a lossless encode; the bounds are accepted.
        jpeg_xl_encode_options lossless = {};
        lossless.distance = 0.0f;
        CONJURE_CHECK_OK(encode_jpeg_xl(image, lossless));
        lossless.lossless = false;
        lossless.distance = 25.0f;
        lossless.effort = 1;
        CONJURE_CHECK_OK(encode_jpeg_xl(image, lossless));
        lossless.effort = 10;
        CONJURE_CHECK_OK(encode_jpeg_xl(image, lossless));
        CHECK(jpeg_xl_encode_options() == jpeg_xl_encode_options());
        CHECK_FALSE(lossless == jpeg_xl_encode_options());
    }

    TEST_CASE("encode_jpeg_xl - the file is the same bytes with no, one or many job system workers [parallel]") {
        const core::image image = gradient_image(320, 288);
        jpeg_xl_encode_options lossy = {};
        lossy.lossless = false;
        lossy.effort = 3;
        jpeg_xl_encode_options lossless = {};
        lossless.effort = 2;

        core::jobs::job_system single = {};
        core::jobs::job_system many = {};
        CONJURE_REQUIRE_OK(single.start(workers(1)));
        CONJURE_REQUIRE_OK(many.start(workers(8)));
        for (const jpeg_xl_encode_options &options : {lossy, lossless}) {
            CAPTURE(options.lossless);
            const std::vector<std::byte> reference = encode_or_fail(image, options);
            CHECK(encode_or_fail(image, options) == reference);
            CHECK(encode_or_fail(image, options, &single) == reference);
            CHECK(encode_or_fail(image, options, &many) == reference);
        }
        CHECK(many.stats().scheduled > 0);
        many.stop();
        single.stop();
    }

    TEST_CASE("jpeg_xl_encode_desc - inconsistent descriptions fail and libjxl's refusals are BACKEND_FAILURE") {
        const core::image image = pattern_image(2, 2);
        const codec::jpeg_xl_encode_desc valid = rgba8_desc(image);
        CONJURE_REQUIRE_OK(codec::encode_jpeg_xl(valid, nullptr));

        codec::jpeg_xl_encode_desc desc = valid;
        desc.width = 0;
        CONJURE_CHECK_ERROR(codec::encode_jpeg_xl(desc, nullptr), core::error_code::INVALID_ARGUMENT);
        for (const u32 channels : {0u, 5u}) {
            CAPTURE(channels);
            desc = valid;
            desc.channelCount = channels;
            CONJURE_CHECK_ERROR(codec::encode_jpeg_xl(desc, nullptr), core::error_code::INVALID_ARGUMENT);
        }
        for (const u32 orientation : {0u, 9u}) {
            CAPTURE(orientation);
            desc = valid;
            desc.orientation = orientation;
            CONJURE_CHECK_ERROR(codec::encode_jpeg_xl(desc, nullptr), core::error_code::INVALID_ARGUMENT);
        }
        desc = valid;
        desc.frames.clear();
        CONJURE_CHECK_ERROR(codec::encode_jpeg_xl(desc, nullptr), core::error_code::INVALID_ARGUMENT);
        desc = valid;
        desc.frames[0] = desc.frames[0].first(desc.frames[0].size() - 1);
        CONJURE_CHECK_ERROR(codec::encode_jpeg_xl(desc, nullptr), core::error_code::INVALID_ARGUMENT);
        desc = valid;
        desc.sampleType = codec::jpeg_xl_sample_type::UINT16;                // the bytes are only enough for 8 bits
        CONJURE_CHECK_ERROR(codec::encode_jpeg_xl(desc, nullptr), core::error_code::INVALID_ARGUMENT);
        desc = valid;
        desc.frames.push_back(desc.frames[0].first(4));                       // the second frame is short
        CONJURE_CHECK_ERROR(codec::encode_jpeg_xl(desc, nullptr), core::error_code::INVALID_ARGUMENT);
        desc = valid;
        desc.frames.push_back(desc.frames[0]);
        desc.frameDurationMilliseconds = 0;                                    // layers, not an animation
        CONJURE_CHECK_ERROR(codec::encode_jpeg_xl(desc, nullptr), core::error_code::INVALID_ARGUMENT);
        const std::vector<std::byte> grey(4);
        desc = valid;
        desc.channelCount = 1;
        desc.frames = {grey};
        desc.premultipliedAlpha = true;
        CONJURE_CHECK_ERROR(codec::encode_jpeg_xl(desc, nullptr), core::error_code::INVALID_ARGUMENT);
        for (const f32 intensityTarget : {-1.0f, std::numeric_limits<f32>::quiet_NaN()}) {
            CAPTURE(intensityTarget);
            desc = valid;
            desc.intensityTarget = intensityTarget;
            CONJURE_CHECK_ERROR(codec::encode_jpeg_xl(desc, nullptr), core::error_code::INVALID_ARGUMENT);
        }
        desc = valid;
        desc.colourBits = 9;                                                   // more than the 8-bit samples hold
        CONJURE_CHECK_ERROR(codec::encode_jpeg_xl(desc, nullptr), core::error_code::INVALID_ARGUMENT);
        const std::vector<f32> floats(16, 0.5f);
        const std::vector<std::byte> floatBytes = bytes_of_values(floats);
        desc = valid;
        desc.sampleType = codec::jpeg_xl_sample_type::FLOAT32;
        desc.frames = {floatBytes};
        desc.colourBits = 8;                                                   // floats store their own bits
        CONJURE_CHECK_ERROR(codec::encode_jpeg_xl(desc, nullptr), core::error_code::INVALID_ARGUMENT);
        desc = valid;
        desc.layerWidth = 2;                                                   // a layer needs both sides
        CONJURE_CHECK_ERROR(codec::encode_jpeg_xl(desc, nullptr), core::error_code::INVALID_ARGUMENT);
        desc = valid;
        desc.layerWidth = 3;
        desc.layerHeight = 3;                                                  // the frame holds 2 x 2 pixels
        CONJURE_CHECK_ERROR(codec::encode_jpeg_xl(desc, nullptr), core::error_code::INVALID_ARGUMENT);

        // Values libjxl checks itself reach it and come back as its refusal.
        for (const u32 effort : {0u, 11u}) {
            CAPTURE(effort);
            desc = valid;
            desc.effort = effort;
            const core::result<std::vector<std::byte>> refused = codec::encode_jpeg_xl(desc, nullptr);
            CONJURE_CHECK_ERROR(refused, core::error_code::BACKEND_FAILURE);
            if (!refused.has_value()) {
                CHECK(refused.get_error().message().find("effort") != std::string::npos);
            }
        }
        desc = valid;
        desc.lossless = false;
        desc.distance = 30.0f;
        CONJURE_CHECK_ERROR(codec::encode_jpeg_xl(desc, nullptr), core::error_code::BACKEND_FAILURE);
        const std::vector<std::byte> notIcc = bytes_of("certainly not an ICC profile");
        desc = valid;
        desc.iccProfile = notIcc;
        CONJURE_CHECK_ERROR(codec::encode_jpeg_xl(desc, nullptr), core::error_code::BACKEND_FAILURE);
    }

    TEST_CASE("texture_asset - decodes every JPEG XL pixel layout and sample type to RGBA8") {
        constexpr u32 WIDTH = 5;
        constexpr u32 HEIGHT = 3;
        const std::array<codec::jpeg_xl_sample_type, 4> sampleTypes = {
          codec::jpeg_xl_sample_type::UINT8,
          codec::jpeg_xl_sample_type::UINT16,
          codec::jpeg_xl_sample_type::FLOAT16,
          codec::jpeg_xl_sample_type::FLOAT32
        };
        for (u32 channels = 1; channels <= 4; ++channels) {
            for (const codec::jpeg_xl_sample_type sampleType : sampleTypes) {
                CAPTURE(channels);
                CAPTURE(static_cast<u32>(sampleType));
                // Every sample's high byte is the pattern: 16-bit samples carry low-byte noise that must go, and
                // float samples (2m + 1) / 512 sit half-way between two 16-bit high bytes, exact in half floats.
                std::vector<u8> bytes8 = {};
                std::vector<u16> bytes16 = {};
                std::vector<u16> halves = {};
                std::vector<f32> floats = {};
                for (u32 y = 0; y < HEIGHT; ++y) {
                    for (u32 x = 0; x < WIDTH; ++x) {
                        for (u32 channel = 0; channel < channels; ++channel) {
                            const u8 high = pattern_value(x, y, channel);
                            const f32 middle = static_cast<f32>(2 * high + 1) / 512.0f;
                            bytes8.push_back(high);
                            bytes16.push_back(static_cast<u16>((high << 8) | ((x * 13 + channel * 7) & 0xFFu)));
                            halves.push_back(half_bits(middle));
                            floats.push_back(middle);
                        }
                    }
                }
                std::vector<std::byte> samples = {};
                if (sampleType == codec::jpeg_xl_sample_type::UINT8) {
                    samples = bytes_of_values(bytes8);
                } else if (sampleType == codec::jpeg_xl_sample_type::UINT16) {
                    samples = bytes_of_values(bytes16);
                } else if (sampleType == codec::jpeg_xl_sample_type::FLOAT16) {
                    samples = bytes_of_values(halves);
                } else {
                    samples = bytes_of_values(floats);
                }
                codec::jpeg_xl_encode_desc desc = {};
                desc.width = WIDTH;
                desc.height = HEIGHT;
                desc.channelCount = channels;
                desc.sampleType = sampleType;
                desc.frames.push_back(samples);
                const std::vector<std::byte> encoded = encode_or_fail(desc);

                for (const texture_colour_space colourSpace : BOTH_COLOUR_SPACES) {
                    CAPTURE(std::string(to_string(colourSpace)));
                    const texture_asset decoded = decode_or_fail(encoded, colourSpace);
                    REQUIRE(decoded.width() == WIDTH);
                    REQUIRE(decoded.height() == HEIGHT);
                    for (u32 y = 0; y < HEIGHT; ++y) {
                        for (u32 x = 0; x < WIDTH; ++x) {
                            CAPTURE(x);
                            CAPTURE(y);
                            const bool grey = channels < 3;
                            const bool hasAlpha = channels == 2 || channels == 4;
                            const u8 red = pattern_value(x, y, 0);
                            const u8 green = grey ? red : pattern_value(x, y, 1);
                            const u8 blue = grey ? red : pattern_value(x, y, 2);
                            const u8 alpha = hasAlpha ? pattern_value(x, y, channels - 1) : 255;
                            CHECK(decoded.pixel(x, y) == core::rgba8{red, green, blue, alpha});
                        }
                    }
                }
            }
        }
    }

    TEST_CASE("texture_asset - a 16-bit JPEG XL reaches RGBA8 exactly as the same 16-bit PNG does") {
        constexpr u32 WIDTH = 6;
        constexpr u32 HEIGHT = 4;
        std::vector<u16> samples = {};
        for (u32 y = 0; y < HEIGHT; ++y) {
            for (u32 x = 0; x < WIDTH; ++x) {
                for (u32 channel = 0; channel < 4; ++channel) {
                    // The whole range, low bytes on both sides of 0x80 (no rounding may creep in).
                    const u32 value = (x * 9973u + y * 4099u + channel * 16411u + 255u) % 65536u;
                    samples.push_back(static_cast<u16>(value));
                }
            }
        }
        const std::vector<std::byte> png = png16_of(WIDTH, HEIGHT, samples);
        const std::vector<std::byte> sampleBytes = bytes_of_values(samples);
        codec::jpeg_xl_encode_desc desc = {};
        desc.width = WIDTH;
        desc.height = HEIGHT;
        desc.sampleType = codec::jpeg_xl_sample_type::UINT16;
        desc.frames.push_back(sampleBytes);
        const std::vector<std::byte> jxl = encode_or_fail(desc);
        // libjxl writes a lossless file deeper than 12 bits at codestream level 10, which needs the container: this
        // case decodes the signature box, ftyp, jxll and jxlc too.
        constexpr std::array<u8, 12> CONTAINER_SIGNATURE = {0, 0, 0, 0x0C, 'J', 'X', 'L', ' ', 0x0D, 0x0A, 0x87, 0x0A};
        REQUIRE(jxl.size() > CONTAINER_SIGNATURE.size());
        CHECK(std::ranges::equal(std::span(jxl).first(CONTAINER_SIGNATURE.size()),
                                 std::as_bytes(std::span(CONTAINER_SIGNATURE))));
        for (const texture_colour_space colourSpace : BOTH_COLOUR_SPACES) {
            CAPTURE(std::string(to_string(colourSpace)));
            const texture_asset fromPng = decode_or_fail(png, colourSpace);
            const texture_asset fromJxl = decode_or_fail(jxl, colourSpace);
            REQUIRE(fromPng.mip_count() == 1);
            CHECK(fromJxl.mips()[0].pixels == fromPng.mips()[0].pixels);
            CHECK(fromJxl.pixel(1, 2).r == static_cast<u8>(samples[(2 * WIDTH + 1) * 4] >> 8));
        }
    }

    TEST_CASE("texture_asset - a Display P3 or linear JPEG XL is converted to sRGB and kept as stored for LINEAR") {
        // Display P3 (204, 102, 51) lies inside the sRGB gamut; its sRGB bytes differ clearly from the P3 ones.
        const core::rgba8 p3Colour = {204, 102, 51, 200};
        core::image p3Image(3, 2);
        p3Image.fill(p3Colour);
        codec::jpeg_xl_encode_desc p3 = rgba8_desc(p3Image);
        p3.colourEncoding = codec::jpeg_xl_colour_encoding::DISPLAY_P3;
        const std::vector<std::byte> p3File = encode_or_fail(p3);
        const std::array<int, 3> expected = display_p3_to_srgb(p3Colour.r, p3Colour.g, p3Colour.b);
        const core::rgba8 converted = decode_or_fail(p3File, texture_colour_space::SRGB).pixel(2, 1);
        CAPTURE(expected[0]);
        CAPTURE(expected[1]);
        CAPTURE(expected[2]);
        CHECK(within(expected[0], converted.r, 2));
        CHECK(within(expected[1], converted.g, 2));
        CHECK(within(expected[2], converted.b, 2));
        CHECK(converted.a == p3Colour.a);
        CHECK(std::abs(expected[0] - static_cast<int>(p3Colour.r)) > 8);           // the conversion is visible
        CHECK(decode_or_fail(p3File, texture_colour_space::LINEAR).pixel(2, 1) == p3Colour);

        // Linear light 128 / 255 is sRGB 188; LINEAR keeps the stored number. Grey goes the same way.
        for (const u32 channels : {4u, 1u}) {
            CAPTURE(channels);
            const std::vector<u8> stored(static_cast<usize>(2) * 2 * channels, 128);
            const std::vector<std::byte> storedBytes = bytes_of_values(stored);
            codec::jpeg_xl_encode_desc linear = {};
            linear.width = 2;
            linear.height = 2;
            linear.channelCount = channels;
            linear.colourEncoding = codec::jpeg_xl_colour_encoding::LINEAR_SRGB;
            linear.frames.push_back(storedBytes);
            const std::vector<std::byte> linearFile = encode_or_fail(linear);
            const int srgb128 = static_cast<int>(std::lround(srgb_encode(128.0 / 255.0) * 255.0));
            const core::rgba8 asSrgb = decode_or_fail(linearFile, texture_colour_space::SRGB).pixel(1, 1);
            CHECK(within(srgb128, asSrgb.r, 1));
            CHECK(within(srgb128, asSrgb.g, 1));
            CHECK(within(srgb128, asSrgb.b, 1));
            const u8 alpha = channels == 4 ? 128 : 255;
            CHECK(asSrgb.a == alpha);
            const core::rgba8 asStored = decode_or_fail(linearFile, texture_colour_space::LINEAR).pixel(1, 1);
            CHECK(asStored == core::rgba8{128, 128, 128, alpha});
        }

        // A lossy (XYB) file decoded for LINEAR comes back in its own encoding, not in linear light.
        codec::jpeg_xl_encode_desc lossyP3 = p3;
        lossyP3.lossless = false;
        const std::vector<std::byte> lossyFile = encode_or_fail(lossyP3);
        const core::rgba8 lossyStored = decode_or_fail(lossyFile, texture_colour_space::LINEAR).pixel(1, 1);
        CHECK(within(p3Colour.r, lossyStored.r, 4));
        CHECK(within(p3Colour.g, lossyStored.g, 4));
        CHECK(within(p3Colour.b, lossyStored.b, 4));
        const core::rgba8 lossyConverted = decode_or_fail(lossyFile, texture_colour_space::SRGB).pixel(1, 1);
        CHECK(within(expected[0], lossyConverted.r, 4));
        CHECK(within(expected[1], lossyConverted.g, 4));
        CHECK(within(expected[2], lossyConverted.b, 4));
    }

    TEST_CASE("texture_asset - a JPEG XL with an ICC profile is converted through the colour management system") {
        const int expected = static_cast<int>(std::lround(srgb_encode(icc_curve(128.0 / 255.0)) * 255.0));
        CAPTURE(expected);
        REQUIRE(std::abs(expected - 128) > 16);                         // the conversion is visible

        for (const bool grey : {false, true}) {
            for (const bool lossless : {true, false}) {
                for (const bool deep : {false, true}) {
                    CAPTURE(grey);
                    CAPTURE(lossless);
                    CAPTURE(deep);
                    const std::vector<std::byte> icc = tabulated_curve_icc(grey);
                    const u32 channels = grey ? 2 : 4;
                    // 128 / 255 in either depth (0x8080 / 65535 is the same value); the alpha passes untouched.
                    std::vector<u8> samples8 = {};
                    std::vector<u16> samples16 = {};
                    for (u32 pixel = 0; pixel < 16; ++pixel) {
                        samples8.insert(samples8.end(), channels - 1, 128);
                        samples8.push_back(static_cast<u8>(100 + pixel));
                        samples16.insert(samples16.end(), channels - 1, 0x8080);
                        samples16.push_back(static_cast<u16>((100 + pixel) * 257));
                    }
                    const std::vector<std::byte> sampleBytes =
                      deep ? bytes_of_values(samples16) : bytes_of_values(samples8);
                    codec::jpeg_xl_encode_desc desc = {};
                    desc.width = 4;
                    desc.height = 4;
                    desc.channelCount = channels;
                    desc.sampleType = deep ? codec::jpeg_xl_sample_type::UINT16 : codec::jpeg_xl_sample_type::UINT8;
                    desc.iccProfile = icc;
                    desc.lossless = lossless;
                    desc.frames.push_back(sampleBytes);
                    const std::vector<std::byte> file = encode_or_fail(desc);
                    const int tolerance = lossless ? 2 : 4;

                    const core::rgba8 converted = decode_or_fail(file, texture_colour_space::SRGB).pixel(2, 1);
                    CHECK(within(expected, converted.r, tolerance));
                    CHECK(within(expected, converted.g, tolerance));
                    CHECK(within(expected, converted.b, tolerance));
                    CHECK(converted.a == 106);
                    // LINEAR asks for the samples in the file's own profile: a lossy file decodes to linear sRGB
                    // and the colour management system converts it back to the ICC profile (else about 17).
                    const core::rgba8 asStored = decode_or_fail(file, texture_colour_space::LINEAR).pixel(2, 1);
                    CHECK(within(128, asStored.r, tolerance));
                    CHECK(within(128, asStored.g, tolerance));
                    CHECK(within(128, asStored.b, tolerance));
                    CHECK(asStored.a == 106);
                }
            }
        }
    }

    TEST_CASE("texture_asset - JPEG XL orientation is applied for all eight EXIF orientations") {
        constexpr u32 WIDTH = 3;
        constexpr u32 HEIGHT = 2;
        const core::image stored = pattern_image(WIDTH, HEIGHT);
        for (u32 orientation = 1; orientation <= 8; ++orientation) {
            CAPTURE(orientation);
            codec::jpeg_xl_encode_desc desc = rgba8_desc(stored);
            desc.orientation = orientation;
            const texture_asset decoded = decode_or_fail(encode_or_fail(desc), texture_colour_space::SRGB);
            const bool swapped = orientation >= 5;
            REQUIRE(decoded.width() == (swapped ? HEIGHT : WIDTH));
            REQUIRE(decoded.height() == (swapped ? WIDTH : HEIGHT));
            for (u32 y = 0; y < decoded.height(); ++y) {
                for (u32 x = 0; x < decoded.width(); ++x) {
                    CAPTURE(x);
                    CAPTURE(y);
                    const position source = stored_position(orientation, x, y, WIDTH, HEIGHT);
                    CHECK(decoded.pixel(x, y) == stored.pixel(source.x, source.y));
                }
            }
        }
    }

    TEST_CASE("texture_asset - an animated JPEG XL keeps its first frame and logs the frames it dropped") {
        const std::array<core::rgba8, 3> colours = {
          core::rgba8{200, 10, 10, 255}, core::rgba8{10, 200, 10, 255}, core::rgba8{10, 10, 200, 255}
        };
        std::vector<core::image> frames = {};
        codec::jpeg_xl_encode_desc desc = {};
        desc.width = 4;
        desc.height = 2;
        for (const core::rgba8 &colour : colours) {
            core::image frame(desc.width, desc.height);
            frame.fill(colour);
            frames.push_back(std::move(frame));
        }
        for (const core::image &frame : frames) {
            desc.frames.push_back(frame.pixels());
        }
        const std::vector<std::byte> animation = encode_or_fail(desc);

        log_capture capture = {};
        const texture_asset decoded = decode_or_fail(animation, texture_colour_space::SRGB);
        CHECK(decoded.pixel(3, 1) == colours[0]);
        CHECK(capture.contains(core::log_level::WARN, "dropped 2 later frame(s)"));

        log_capture hdrCapture = {};
        core::result<equirect_image> hdr = equirect_image::decode_hdr(animation);
        CONJURE_REQUIRE_OK(hdr);
        CHECK(hdr.value().rgb[0] == doctest::Approx(codec::srgb_to_linear(200.0f / 255.0f)).epsilon(0.002));
        CHECK(hdrCapture.contains(core::log_level::WARN, "dropped 2 later frame(s)"));

        // A still image logs nothing.
        log_capture stillCapture = {};
        static_cast<void>(decode_or_fail(encode_or_fail(frames[1]), texture_colour_space::SRGB));
        CHECK(stillCapture.count(core::log_level::WARN) == 0);
    }

    TEST_CASE("texture_asset - premultiplied JPEG XL alpha is unpremultiplied") {
        const std::array<core::rgba8, 4> straight = {
          core::rgba8{200, 100, 50, 255}, core::rgba8{200, 100, 50, 128}, core::rgba8{240, 60, 8, 64},
          core::rgba8{90, 180, 30, 200}
        };
        std::vector<u8> premultiplied = {};
        for (const core::rgba8 &colour : straight) {
            for (const u8 channel : {colour.r, colour.g, colour.b}) {
                premultiplied.push_back(static_cast<u8>(std::lround(channel * colour.a / 255.0)));
            }
            premultiplied.push_back(colour.a);
        }
        const std::vector<std::byte> samples = bytes_of_values(premultiplied);
        codec::jpeg_xl_encode_desc desc = {};
        desc.width = 2;
        desc.height = 2;
        desc.premultipliedAlpha = true;
        desc.frames.push_back(samples);
        const texture_asset decoded = decode_or_fail(encode_or_fail(desc), texture_colour_space::SRGB);
        for (u32 index = 0; index < straight.size(); ++index) {
            CAPTURE(index);
            const core::rgba8 got = decoded.pixel(index % 2, index / 2);
            const core::rgba8 &wanted = straight[index];
            CHECK(got.a == wanted.a);
            // Premultiplying to 8 bits loses up to 255 / alpha / 2 of the colour.
            const int tolerance = static_cast<int>(std::ceil(255.0 / wanted.a / 2.0)) + 1;
            CHECK(within(wanted.r, got.r, tolerance));
            CHECK(within(wanted.g, got.g, tolerance));
            CHECK(within(wanted.b, got.b, tolerance));
        }
        // Without the flag the same samples are straight colour already and stay as they are.
        desc.premultipliedAlpha = false;
        const texture_asset asStored = decode_or_fail(encode_or_fail(desc), texture_colour_space::SRGB);
        CHECK(asStored.pixel(1, 0).r == premultiplied[4]);
    }

    TEST_CASE("texture_asset - premultiplied JPEG XL alpha is divided out before the colour is converted") {
        // Straight colours premultiplied in the encoding the file stores. Converting first and dividing afterwards
        // turns a straight linear 26 / 128 at alpha 128 into sRGB 179 instead of 125.
        constexpr std::array<u8, 4> ALPHAS = {255, 200, 128, 64};
        constexpr std::array<std::array<u8, 3>, 4> STRAIGHT = {
          {{51, 120, 200}, {51, 120, 200}, {51, 120, 200}, {180, 90, 30}}
        };
        std::vector<u8> stored = {};
        for (usize index = 0; index < ALPHAS.size(); ++index) {
            for (const u8 channel : STRAIGHT[index]) {
                stored.push_back(static_cast<u8>(std::lround(channel * ALPHAS[index] / 255.0)));
            }
            stored.push_back(ALPHAS[index]);
        }
        const std::vector<std::byte> storedBytes = bytes_of_values(stored);
        // The straight value a file holds: the stored one divided by alpha, in the stored encoding.
        const auto straight = [&stored, &ALPHAS](usize index, usize channel) {
            return std::min(1.0, static_cast<f64>(stored[index * 4 + channel]) / ALPHAS[index]);
        };
        const auto channel_of = [](const core::rgba8 &colour, usize channel) {
            return channel == 0 ? colour.r : (channel == 1 ? colour.g : colour.b);
        };
        codec::jpeg_xl_encode_desc desc = {};
        desc.width = 2;
        desc.height = 2;
        desc.premultipliedAlpha = true;
        desc.frames.push_back(storedBytes);

        for (const codec::jpeg_xl_colour_encoding encoding : {
               codec::jpeg_xl_colour_encoding::LINEAR_SRGB, codec::jpeg_xl_colour_encoding::DISPLAY_P3
             }) {
            CAPTURE(static_cast<u32>(encoding));
            desc.colourEncoding = encoding;
            const texture_asset decoded = decode_or_fail(encode_or_fail(desc), texture_colour_space::SRGB);
            for (usize index = 0; index < ALPHAS.size(); ++index) {
                CAPTURE(index);
                const std::array<f64, 3> held = {straight(index, 0), straight(index, 1), straight(index, 2)};
                const core::rgba8 pixel = decoded.pixel(static_cast<u32>(index % 2), static_cast<u32>(index / 2));
                CHECK(pixel.a == ALPHAS[index]);
                const std::array<int, 3> p3Expected = display_p3_to_srgb(held);
                for (usize channel = 0; channel < 3; ++channel) {
                    CAPTURE(channel);
                    const bool linear = encoding == codec::jpeg_xl_colour_encoding::LINEAR_SRGB;
                    const int expected =
                      linear ? static_cast<int>(std::lround(srgb_encode(held[channel]) * 255.0)) : p3Expected[channel];
                    CHECK(within(expected, channel_of(pixel, channel), 2));
                }
            }
        }

        // A lossy (XYB) file is rendered back into the stored encoding before the division: a flat Display P3
        // patch of straight (51, 120, 200) at alpha 128.
        std::vector<u8> flat = {};
        for (u32 pixel = 0; pixel < 64; ++pixel) {
            flat.insert(flat.end(), {stored[8], stored[9], stored[10], stored[11]});
        }
        const std::vector<std::byte> flatBytes = bytes_of_values(flat);
        codec::jpeg_xl_encode_desc lossy = desc;
        lossy.width = 8;
        lossy.height = 8;
        lossy.lossless = false;
        lossy.colourEncoding = codec::jpeg_xl_colour_encoding::DISPLAY_P3;
        lossy.frames = {flatBytes};
        const core::rgba8 lossyPixel = decode_or_fail(encode_or_fail(lossy), texture_colour_space::SRGB).pixel(4, 4);
        const std::array<int, 3> lossyExpected =
          display_p3_to_srgb(std::array<f64, 3>{straight(2, 0), straight(2, 1), straight(2, 2)});
        CAPTURE(lossyExpected[0]);
        CAPTURE(lossyExpected[1]);
        CAPTURE(lossyExpected[2]);
        CHECK(within(lossyExpected[0], lossyPixel.r, 4));
        CHECK(within(lossyExpected[1], lossyPixel.g, 4));
        CHECK(within(lossyExpected[2], lossyPixel.b, 4));
        CHECK(lossyPixel.a == 128);

        // LINEAR keeps the stored encoding and divides there.
        const texture_asset asStored = decode_or_fail(encode_or_fail(desc), texture_colour_space::LINEAR);
        for (usize index = 0; index < ALPHAS.size(); ++index) {
            CAPTURE(index);
            for (usize channel = 0; channel < 3; ++channel) {
                const int expected = static_cast<int>(std::lround(straight(index, channel) * 255.0));
                const core::rgba8 pixel = asStored.pixel(static_cast<u32>(index % 2), static_cast<u32>(index / 2));
                CHECK(within(expected, channel_of(pixel, channel), 1));
            }
        }
    }

    TEST_CASE("equirect_image - decode_hdr divides premultiplied JPEG XL alpha out of the radiance") {
        // Straight linear radiance (0.2, 0.5, 4) at alpha 0.5 and (1, 2, 3) at alpha 1, stored premultiplied.
        const std::vector<f32> stored = {0.1f, 0.25f, 2.0f, 0.5f, 1.0f, 2.0f, 3.0f, 1.0f};
        const std::vector<std::byte> storedBytes = bytes_of_values(stored);
        codec::jpeg_xl_encode_desc desc = {};
        desc.width = 2;
        desc.height = 1;
        desc.sampleType = codec::jpeg_xl_sample_type::FLOAT32;
        desc.colourEncoding = codec::jpeg_xl_colour_encoding::LINEAR_SRGB;
        desc.premultipliedAlpha = true;
        desc.frames.push_back(storedBytes);
        core::result<equirect_image> decoded = equirect_image::decode_hdr(encode_or_fail(desc));
        CONJURE_REQUIRE_OK(decoded);
        const std::array<f32, 6> expected = {0.2f, 0.5f, 4.0f, 1.0f, 2.0f, 3.0f};
        REQUIRE(decoded.value().rgb.size() == expected.size());
        for (usize index = 0; index < expected.size(); ++index) {
            CAPTURE(index);
            CHECK(decoded.value().rgb[index] == doctest::Approx(expected[index]).epsilon(0.0001));
        }

        // An 8-bit sRGB file divides in sRGB: sRGB 188 at alpha 128 is stored as 94 and reaches linear 0.5.
        const std::vector<u8> srgbStored = {94, 94, 94, 128};
        const std::vector<std::byte> srgbBytes = bytes_of_values(srgbStored);
        codec::jpeg_xl_encode_desc srgb = {};
        srgb.width = 1;
        srgb.height = 1;
        srgb.premultipliedAlpha = true;
        srgb.frames.push_back(srgbBytes);
        core::result<equirect_image> fromSrgb = equirect_image::decode_hdr(encode_or_fail(srgb));
        CONJURE_REQUIRE_OK(fromSrgb);
        CHECK(fromSrgb.value().rgb[0] == doctest::Approx(srgb_decode(94.0 / 128.0)).epsilon(0.002));
    }

    TEST_CASE("texture_asset - a 16-bit JPEG XL alpha next to 8-bit colour keeps its high byte") {
        // The colour is stored in 8 bits, the alpha in 16: 0xC800 must give 200 (rounding gives 199 at some pixels,
        // libjxl dithers 8-bit output) and 0x00C8 must give 0.
        constexpr u32 WIDTH = 8;
        constexpr u32 HEIGHT = 4;
        std::vector<u16> samples = {};
        for (u32 y = 0; y < HEIGHT; ++y) {
            for (u32 x = 0; x < WIDTH; ++x) {
                for (u32 channel = 0; channel < 3; ++channel) {
                    samples.push_back(static_cast<u16>(pattern_value(x, y, channel) * 257u));
                }
                samples.push_back((x + y) % 2 == 0 ? u16{0xC800} : u16{0x00C8});
            }
        }
        const std::vector<std::byte> sampleBytes = bytes_of_values(samples);
        codec::jpeg_xl_encode_desc desc = {};
        desc.width = WIDTH;
        desc.height = HEIGHT;
        desc.sampleType = codec::jpeg_xl_sample_type::UINT16;
        desc.colourBits = 8;
        desc.frames.push_back(sampleBytes);
        const std::vector<std::byte> mixed = encode_or_fail(desc);
        desc.colourBits = 0;
        const std::vector<std::byte> deep = encode_or_fail(desc);

        for (const texture_colour_space colourSpace : BOTH_COLOUR_SPACES) {
            CAPTURE(std::string(to_string(colourSpace)));
            const texture_asset decoded = decode_or_fail(mixed, colourSpace);
            for (u32 y = 0; y < HEIGHT; ++y) {
                for (u32 x = 0; x < WIDTH; ++x) {
                    CAPTURE(x);
                    CAPTURE(y);
                    const u8 alpha = (x + y) % 2 == 0 ? 200 : 0;
                    const core::rgba8 expected = {
                      pattern_value(x, y, 0), pattern_value(x, y, 1), pattern_value(x, y, 2), alpha
                    };
                    CHECK(decoded.pixel(x, y) == expected);
                }
            }
            CHECK(decoded == decode_or_fail(deep, colourSpace));
        }
    }

    TEST_CASE("regression - premultiplied JPEG XL alpha that nothing converts afterwards is divided by libjxl") {
        // An sRGB file as an SRGB texture, and any file as a LINEAR one, need no conversion after the division:
        // libjxl divides and writes the bytes itself, so the decode allocates what the same samples marked straight
        // allocate, not a float copy of the frame (16 bytes a pixel) as well. The colour is the straight one either
        // way, and within a rounding step of the HDR path's, which converts after the engine's division.
        constexpr u32 SIZE = 128;
        std::vector<u8> stored = {};
        for (u32 y = 0; y < SIZE; ++y) {
            for (u32 x = 0; x < SIZE; ++x) {
                const u32 alpha = 64 + pattern_value(x, y, 3) % 192;
                for (u32 channel = 0; channel < 3; ++channel) {
                    stored.push_back(static_cast<u8>(std::lround(pattern_value(x, y, channel) * alpha / 255.0)));
                }
                stored.push_back(static_cast<u8>(alpha));
            }
        }
        const std::vector<std::byte> storedBytes = bytes_of_values(stored);
        codec::jpeg_xl_encode_desc desc = {};
        desc.width = SIZE;
        desc.height = SIZE;
        desc.effort = 2;
        desc.premultipliedAlpha = true;
        desc.frames.push_back(storedBytes);
        const std::vector<std::byte> premultiplied = encode_or_fail(desc);
        desc.premultipliedAlpha = false;
        const std::vector<std::byte> straight = encode_or_fail(desc);
        const auto allocated_by = [](std::span<const std::byte> file, texture_colour_space colourSpace) {
            const u64 before = core::memory::snapshot().allocatedBytes;
            const core::result<codec::decoded_image> decoded = codec::decode_jpeg_xl_rgba8(file, colourSpace, nullptr);
            const u64 allocated = core::memory::snapshot().allocatedBytes - before;
            CHECK(decoded.has_value());
            return allocated;
        };

        for (const texture_colour_space colourSpace : BOTH_COLOUR_SPACES) {
            CAPTURE(std::string(to_string(colourSpace)));
            CHECK(allocated_by(premultiplied, colourSpace) < allocated_by(straight, colourSpace) + SIZE * SIZE * 4);
            const texture_asset decoded = decode_or_fail(premultiplied, colourSpace);
            for (u32 y = 0; y < SIZE; y += 7) {
                for (u32 x = 0; x < SIZE; x += 5) {
                    CAPTURE(x);
                    CAPTURE(y);
                    const core::rgba8 got = decoded.pixel(x, y);
                    const u32 alpha = 64 + pattern_value(x, y, 3) % 192;
                    CHECK(got.a == alpha);
                    // Premultiplying to 8 bits loses up to 255 / alpha / 2 of the colour.
                    const int tolerance = static_cast<int>(std::ceil(255.0 / alpha / 2.0)) + 1;
                    CHECK(within(pattern_value(x, y, 0), got.r, tolerance));
                    CHECK(within(pattern_value(x, y, 1), got.g, tolerance));
                    CHECK(within(pattern_value(x, y, 2), got.b, tolerance));
                }
            }
        }

        // A lossy (XYB) file is rendered back into its sRGB before libjxl divides: a flat patch of straight
        // (51, 120, 200) at alpha 128.
        std::vector<u8> flat = {};
        for (u32 pixel = 0; pixel < 64; ++pixel) {
            flat.insert(flat.end(), {26, 60, 100, 128});
        }
        const std::vector<std::byte> flatBytes = bytes_of_values(flat);
        codec::jpeg_xl_encode_desc lossy = {};
        lossy.width = 8;
        lossy.height = 8;
        lossy.lossless = false;
        lossy.premultipliedAlpha = true;
        lossy.frames.push_back(flatBytes);
        const std::vector<std::byte> lossyFile = encode_or_fail(lossy);
        for (const texture_colour_space colourSpace : BOTH_COLOUR_SPACES) {
            CAPTURE(std::string(to_string(colourSpace)));
            const core::rgba8 got = decode_or_fail(lossyFile, colourSpace).pixel(4, 4);
            CHECK(within(51, got.r, 4));
            CHECK(within(120, got.g, 4));
            CHECK(within(200, got.b, 4));
            CHECK(got.a == 128);
        }

        core::result<equirect_image> hdr = equirect_image::decode_hdr(premultiplied);
        CONJURE_REQUIRE_OK(hdr);
        const texture_asset srgb = decode_or_fail(premultiplied, texture_colour_space::SRGB);
        u32 apart = 0;
        for (u32 y = 0; y < SIZE; ++y) {
            for (u32 x = 0; x < SIZE; ++x) {
                const core::rgba8 got = srgb.pixel(x, y);
                const std::array<u8, 3> bytes = {got.r, got.g, got.b};
                for (u32 channel = 0; channel < 3; ++channel) {
                    const f64 linear = hdr.value().rgb[(static_cast<usize>(y) * SIZE + x) * 3 + channel];
                    const int expected = static_cast<int>(std::lround(srgb_encode(std::min(linear, 1.0)) * 255.0));
                    apart += within(expected, bytes[channel], 1) ? 0 : 1;
                }
            }
        }
        CHECK(apart == 0);
    }

    TEST_CASE("texture_asset - libjxl divides a float premultiplied JPEG XL that nothing converts") {
        // Deep samples reach RGBA8 through the float scanline callback, and with nothing to convert libjxl divides
        // there too. Alphas that are powers of two make the stored products and libjxl's division exact, so every
        // byte is the high byte of the straight value's 16-bit rounding, alpha's included.
        constexpr std::array<f32, 4> ALPHAS = {0.5f, 0.25f, 1.0f, 0.125f};
        constexpr std::array<std::array<f32, 3>, 4> STRAIGHT = {
          {{0.2f, 0.5f, 0.8f}, {0.9f, 0.1f, 0.3f}, {0.6f, 0.4f, 0.05f}, {0.75f, 0.125f, 1.0f}}
        };
        std::vector<f32> stored = {};
        for (usize index = 0; index < ALPHAS.size(); ++index) {
            for (const f32 channel : STRAIGHT[index]) {
                stored.push_back(channel * ALPHAS[index]);
            }
            stored.push_back(ALPHAS[index]);
        }
        const std::vector<std::byte> storedBytes = bytes_of_values(stored);
        const auto high_byte = [](f32 value) {
            return static_cast<u8>(static_cast<u32>(std::lround(value * 65535.0f)) >> 8);
        };
        codec::jpeg_xl_encode_desc desc = {};
        desc.width = 2;
        desc.height = 2;
        desc.sampleType = codec::jpeg_xl_sample_type::FLOAT32;
        desc.premultipliedAlpha = true;
        desc.frames.push_back(storedBytes);
        const std::vector<std::byte> srgbFile = encode_or_fail(desc);
        desc.colourEncoding = codec::jpeg_xl_colour_encoding::LINEAR_SRGB;
        const std::vector<std::byte> linearFile = encode_or_fail(desc);

        // An sRGB file either way, and a linear one as LINEAR (as SRGB it is converted, which the engine does).
        const std::array<std::pair<const std::vector<std::byte> *, texture_colour_space>, 3> decodes = {{
          {&srgbFile, texture_colour_space::SRGB},
          {&srgbFile, texture_colour_space::LINEAR},
          {&linearFile, texture_colour_space::LINEAR}
        }};
        for (const auto &[file, colourSpace] : decodes) {
            CAPTURE(file == &srgbFile);
            CAPTURE(std::string(to_string(colourSpace)));
            const texture_asset decoded = decode_or_fail(*file, colourSpace);
            for (u32 index = 0; index < ALPHAS.size(); ++index) {
                CAPTURE(index);
                const core::rgba8 expected = {
                  high_byte(STRAIGHT[index][0]), high_byte(STRAIGHT[index][1]), high_byte(STRAIGHT[index][2]),
                  high_byte(ALPHAS[index])
                };
                CHECK(decoded.pixel(index % 2, index / 2) == expected);
            }
        }
    }

    TEST_CASE("texture_asset - a premultiplied grey JPEG XL with alpha is divided before its conversion to sRGB") {
        // Grey and alpha as floats tagged linear, decoded as SRGB: the engine divides, then converts through a grey
        // sRGB profile, whose ICC the header-only XYB decoder writes. The straight grey stored unpremultiplied,
        // which libjxl converts itself, gives the same bytes within a rounding step.
        constexpr std::array<f32, 4> GREYS = {0.05f, 0.2f, 0.5f, 0.9f};
        constexpr std::array<f32, 4> ALPHAS = {1.0f, 0.5f, 0.25f, 0.75f};
        std::vector<f32> premultipliedSamples = {};
        std::vector<f32> straightSamples = {};
        for (usize index = 0; index < GREYS.size(); ++index) {
            premultipliedSamples.insert(premultipliedSamples.end(), {GREYS[index] * ALPHAS[index], ALPHAS[index]});
            straightSamples.insert(straightSamples.end(), {GREYS[index], ALPHAS[index]});
        }
        const std::vector<std::byte> premultipliedBytes = bytes_of_values(premultipliedSamples);
        const std::vector<std::byte> straightBytes = bytes_of_values(straightSamples);
        codec::jpeg_xl_encode_desc desc = {};
        desc.width = static_cast<u32>(GREYS.size());
        desc.height = 1;
        desc.channelCount = 2;
        desc.sampleType = codec::jpeg_xl_sample_type::FLOAT32;
        desc.colourEncoding = codec::jpeg_xl_colour_encoding::LINEAR_SRGB;
        desc.premultipliedAlpha = true;
        desc.frames.push_back(premultipliedBytes);
        const texture_asset fromPremultiplied = decode_or_fail(encode_or_fail(desc), texture_colour_space::SRGB);
        desc.premultipliedAlpha = false;
        desc.frames = {straightBytes};
        const texture_asset fromStraight = decode_or_fail(encode_or_fail(desc), texture_colour_space::SRGB);

        for (u32 index = 0; index < GREYS.size(); ++index) {
            CAPTURE(index);
            const core::rgba8 got = fromPremultiplied.pixel(index, 0);
            const core::rgba8 reference = fromStraight.pixel(index, 0);
            // Float samples keep the high byte of 16 bits, colour and alpha alike.
            const u32 expected16 = static_cast<u32>(std::lround(srgb_encode(GREYS[index]) * 65535.0));
            const int expected = static_cast<int>(expected16 >> 8);
            const u8 alpha = static_cast<u8>(static_cast<u32>(std::lround(ALPHAS[index] * 65535.0)) >> 8);
            CHECK(within(expected, got.r, 1));
            CHECK(got.g == got.r);
            CHECK(got.b == got.r);
            CHECK(got.a == alpha);
            CHECK(within(reference.r, got.r, 1));
            CHECK(reference.a == alpha);
        }
    }

    TEST_CASE("regression - 8-bit JPEG XL colour converted beside a 16-bit alpha is rounded, not cut to a high byte") {
        // 8-bit colour beside a 16-bit alpha, which libjxl converts (Display P3 to sRGB, and a lossy file from XYB).
        // Keeping the high byte of its 16-bit result rounds v * 255 at v instead of at one half: dark values come
        // out up to a step low and bright ones up to a step high. Rounded, every byte is within half a step (and
        // libjxl's arithmetic) of the same colour taken to linear floats by the HDR path and encoded here.
        constexpr u32 SIZE = 32;
        constexpr f64 HALF_STEP = 0.55;
        std::vector<u16> samples = {};
        for (u32 y = 0; y < SIZE; ++y) {
            for (u32 x = 0; x < SIZE; ++x) {
                for (u32 channel = 0; channel < 3; ++channel) {
                    samples.push_back(static_cast<u16>(pattern_value(x, y, channel) * 257u));
                }
                samples.push_back(static_cast<u16>(0xFF00u + (x + y) % 256u));
            }
        }
        const std::vector<std::byte> sampleBytes = bytes_of_values(samples);
        codec::jpeg_xl_encode_desc desc = {};
        desc.width = SIZE;
        desc.height = SIZE;
        desc.sampleType = codec::jpeg_xl_sample_type::UINT16;
        desc.colourBits = 8;
        desc.colourEncoding = codec::jpeg_xl_colour_encoding::DISPLAY_P3;
        desc.frames.push_back(sampleBytes);
        const std::vector<std::byte> p3File = encode_or_fail(desc);
        desc.colourEncoding = codec::jpeg_xl_colour_encoding::SRGB;
        desc.lossless = false;
        const std::vector<std::byte> lossyFile = encode_or_fail(desc);

        for (const std::vector<std::byte> *file : {&p3File, &lossyFile}) {
            CAPTURE(file == &p3File);
            const texture_asset decoded = decode_or_fail(*file, texture_colour_space::SRGB);
            core::result<equirect_image> linear = equirect_image::decode_hdr(*file);
            CONJURE_REQUIRE_OK(linear);
            f64 largest = 0.0;
            for (u32 y = 0; y < SIZE; ++y) {
                for (u32 x = 0; x < SIZE; ++x) {
                    const core::rgba8 got = decoded.pixel(x, y);
                    const std::array<u8, 3> bytes = {got.r, got.g, got.b};
                    for (u32 channel = 0; channel < 3; ++channel) {
                        const f64 value = linear.value().rgb[(static_cast<usize>(y) * SIZE + x) * 3 + channel];
                        const f64 exact = srgb_encode(std::clamp(value, 0.0, 1.0)) * 255.0;
                        largest = std::max(largest, std::abs(bytes[channel] - exact));
                    }
                    CHECK(got.a == 0xFF);
                }
            }
            CHECK(largest < HALF_STEP);
        }
    }

    TEST_CASE("equirect_image - decode_hdr reads PQ and HLG JPEG XL in units of reference white") {
        // Grey luminances in nits; BT.2408's reference white, 203 nits, decodes to 1.
        constexpr std::array<f64, 5> PQ_NITS = {0.0, 20.0, 203.0, 1000.0, 4000.0};
        std::vector<f64> pq = {};
        for (const f64 nits : PQ_NITS) {
            pq.push_back(pq_signal(nits));
        }
        // libjxl's PQ intensity target defaults to 10000 nits; a file may name another (its brightest pixel), which
        // moves libjxl's 1.0 but not what a signal means.
        for (const f32 intensityTarget : {0.0f, 4000.0f}) {
            CAPTURE(intensityTarget);
            const std::vector<std::byte> file = grey_float_file(
              static_cast<u32>(pq.size()),
              pq,
              codec::jpeg_xl_colour_encoding::REC2100_PQ,
              intensityTarget
            );
            core::result<equirect_image> decoded = equirect_image::decode_hdr(file);
            CONJURE_REQUIRE_OK(decoded);
            for (usize index = 0; index < PQ_NITS.size(); ++index) {
                CAPTURE(PQ_NITS[index]);
                for (usize channel = 0; channel < 3; ++channel) {
                    const f64 expected = PQ_NITS[index] / codec::JPEG_XL_REFERENCE_WHITE_NITS;
                    CHECK(decoded.value().rgb[index * 3 + channel] == doctest::Approx(expected).epsilon(0.01));
                }
            }
        }

        // HLG on its nominal 1000-nit display: 75 % signal is reference white.
        constexpr std::array<f64, 3> HLG_NITS = {20.0, 203.0, 1000.0};
        std::vector<f64> hlg = {};
        for (const f64 nits : HLG_NITS) {
            hlg.push_back(hlg_signal(nits));
        }
        CHECK(hlg[1] == doctest::Approx(0.75).epsilon(0.002));
        const std::vector<std::byte> hlgFile =
          grey_float_file(static_cast<u32>(hlg.size()), hlg, codec::jpeg_xl_colour_encoding::REC2100_HLG, 0.0f);
        core::result<equirect_image> hlgDecoded = equirect_image::decode_hdr(hlgFile);
        CONJURE_REQUIRE_OK(hlgDecoded);
        for (usize index = 0; index < HLG_NITS.size(); ++index) {
            CAPTURE(HLG_NITS[index]);
            const f64 expected = HLG_NITS[index] / codec::JPEG_XL_REFERENCE_WHITE_NITS;
            CHECK(hlgDecoded.value().rgb[index * 3] == doctest::Approx(expected).epsilon(0.01));
        }
    }

    TEST_CASE("equirect_image - decode_hdr divides a premultiplied PQ JPEG XL before taking it to reference white") {
        // Grey PQ signals premultiplied by alpha in the signal the file stores: the division comes first, then
        // libjxl's PQ curve for the file's intensity target, then the rescale, so each pixel is nits / 203.
        constexpr std::array<f64, 4> NITS = {20.0, 203.0, 1000.0, 400.0};
        constexpr std::array<f64, 4> ALPHAS = {1.0, 0.5, 0.25, 0.75};
        std::vector<f32> stored = {};
        for (usize index = 0; index < NITS.size(); ++index) {
            const f32 premultiplied = static_cast<f32>(pq_signal(NITS[index]) * ALPHAS[index]);
            stored.insert(stored.end(), {premultiplied, premultiplied, premultiplied, static_cast<f32>(ALPHAS[index])});
        }
        const std::vector<std::byte> storedBytes = bytes_of_values(stored);
        codec::jpeg_xl_encode_desc desc = {};
        desc.width = static_cast<u32>(NITS.size());
        desc.height = 1;
        desc.sampleType = codec::jpeg_xl_sample_type::FLOAT32;
        desc.colourEncoding = codec::jpeg_xl_colour_encoding::REC2100_PQ;
        desc.premultipliedAlpha = true;
        desc.frames.push_back(storedBytes);
        core::result<equirect_image> decoded = equirect_image::decode_hdr(encode_or_fail(desc));
        CONJURE_REQUIRE_OK(decoded);
        REQUIRE(decoded.value().rgb.size() == NITS.size() * 3);
        for (usize index = 0; index < NITS.size(); ++index) {
            CAPTURE(NITS[index]);
            const f64 expected = NITS[index] / codec::JPEG_XL_REFERENCE_WHITE_NITS;
            for (usize channel = 0; channel < 3; ++channel) {
                CHECK(decoded.value().rgb[index * 3 + channel] == doctest::Approx(expected).epsilon(0.01));
            }
        }
    }

    TEST_CASE("equirect_image - decode_hdr reads lossy PQ and HLG JPEG XL in units of reference white") {
        // A lossy file goes through libjxl's XYB stage, whose linear output has 1.0 at the intensity target, not
        // through the CMS: flat patches keep their luminance within the lossy error.
        constexpr u32 PATCH = 16;
        constexpr std::array<f64, 3> NITS = {100.0, 203.0, 1000.0};
        std::vector<f64> pq = {};
        std::vector<f64> hlg = {};
        for (const f64 nits : NITS) {
            pq.push_back(pq_signal(nits));
            hlg.push_back(hlg_signal(nits));
        }
        const u32 width = PATCH * static_cast<u32>(NITS.size());
        for (const codec::jpeg_xl_colour_encoding encoding : {
               codec::jpeg_xl_colour_encoding::REC2100_PQ, codec::jpeg_xl_colour_encoding::REC2100_HLG
             }) {
            CAPTURE(static_cast<u32>(encoding));
            const bool isPq = encoding == codec::jpeg_xl_colour_encoding::REC2100_PQ;
            const std::vector<f64> signals = grey_patches(isPq ? pq : hlg, PATCH);
            const std::vector<std::byte> file = grey_float_file(width, signals, encoding, 0.0f, false);
            core::result<equirect_image> decoded = equirect_image::decode_hdr(file);
            CONJURE_REQUIRE_OK(decoded);
            for (usize patch = 0; patch < NITS.size(); ++patch) {
                CAPTURE(NITS[patch]);
                const usize middle = (static_cast<usize>(PATCH / 2) * width + patch * PATCH + PATCH / 2) * 3;
                const f64 expected = NITS[patch] / codec::JPEG_XL_REFERENCE_WHITE_NITS;
                for (usize channel = 0; channel < 3; ++channel) {
                    CHECK(decoded.value().rgb[middle + channel] == doctest::Approx(expected).epsilon(0.05));
                }
            }
        }
    }

    TEST_CASE("equirect_image - a PQ or HLG JPEG XL sky bakes like the same sky stored as floats") {
        // Grey 2 above and 0.5 below: 406 and 101.5 nits.
        constexpr std::array<f64, 2> ROWS = {2.0, 0.5};
        std::vector<f64> linear = {};
        std::vector<f64> pq = {};
        std::vector<f64> hlg = {};
        for (const f64 value : ROWS) {
            for (u32 x = 0; x < 4; ++x) {
                const f64 nits = value * codec::JPEG_XL_REFERENCE_WHITE_NITS;
                linear.push_back(value);
                pq.push_back(pq_signal(nits));
                hlg.push_back(hlg_signal(nits));
            }
        }
        environment_bake_options options = {};
        options.skySize = 8;
        options.prefilteredSize = 8;
        options.sampleCount = 8;
        const auto bake = [&options](const std::vector<std::byte> &file) {
            core::result<equirect_image> decoded = equirect_image::decode_hdr(file);
            CONJURE_REQUIRE_OK(decoded);
            core::result<environment_asset> baked = bake_environment(decoded.value(), options, nullptr);
            CONJURE_REQUIRE_OK(baked);
            return std::move(baked).value();
        };
        const environment_asset reference =
          bake(grey_float_file(4, linear, codec::jpeg_xl_colour_encoding::LINEAR_SRGB, 0.0f));
        const environment_asset fromPq = bake(grey_float_file(4, pq, codec::jpeg_xl_colour_encoding::REC2100_PQ, 0.0f));
        const environment_asset fromHlg =
          bake(grey_float_file(4, hlg, codec::jpeg_xl_colour_encoding::REC2100_HLG, 0.0f));
        for (usize coefficient = 0; coefficient < reference.irradiance().size(); ++coefficient) {
            CAPTURE(coefficient);
            const math::vec3 &expected = reference.irradiance()[coefficient];
            for (const environment_asset *baked : {&fromPq, &fromHlg}) {
                const math::vec3 &actual = baked->irradiance()[coefficient];
                CHECK(actual.x == doctest::Approx(expected.x).epsilon(0.01));
                CHECK(actual.y == doctest::Approx(expected.y).epsilon(0.01));
                CHECK(actual.z == doctest::Approx(expected.z).epsilon(0.01));
            }
        }
        CHECK(reference.irradiance()[0].x > 1.0f);
    }

    TEST_CASE("texture_asset - a PQ or HLG JPEG XL texture puts reference white at sRGB white") {
        // The sRGB curve of nits / 203, clipped at 1; float and 16-bit samples keep the high byte.
        constexpr std::array<f64, 4> NITS = {20.0, 50.0, 203.0, 1000.0};
        const auto expected_byte = [](f64 nits) {
            const f64 linear = std::min(1.0, nits / codec::JPEG_XL_REFERENCE_WHITE_NITS);
            return static_cast<int>(static_cast<u32>(std::lround(srgb_encode(linear) * 65535.0)) >> 8);
        };
        std::vector<u16> pq16 = {};
        std::vector<f64> hlg = {};
        for (const f64 nits : NITS) {
            pq16.insert(pq16.end(), 3, static_cast<u16>(std::lround(pq_signal(nits) * 65535.0)));
            hlg.push_back(hlg_signal(nits));
        }
        const std::vector<std::byte> pqBytes = bytes_of_values(pq16);
        codec::jpeg_xl_encode_desc pq = {};
        pq.width = static_cast<u32>(NITS.size());
        pq.height = 1;
        pq.channelCount = 3;
        pq.sampleType = codec::jpeg_xl_sample_type::UINT16;
        pq.colourEncoding = codec::jpeg_xl_colour_encoding::REC2100_PQ;
        pq.frames.push_back(pqBytes);
        const std::vector<std::byte> pqFile = encode_or_fail(pq);
        const std::vector<std::byte> hlgFile =
          grey_float_file(static_cast<u32>(hlg.size()), hlg, codec::jpeg_xl_colour_encoding::REC2100_HLG, 0.0f);

        const texture_asset fromPq = decode_or_fail(pqFile, texture_colour_space::SRGB);
        const texture_asset fromHlg = decode_or_fail(hlgFile, texture_colour_space::SRGB);
        for (u32 index = 0; index < NITS.size(); ++index) {
            CAPTURE(NITS[index]);
            const int expected = expected_byte(NITS[index]);
            for (const core::rgba8 pixel : {fromPq.pixel(index, 0), fromHlg.pixel(index, 0)}) {
                CHECK(within(expected, pixel.r, 1));
                CHECK(within(expected, pixel.g, 1));
                CHECK(within(expected, pixel.b, 1));
                CHECK(pixel.a == 255);
            }
        }
        CHECK(fromPq.pixel(2, 0).r == 255);

        // LINEAR keeps the PQ signal as stored.
        const texture_asset asStored = decode_or_fail(pqFile, texture_colour_space::LINEAR);
        CHECK(asStored.pixel(2, 0).r == static_cast<u8>(pq16[6] >> 8));
    }

    TEST_CASE("equirect_image - decode_hdr makes NaN, infinite and negative JPEG XL samples finite for the bake") {
        // A 4 x 2 half-float equirect: a sun stored as +inf, NaN, -inf, a negative value and the largest half. Half
        // floats keep libjxl's lossless residuals small next to the infinities.
        std::vector<u16> halves = {
          0x7C00, 0x7E00, 0xC000,                       // +inf, NaN, -2
          0x3C00, 0xFC00, 0x7BFF,                       // 1, -inf, 65504
          0x3800, 0x3400, 0x4000                        // 0.5, 0.25, 2
        };
        halves.insert(halves.end(), 15, u16{0x3C00});
        const std::vector<std::byte> samples = bytes_of_values(halves);
        codec::jpeg_xl_encode_desc desc = {};
        desc.width = 4;
        desc.height = 2;
        desc.channelCount = 3;
        desc.sampleType = codec::jpeg_xl_sample_type::FLOAT16;
        desc.colourEncoding = codec::jpeg_xl_colour_encoding::LINEAR_SRGB;
        desc.frames.push_back(samples);
        const std::vector<std::byte> file = encode_or_fail(desc);

        // libjxl hands the samples out as the file stores them.
        core::result<codec::decoded_hdr_image> raw = codec::decode_jpeg_xl_rgb_float(file, nullptr);
        CONJURE_REQUIRE_OK(raw);
        CHECK(raw.value().rgb[0] == std::numeric_limits<f32>::infinity());
        CHECK(std::isnan(raw.value().rgb[1]));
        CHECK(raw.value().rgb[2] == -2.0f);
        CHECK(raw.value().rgb[4] == -std::numeric_limits<f32>::infinity());

        // decode_hdr gives the bake finite, non-negative radiance: +inf becomes MAX_ENVIRONMENT_RADIANCE and finite
        // values keep what they are.
        core::result<equirect_image> decoded = equirect_image::decode_hdr(file);
        CONJURE_REQUIRE_OK(decoded);
        std::vector<f32> expected = {MAX_ENVIRONMENT_RADIANCE, 0.0f, 0.0f, 1.0f, 0.0f, 65504.0f, 0.5f, 0.25f, 2.0f};
        expected.insert(expected.end(), 15, 1.0f);
        CHECK(decoded.value().rgb == expected);

        environment_bake_options options = {};
        options.skySize = 8;
        options.prefilteredSize = 8;
        options.sampleCount = 8;
        core::result<environment_asset> baked = bake_environment(decoded.value(), options, nullptr);
        CONJURE_REQUIRE_OK(baked);
        for (const math::vec3 &coefficient : baked.value().irradiance()) {
            CHECK(std::isfinite(coefficient.x));
            CHECK(std::isfinite(coefficient.y));
            CHECK(std::isfinite(coefficient.z));
        }
        const math::vec3 up = evaluate_sh9_irradiance(baked.value().irradiance(), math::vec3(0.0f, 1.0f, 0.0f));
        CHECK(std::isfinite(up.x));
        CHECK(std::isfinite(up.y));
        CHECK(std::isfinite(up.z));
    }

    TEST_CASE("texture_asset - truncated and corrupt JPEG XL files fail with DECODE_FAILED naming which") {
        const auto message_of = [](const auto &failed) {
            return failed.has_value() ? std::string() : std::string(failed.get_error().message());
        };
        const std::vector<std::byte> valid = encode_or_fail(pattern_image(40, 24));
        REQUIRE(valid.size() > 64);
        log_capture capture = {};
        for (const usize length : {usize{2}, usize{8}, valid.size() / 2, valid.size() - 1}) {
            CAPTURE(length);
            const std::span<const std::byte> truncated = std::span<const std::byte>(valid).first(length);
            REQUIRE(is_jpeg_xl(truncated));
            const core::result<texture_asset> decoded = texture_asset::decode(truncated, texture_colour_space::SRGB);
            CONJURE_CHECK_ERROR(decoded, renderer_error::DECODE_FAILED);
            CHECK(message_of(decoded).find("JPEG XL: the file is truncated") != std::string::npos);
            const core::result<equirect_image> hdr = equirect_image::decode_hdr(truncated);
            CONJURE_CHECK_ERROR(hdr, renderer_error::DECODE_FAILED);
            CHECK(message_of(hdr).find("truncated") != std::string::npos);
        }

        // The frame data overwritten, and a signature followed by noise.
        std::vector<std::byte> corrupt = valid;
        for (usize index = 16; index < corrupt.size(); ++index) {
            corrupt[index] = static_cast<std::byte>(0xFF);
        }
        const core::result<texture_asset> corruptDecoded = texture_asset::decode(corrupt, texture_colour_space::SRGB);
        CONJURE_CHECK_ERROR(corruptDecoded, renderer_error::DECODE_FAILED);
        CHECK(message_of(corruptDecoded).find("corrupt") != std::string::npos);
        std::vector<std::byte> noise = {std::byte{0xFF}, std::byte{0x0A}};
        for (u32 index = 0; index < 256; ++index) {
            noise.push_back(static_cast<std::byte>((index * 151u + 7u) & 0xFFu));
        }
        const core::result<texture_asset> noiseDecoded = texture_asset::decode(noise, texture_colour_space::LINEAR);
        CONJURE_CHECK_ERROR(noiseDecoded, renderer_error::DECODE_FAILED);
        CHECK(message_of(noiseDecoded).find("corrupt") != std::string::npos);
        CONJURE_CHECK_ERROR(equirect_image::decode_hdr(noise), renderer_error::DECODE_FAILED);
    }

    TEST_CASE("texture_asset - a JPEG XL above MAX_DIMENSION is refused from its header before any pixel exists") {
        // A real file one texel too wide.
        core::image wide(texture_asset::MAX_DIMENSION + 1, 1);
        jpeg_xl_encode_options fast = {};
        fast.effort = 1;
        const std::vector<std::byte> wideFile = encode_or_fail(wide, fast);
        const core::result<texture_asset> refused = texture_asset::decode(wideFile, texture_colour_space::SRGB);
        CONJURE_CHECK_ERROR(refused, core::error_code::INVALID_ARGUMENT);
        if (!refused.has_value()) {
            CHECK(refused.get_error().message().find("texel limit") != std::string::npos);
        }
        CONJURE_CHECK_ERROR(equirect_image::decode_hdr(wideFile), core::error_code::INVALID_ARGUMENT);

        // A header that claims 16385 x 16385 (a gigabyte of RGBA8) and holds no image: refused with next to
        // nothing allocated, where decoding first would have needed the gigabyte.
        const std::vector<std::byte> huge = jpeg_xl_header_only(16385, 16385);
        REQUIRE(is_jpeg_xl(huge));
        const u64 before = core::memory::snapshot().allocatedBytes;
        const core::result<texture_asset> hugeRefused = texture_asset::decode(huge, texture_colour_space::SRGB);
        const u64 allocated = core::memory::snapshot().allocatedBytes - before;
        CONJURE_CHECK_ERROR(hugeRefused, core::error_code::INVALID_ARGUMENT);
        CHECK(allocated < 64ull * 1024 * 1024);
        CONJURE_CHECK_ERROR(equirect_image::decode_hdr(huge), core::error_code::INVALID_ARGUMENT);

        // Within the limit the same header is decoded on: its 64 zero bytes read as a frame whose sections are all
        // empty, which libjxl rejects.
        const core::result<texture_asset> withinLimit =
          texture_asset::decode(jpeg_xl_header_only(300, 200), texture_colour_space::SRGB);
        CONJURE_CHECK_ERROR(withinLimit, renderer_error::DECODE_FAILED);
        if (!withinLimit.has_value()) {
            CHECK(withinLimit.get_error().message().find("corrupt") != std::string::npos);
        }
        CONJURE_CHECK_ERROR(texture_asset::decode({}, texture_colour_space::SRGB), core::error_code::INVALID_ARGUMENT);
    }

    TEST_CASE("texture_asset - a JPEG XL declaring too many extra channels is refused from its header") {
        // 4096 extra channels on a 4096 x 4096 image: libjxl would allocate a 64 MiB plane for each of them, 256 GiB
        // in all, from a header of a few hundred bytes.
        const std::vector<std::byte> many = jpeg_xl_header_with_extra_channels(4096, 4096, 4096);
        const u64 before = core::memory::snapshot().allocatedBytes;
        const core::result<texture_asset> refused = texture_asset::decode(many, texture_colour_space::SRGB);
        const u64 allocated = core::memory::snapshot().allocatedBytes - before;
        CONJURE_CHECK_ERROR(refused, core::error_code::INVALID_ARGUMENT);
        if (!refused.has_value()) {
            const std::string_view message = refused.get_error().message();
            CHECK(message.find("4096 extra channels exceed the limit of 16") != std::string_view::npos);
        }
        CHECK(allocated < 16ull * 1024 * 1024);
        CONJURE_CHECK_ERROR(equirect_image::decode_hdr(many), core::error_code::INVALID_ARGUMENT);

        // One above the limit is refused. At the limit the header passes and the decode goes on: its 64 zero bytes
        // read as a frame whose sections are all empty, which libjxl rejects as corrupt, and without them the file
        // ends where the frame should start.
        const u32 limit = codec::JPEG_XL_MAX_EXTRA_CHANNELS;
        CONJURE_CHECK_ERROR(
          texture_asset::decode(jpeg_xl_header_with_extra_channels(64, 64, limit + 1), texture_colour_space::SRGB),
          core::error_code::INVALID_ARGUMENT
        );
        const std::vector<std::byte> atLimit = jpeg_xl_header_with_extra_channels(64, 64, limit);
        const core::result<texture_asset> zeroFrame = texture_asset::decode(atLimit, texture_colour_space::SRGB);
        CONJURE_CHECK_ERROR(zeroFrame, renderer_error::DECODE_FAILED);
        if (!zeroFrame.has_value()) {
            CHECK(zeroFrame.get_error().message().find("corrupt") != std::string::npos);
        }
        const std::span<const std::byte> headerOnly = std::span<const std::byte>(atLimit).first(atLimit.size() - 64);
        const core::result<texture_asset> noFrame = texture_asset::decode(headerOnly, texture_colour_space::SRGB);
        CONJURE_CHECK_ERROR(noFrame, renderer_error::DECODE_FAILED);
        if (!noFrame.has_value()) {
            CHECK(noFrame.get_error().message().find("truncated") != std::string::npos);
        }
    }

    TEST_CASE("texture_asset - a JPEG XL layer above MAX_DIMENSION is refused before its frame is decoded") {
        // The header says 16 x 16; the only frame claims 20000 x 20000 in a file of a few hundred bytes. Decoding it
        // would allocate the frame's state and full planes (gigabytes) to produce 16 x 16 texels.
        const std::vector<std::byte> file = jpeg_xl_with_frame_size(16, 16, 20000, 20000);
        REQUIRE(file.size() < 1024);
        const u64 before = core::memory::snapshot().allocatedBytes;
        const core::result<texture_asset> refused = texture_asset::decode(file, texture_colour_space::SRGB);
        const u64 allocated = core::memory::snapshot().allocatedBytes - before;
        CONJURE_CHECK_ERROR(refused, core::error_code::INVALID_ARGUMENT);
        if (!refused.has_value()) {
            CHECK(refused.get_error().message().find("20000x20000 layer exceeds the 16384 texel limit") !=
              std::string::npos);
        }
        CHECK(allocated < 16ull * 1024 * 1024);
        CONJURE_CHECK_ERROR(equirect_image::decode_hdr(file), core::error_code::INVALID_ARGUMENT);

        // Only the width can be too large as well.
        CONJURE_CHECK_ERROR(
          texture_asset::decode(jpeg_xl_with_frame_size(16, 16, 16385, 16), texture_colour_space::SRGB),
          core::error_code::INVALID_ARGUMENT
        );
    }

    TEST_CASE("texture_asset - libjxl's memory for a JPEG XL is bounded by the size of the image") {
        // A 16 x 16 image whose only frame is reference-only (decoded for later frames, never reported, so no
        // header check sees its size) and claims 65536 x 65536: its state alone is far above the image's limit.
        const std::vector<std::byte> file = jpeg_xl_with_frame_size(16, 16, 65536, 65536, frame_kind::REFERENCE_ONLY);
        REQUIRE(file.size() < 8192);
        const u64 limit = codec::JPEG_XL_BUDGET_BASE_BYTES + codec::JPEG_XL_BUDGET_THREADS_BYTES +
          16ull * 16 * 3 * codec::JPEG_XL_BUDGET_SAMPLE_BYTES;
        const u64 before = core::memory::snapshot().allocatedBytes;
        const core::result<texture_asset> refused = texture_asset::decode(file, texture_colour_space::SRGB);
        const u64 allocated = core::memory::snapshot().allocatedBytes - before;
        CONJURE_CHECK_ERROR(refused, core::error_code::INVALID_ARGUMENT);
        if (!refused.has_value()) {
            CHECK(refused.get_error().message().find("memory limit") != std::string::npos);
        }
        CHECK(allocated < limit);
        CONJURE_CHECK_ERROR(equirect_image::decode_hdr(file), core::error_code::INVALID_ARGUMENT);

        // A layer smaller than the image still decodes, onto the image's canvas.
        core::image layer(8, 8);
        layer.fill(core::rgba8{200, 100, 50, 255});
        codec::jpeg_xl_encode_desc partial = {};
        partial.width = 16;
        partial.height = 16;
        partial.layerWidth = 8;
        partial.layerHeight = 8;
        partial.frames.push_back(layer.pixels());
        const texture_asset canvas = decode_or_fail(encode_or_fail(partial), texture_colour_space::SRGB);
        CHECK(canvas.width() == 16);
        CHECK(canvas.pixel(3, 3) == core::rgba8{200, 100, 50, 255});
    }

    TEST_CASE("regression - a JPEG XL near its memory limit decodes or fails alike inline and on workers [parallel]") {
        // The limit counts JPEG_XL_MAX_THREADS threads whatever the job system has. It used to count the threads
        // at hand, so a 16 x 16 image whose frame state needs about 94 MB was refused inline (a 40 MiB limit) and
        // read on to its end on eight workers (104 MiB), and a refusal named a different limit per worker count.
        const std::vector<std::byte> withinLimit = jpeg_xl_with_frame_size(16, 16, 16384, 16384);
        const std::vector<std::byte> aboveLimit =
          jpeg_xl_with_frame_size(16, 16, 32768, 32768, frame_kind::REFERENCE_ONLY);
        const u64 limit = codec::JPEG_XL_BUDGET_BASE_BYTES + codec::JPEG_XL_BUDGET_THREADS_BYTES +
          16ull * 16 * 3 * codec::JPEG_XL_BUDGET_SAMPLE_BYTES;
        const std::string limitText = std::to_string(limit / (1024 * 1024)) + " MiB memory limit";
        const auto message_of = [](std::span<const std::byte> file, core::jobs::job_system *jobs) {
            const core::result<texture_asset> decoded =
              texture_asset::decode(file, texture_colour_space::SRGB, false, jobs);
            return decoded.has_value() ? std::string("decoded") : std::string(decoded.get_error().message());
        };

        // The empty sections end the file before any image: past the limit check, it reads as truncated.
        const std::string inlineWithin = message_of(withinLimit, nullptr);
        const std::string inlineAbove = message_of(aboveLimit, nullptr);
        CHECK(inlineWithin.find("truncated") != std::string::npos);
        CHECK(inlineAbove.find(limitText) != std::string::npos);
        for (const u32 workerCount : {1u, 2u, 8u}) {
            CAPTURE(workerCount);
            core::jobs::job_system jobs = {};
            CONJURE_REQUIRE_OK(jobs.start(workers(workerCount)));
            CHECK(message_of(withinLimit, &jobs) == inlineWithin);
            CHECK(message_of(aboveLimit, &jobs) == inlineAbove);
            jobs.stop();
        }
    }

    TEST_CASE("texture_asset - a JPEG XL ICC profile size read from the header allocates nothing by itself") {
        // A header announcing a compressed ICC profile of 256 MiB - 1 (libjxl's ceiling) with no profile data: libjxl
        // grows its buffers only as the profile's data arrives, so the file reads as truncated, not as 256 MiB.
        const std::vector<std::byte> file = jpeg_xl_header_with_icc_size(64, 64);
        const u64 before = core::memory::snapshot().allocatedBytes;
        const core::result<texture_asset> decoded = texture_asset::decode(file, texture_colour_space::SRGB);
        const u64 allocated = core::memory::snapshot().allocatedBytes - before;
        CONJURE_CHECK_ERROR(decoded, renderer_error::DECODE_FAILED);
        if (!decoded.has_value()) {
            CHECK(decoded.get_error().message().find("truncated") != std::string::npos);
        }
        CHECK(allocated < 16ull * 1024 * 1024);
    }

    TEST_CASE("texture_asset - a JPEG XL box that claims more bytes than the file holds allocates none of them") {
        // A complete codestream in a container, then an Exif box whose 64-bit size claims 2^62 bytes. No box event
        // is asked for, so libjxl ends at the codestream's last frame and never sizes anything by the box.
        const core::image image = pattern_image(8, 8);
        const std::vector<std::byte> file = container_with_huge_box(encode_or_fail(image));
        REQUIRE(is_jpeg_xl(file));
        const u64 before = core::memory::snapshot().allocatedBytes;
        const core::result<texture_asset> decoded = texture_asset::decode(file, texture_colour_space::SRGB);
        const u64 allocated = core::memory::snapshot().allocatedBytes - before;
        CONJURE_REQUIRE_OK(decoded);
        CHECK(same_pixels(image, decoded.value()));
        CHECK(allocated < 16ull * 1024 * 1024);
    }

    TEST_CASE("texture_asset - a JPEG XL container with Exif ahead of a codestream split across jxlp boxes decodes") {
        // Both passes of the decoder (the layer sizes, then the image) read the codestream through the boxes, and
        // the Exif box before it is skipped: no box event is asked for.
        const core::image image = pattern_image(40, 24);
        const std::vector<std::byte> codestream = encode_or_fail(image);
        REQUIRE(codestream.size() > 2);
        REQUIRE(codestream[0] == std::byte{0xFF});      // a bare codestream, wrapped by hand below
        const std::vector<std::byte> file = container_with_split_codestream(codestream);
        REQUIRE(is_jpeg_xl(file));
        for (const texture_colour_space colourSpace : BOTH_COLOUR_SPACES) {
            CAPTURE(std::string(to_string(colourSpace)));
            CHECK(same_pixels(image, decode_or_fail(file, colourSpace)));
        }
    }

    TEST_CASE("texture_asset - JPEG XL decoding is the same bytes every run and with one or many workers [parallel]") {
        const core::image image = gradient_image(300, 280);
        jpeg_xl_encode_options lossy = {};
        lossy.lossless = false;
        lossy.distance = 2.0f;
        lossy.effort = 3;
        jpeg_xl_encode_options lossless = {};
        lossless.effort = 2;

        core::jobs::job_system single = {};
        core::jobs::job_system many = {};
        CONJURE_REQUIRE_OK(single.start(workers(1)));
        CONJURE_REQUIRE_OK(many.start(workers(8)));
        for (const jpeg_xl_encode_options &options : {lossy, lossless}) {
            CAPTURE(options.lossless);
            const std::vector<std::byte> encoded = encode_or_fail(image, options);
            for (const texture_colour_space colourSpace : BOTH_COLOUR_SPACES) {
                CAPTURE(std::string(to_string(colourSpace)));
                const texture_asset reference = decode_or_fail(encoded, colourSpace);
                CHECK(decode_or_fail(encoded, colourSpace) == reference);
                CHECK(decode_or_fail(encoded, colourSpace, &single) == reference);
                CHECK(decode_or_fail(encoded, colourSpace, &many) == reference);
                CHECK(decode_or_fail(encoded, colourSpace, &many) == reference);
            }
            core::result<equirect_image> hdrInline = equirect_image::decode_hdr(encoded);
            core::result<equirect_image> hdrParallel = equirect_image::decode_hdr(encoded, &many);
            CONJURE_REQUIRE_OK(hdrInline);
            CONJURE_REQUIRE_OK(hdrParallel);
            CHECK(hdrParallel.value().rgb == hdrInline.value().rgb);
        }
        // The decode did run on the workers.
        CHECK(many.stats().scheduled > 0);
        CHECK(single.stats().scheduled > 0);
        many.stop();
        single.stop();

        // A stopped system (no workers) decodes inline.
        const std::vector<std::byte> encoded = encode_or_fail(image, lossless);
        const texture_asset stopped = decode_or_fail(encoded, texture_colour_space::SRGB, &many);
        CHECK(stopped == decode_or_fail(encoded, texture_colour_space::SRGB));
    }

    TEST_CASE("equirect_image - decode_hdr reads a float JPEG XL as linear light and keeps values above 1") {
        const std::vector<f32> radiance = {
          0.0f, 0.5f, 2.0f,     10.0f, 0.25f, 1.0f,
          64.0f, 0.125f, 3.0f,  0.75f, 1.5f, 0.0625f
        };
        const std::vector<std::byte> samples = bytes_of_values(radiance);
        codec::jpeg_xl_encode_desc desc = {};
        desc.width = 2;
        desc.height = 2;
        desc.channelCount = 3;
        desc.sampleType = codec::jpeg_xl_sample_type::FLOAT32;
        desc.colourEncoding = codec::jpeg_xl_colour_encoding::LINEAR_SRGB;
        desc.frames.push_back(samples);
        core::result<equirect_image> decoded = equirect_image::decode_hdr(encode_or_fail(desc));
        CONJURE_REQUIRE_OK(decoded);
        CHECK(decoded.value().width == 2);
        CHECK(decoded.value().height == 2);
        CHECK(decoded.value().is_valid());
        CHECK(decoded.value().rgb == radiance);

        // Half floats and an sRGB-encoded 8-bit file reach linear light too; grey fills all three channels.
        std::vector<u16> halves = {};
        for (const f32 value : {0.5f, 4.0f}) {
            halves.push_back(half_bits(value));
        }
        const std::vector<std::byte> halfSamples = bytes_of_values(halves);
        desc.width = 2;
        desc.height = 1;
        desc.channelCount = 1;
        desc.sampleType = codec::jpeg_xl_sample_type::FLOAT16;
        desc.frames = {halfSamples};
        core::result<equirect_image> grey = equirect_image::decode_hdr(encode_or_fail(desc));
        CONJURE_REQUIRE_OK(grey);
        CHECK(grey.value().rgb == std::vector<f32>{0.5f, 0.5f, 0.5f, 4.0f, 4.0f, 4.0f});

        core::image srgb(1, 1);
        srgb.fill(core::rgba8{188, 255, 0, 255});
        core::result<equirect_image> fromSrgb = equirect_image::decode_hdr(encode_or_fail(srgb));
        CONJURE_REQUIRE_OK(fromSrgb);
        CHECK(fromSrgb.value().rgb[0] == doctest::Approx(srgb_decode(188.0 / 255.0)).epsilon(0.002));
        CHECK(fromSrgb.value().rgb[1] == doctest::Approx(1.0f).epsilon(0.002));
        CHECK(fromSrgb.value().rgb[2] == doctest::Approx(0.0f).epsilon(0.002));
    }

    TEST_CASE("texture_asset - load_from_file picks the decoder by signature, not by the file's name") {
        temp_directory directory("jpeg_xl_names");
        const core::image image = pattern_image(4, 3);
        const std::vector<std::byte> jxl = encode_or_fail(image);
        core::result<std::vector<std::byte>> png = image.encode_png();
        CONJURE_REQUIRE_OK(png);
        const auto write = [&directory](std::string_view name, std::span<const std::byte> bytes) {
            const std::string_view contents(reinterpret_cast<const char *>(bytes.data()), bytes.size());
            return directory.write_file(name, contents);
        };
        const std::filesystem::path jxlAsPng = write("really_jpeg_xl.png", jxl);
        const std::filesystem::path pngAsJxl = write("really_png.jxl", png.value());
        const std::filesystem::path honest = write("honest.jxl", jxl);
        for (const std::filesystem::path &path : {jxlAsPng, pngAsJxl, honest}) {
            CAPTURE(path.filename().generic_string());
            core::result<texture_asset> loaded = texture_asset::load_from_file(path, texture_colour_space::SRGB, false);
            CONJURE_REQUIRE_OK(loaded);
            CHECK(same_pixels(image, loaded.value()));
            CHECK(loaded.value().name() == path.filename().generic_string());
        }
    }
}
