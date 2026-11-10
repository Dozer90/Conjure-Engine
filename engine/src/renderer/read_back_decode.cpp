#include "renderer/read_back_decode.h"
#include "renderer/import/mip_generator.h"

#include "conjure/renderer/mesh_asset.h"

#include "conjure/core/math.h"

#include <cmath>
#include <cstring>

using namespace conjure;
using namespace conjure::renderer;

f32 conjure::renderer::decode_r11g11b10_channel(u32 bits, u32 mantissaBits) noexcept {
    const u32 exponent = (bits >> mantissaBits) & 0x1Fu;
    const u32 mantissa = bits & ((1u << mantissaBits) - 1u);
    if (exponent == 0) {
        return std::ldexp(static_cast<f32>(mantissa), -14 - static_cast<int>(mantissaBits));
    }
    if (exponent == 0x1Fu) {
        return mantissa == 0 ? std::numeric_limits<f32>::infinity() : std::numeric_limits<f32>::quiet_NaN();
    }
    return std::ldexp(
      1.0f + static_cast<f32>(mantissa) / static_cast<f32>(1u << mantissaBits),
      static_cast<int>(exponent) - 15
    );
}

u8 conjure::renderer::encode_srgb8(f32 linear) noexcept {
    if (std::isnan(linear)) {
        return 0;
    }
    return static_cast<u8>(std::lround(math::saturate(detail::linear_to_srgb(math::saturate(linear))) * 255.0f));
}

core::rgba8 conjure::renderer::decode_read_back_texel(rhi::texture_format format, const std::byte *texel) noexcept {
    core::rgba8 colour = {};
    switch (format) {
        case rhi::texture_format::RGBA8_UNORM:
        case rhi::texture_format::RGBA8_SRGB: {
            colour.r = static_cast<u8>(texel[0]);
            colour.g = static_cast<u8>(texel[1]);
            colour.b = static_cast<u8>(texel[2]);
            colour.a = static_cast<u8>(texel[3]);
            break;
        }
        case rhi::texture_format::BGRA8_SRGB: {
            colour.r = static_cast<u8>(texel[2]);
            colour.g = static_cast<u8>(texel[1]);
            colour.b = static_cast<u8>(texel[0]);
            colour.a = static_cast<u8>(texel[3]);
            break;
        }
        case rhi::texture_format::RGBA16_FLOAT: {
            u16 halves[4] = {};
            std::memcpy(halves, texel, sizeof(halves));
            colour.r = encode_srgb8(unpack_half(halves[0]));
            colour.g = encode_srgb8(unpack_half(halves[1]));
            colour.b = encode_srgb8(unpack_half(halves[2]));
            const f32 alpha = unpack_half(halves[3]);
            colour.a = std::isnan(alpha) ? 0 : static_cast<u8>(std::lround(math::saturate(alpha) * 255.0f));
            break;
        }
        case rhi::texture_format::R11G11B10_FLOAT: {
            u32 packed = 0;
            std::memcpy(&packed, texel, sizeof(packed));
            colour.r = encode_srgb8(decode_r11g11b10_channel(packed & 0x7FFu, 6));
            colour.g = encode_srgb8(decode_r11g11b10_channel((packed >> 11) & 0x7FFu, 6));
            colour.b = encode_srgb8(decode_r11g11b10_channel((packed >> 22) & 0x3FFu, 5));
            colour.a = 255;
            break;
        }
        default: {
            break;
        }
    }
    return colour;
}
