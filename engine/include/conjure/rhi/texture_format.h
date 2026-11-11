#pragma once

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <string_view>

// Texture formats the engine uses (docs/design/02_renderer.md 4.1). The list is deliberately
// short: every renderer target, vertex stream and asset payload is one of these, and
// engine/src/rhi/texture_format_mapping.h maps each one to its NVRHI format (verified at compile
// time). The block-compressed formats are reserved for a later cook step and never produced yet.

namespace conjure::rhi {

    enum class texture_format : u8 {
        UNKNOWN = 0,
        R8_UNORM,
        RG8_UNORM,
        RGBA8_UNORM,
        RGBA8_SRGB,
        BGRA8_SRGB,
        RG16_SNORM,
        RGBA16_SNORM,
        R16_FLOAT,
        RG16_FLOAT,
        RGBA16_FLOAT,
        R32_FLOAT,
        RG32_FLOAT,
        RGB32_FLOAT,
        RGBA32_FLOAT,
        R32_UINT,
        R11G11B10_FLOAT,
        D32_FLOAT,
        BC1_SRGB,
        BC5_UNORM,
        BC7_SRGB,
        COUNT
    };

    /// <summary>
    /// Bytes of one pixel of an uncompressed format. 0 for UNKNOWN and for block-compressed
    /// formats, whose storage is described by bytes_per_block() and block_size().
    /// </summary>
    [[nodiscard]] constexpr u32 bytes_per_pixel(texture_format format) noexcept {
        switch (format) {
            case texture_format::R8_UNORM: {
                return 1;
            }
            case texture_format::RG8_UNORM:
            case texture_format::R16_FLOAT: {
                return 2;
            }
            case texture_format::RGBA8_UNORM:
            case texture_format::RGBA8_SRGB:
            case texture_format::BGRA8_SRGB:
            case texture_format::RG16_SNORM:
            case texture_format::RG16_FLOAT:
            case texture_format::R32_FLOAT:
            case texture_format::R32_UINT:
            case texture_format::R11G11B10_FLOAT:
            case texture_format::D32_FLOAT: {
                return 4;
            }
            case texture_format::RGBA16_SNORM:
            case texture_format::RGBA16_FLOAT:
            case texture_format::RG32_FLOAT: {
                return 8;
            }
            case texture_format::RGB32_FLOAT: {
                return 12;
            }
            case texture_format::RGBA32_FLOAT: {
                return 16;
            }
            case texture_format::UNKNOWN:
            case texture_format::BC1_SRGB:
            case texture_format::BC5_UNORM:
            case texture_format::BC7_SRGB:
            case texture_format::COUNT: {
                return 0;
            }
        }
        return 0;
    }

    /// <summary>
    /// True for the block-compressed formats (BC1, BC5, BC7).
    /// </summary>
    [[nodiscard]] constexpr bool is_compressed(texture_format format) noexcept {
        return format == texture_format::BC1_SRGB || format == texture_format::BC5_UNORM ||
          format == texture_format::BC7_SRGB;
    }

    /// <summary>
    /// Width and height in pixels of one storage block: 4 for the compressed formats, 1 otherwise
    /// (0 for UNKNOWN).
    /// </summary>
    [[nodiscard]] constexpr u32 block_size(texture_format format) noexcept {
        if (format == texture_format::UNKNOWN || format == texture_format::COUNT) {
            return 0;
        }
        return is_compressed(format) ? 4 : 1;
    }

    /// <summary>
    /// Bytes of one storage block: bytes_per_pixel() for uncompressed formats, 8 for BC1 and 16
    /// for BC5 and BC7.
    /// </summary>
    [[nodiscard]] constexpr u32 bytes_per_block(texture_format format) noexcept {
        switch (format) {
            case texture_format::BC1_SRGB: {
                return 8;
            }
            case texture_format::BC5_UNORM:
            case texture_format::BC7_SRGB: {
                return 16;
            }
            default: {
                return bytes_per_pixel(format);
            }
        }
    }

    /// <summary>
    /// True for depth formats (D32_FLOAT).
    /// </summary>
    [[nodiscard]] constexpr bool is_depth(texture_format format) noexcept {
        return format == texture_format::D32_FLOAT;
    }

    /// <summary>
    /// True for formats whose values are sRGB encoded and decoded by the sampler.
    /// </summary>
    [[nodiscard]] constexpr bool is_srgb(texture_format format) noexcept {
        return format == texture_format::RGBA8_SRGB || format == texture_format::BGRA8_SRGB ||
          format == texture_format::BC1_SRGB || format == texture_format::BC7_SRGB;
    }

    /// <summary>
    /// Bytes of one tightly packed row of 'width' pixels, rounded up to whole blocks for the
    /// compressed formats. 0 for UNKNOWN.
    /// </summary>
    [[nodiscard]] constexpr u64 row_pitch(texture_format format, u32 width) noexcept {
        const u32 block = block_size(format);
        if (block == 0) {
            return 0;
        }
        const u64 blocksPerRow = (static_cast<u64>(width) + block - 1) / block;
        return blocksPerRow * bytes_per_block(format);
    }

    /// <summary>
    /// The enumerator name of a format ("RGBA8_SRGB"); "UNKNOWN" for UNKNOWN and out-of-range values.
    /// </summary>
    [[nodiscard]] CONJURE_API std::string_view to_string(texture_format format) noexcept;

    /// <summary>
    /// Parses an enumerator name case-insensitively. Fails with core::error_code::INVALID_ARGUMENT
    /// for anything that is not a format name; "UNKNOWN" parses to UNKNOWN.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<texture_format> parse_texture_format(std::string_view name) noexcept;
}
