#pragma once

#include "conjure/rhi/texture_format.h"

#include <nvrhi/nvrhi.h>

// texture_format <-> nvrhi::Format. Private to the rhi module: public headers never include NVRHI
// (docs/design/02_renderer.md R15). Both directions are constexpr so texture_format.cpp can
// static_assert the round trip of every enumerator.

namespace conjure::rhi {

    /// <summary>
    /// The NVRHI format of an engine format; UNKNOWN for UNKNOWN and COUNT.
    /// </summary>
    [[nodiscard]] constexpr nvrhi::Format to_nvrhi_format(texture_format format) noexcept {
        switch (format) {
            case texture_format::R8_UNORM: {
                return nvrhi::Format::R8_UNORM;
            }
            case texture_format::RG8_UNORM: {
                return nvrhi::Format::RG8_UNORM;
            }
            case texture_format::RGBA8_UNORM: {
                return nvrhi::Format::RGBA8_UNORM;
            }
            case texture_format::RGBA8_SRGB: {
                return nvrhi::Format::SRGBA8_UNORM;
            }
            case texture_format::BGRA8_SRGB: {
                return nvrhi::Format::SBGRA8_UNORM;
            }
            case texture_format::RG16_SNORM: {
                return nvrhi::Format::RG16_SNORM;
            }
            case texture_format::RGBA16_SNORM: {
                return nvrhi::Format::RGBA16_SNORM;
            }
            case texture_format::R16_FLOAT: {
                return nvrhi::Format::R16_FLOAT;
            }
            case texture_format::RG16_FLOAT: {
                return nvrhi::Format::RG16_FLOAT;
            }
            case texture_format::RGBA16_FLOAT: {
                return nvrhi::Format::RGBA16_FLOAT;
            }
            case texture_format::R32_FLOAT: {
                return nvrhi::Format::R32_FLOAT;
            }
            case texture_format::RG32_FLOAT: {
                return nvrhi::Format::RG32_FLOAT;
            }
            case texture_format::RGB32_FLOAT: {
                return nvrhi::Format::RGB32_FLOAT;
            }
            case texture_format::RGBA32_FLOAT: {
                return nvrhi::Format::RGBA32_FLOAT;
            }
            case texture_format::R32_UINT: {
                return nvrhi::Format::R32_UINT;
            }
            case texture_format::R11G11B10_FLOAT: {
                return nvrhi::Format::R11G11B10_FLOAT;
            }
            case texture_format::D32_FLOAT: {
                return nvrhi::Format::D32;
            }
            case texture_format::BC1_SRGB: {
                return nvrhi::Format::BC1_UNORM_SRGB;
            }
            case texture_format::BC5_UNORM: {
                return nvrhi::Format::BC5_UNORM;
            }
            case texture_format::BC7_SRGB: {
                return nvrhi::Format::BC7_UNORM_SRGB;
            }
            case texture_format::UNKNOWN:
            case texture_format::COUNT: {
                return nvrhi::Format::UNKNOWN;
            }
        }
        return nvrhi::Format::UNKNOWN;
    }

    /// <summary>
    /// The engine format of an NVRHI format; UNKNOWN for formats the engine does not use.
    /// </summary>
    [[nodiscard]] constexpr texture_format from_nvrhi_format(nvrhi::Format format) noexcept {
        switch (format) {
            case nvrhi::Format::R8_UNORM: {
                return texture_format::R8_UNORM;
            }
            case nvrhi::Format::RG8_UNORM: {
                return texture_format::RG8_UNORM;
            }
            case nvrhi::Format::RGBA8_UNORM: {
                return texture_format::RGBA8_UNORM;
            }
            case nvrhi::Format::SRGBA8_UNORM: {
                return texture_format::RGBA8_SRGB;
            }
            case nvrhi::Format::SBGRA8_UNORM: {
                return texture_format::BGRA8_SRGB;
            }
            case nvrhi::Format::RG16_SNORM: {
                return texture_format::RG16_SNORM;
            }
            case nvrhi::Format::RGBA16_SNORM: {
                return texture_format::RGBA16_SNORM;
            }
            case nvrhi::Format::R16_FLOAT: {
                return texture_format::R16_FLOAT;
            }
            case nvrhi::Format::RG16_FLOAT: {
                return texture_format::RG16_FLOAT;
            }
            case nvrhi::Format::RGBA16_FLOAT: {
                return texture_format::RGBA16_FLOAT;
            }
            case nvrhi::Format::R32_FLOAT: {
                return texture_format::R32_FLOAT;
            }
            case nvrhi::Format::RG32_FLOAT: {
                return texture_format::RG32_FLOAT;
            }
            case nvrhi::Format::RGB32_FLOAT: {
                return texture_format::RGB32_FLOAT;
            }
            case nvrhi::Format::RGBA32_FLOAT: {
                return texture_format::RGBA32_FLOAT;
            }
            case nvrhi::Format::R32_UINT: {
                return texture_format::R32_UINT;
            }
            case nvrhi::Format::R11G11B10_FLOAT: {
                return texture_format::R11G11B10_FLOAT;
            }
            case nvrhi::Format::D32: {
                return texture_format::D32_FLOAT;
            }
            case nvrhi::Format::BC1_UNORM_SRGB: {
                return texture_format::BC1_SRGB;
            }
            case nvrhi::Format::BC5_UNORM: {
                return texture_format::BC5_UNORM;
            }
            case nvrhi::Format::BC7_UNORM_SRGB: {
                return texture_format::BC7_SRGB;
            }
            default: {
                return texture_format::UNKNOWN;
            }
        }
    }
}
