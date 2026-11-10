#include "renderer/import/mip_generator.h"

#include "conjure/core/math.h"

#include <algorithm>
#include <array>
#include <cmath>

using namespace conjure;
using namespace conjure::renderer;

namespace {
    /// <summary>
    /// Decoded value of every sRGB byte, built once.
    /// </summary>
    [[nodiscard]] const std::array<f32, 256> &srgb_decode_table() noexcept {
        static const std::array<f32, 256> table = []() {
            std::array<f32, 256> values = {};
            for (usize index = 0; index < values.size(); ++index) {
                values[index] = detail::srgb_to_linear(static_cast<f32>(index) / 255.0f);
            }
            return values;
        }();
        return table;
    }

    [[nodiscard]] u8 to_byte(f32 value) noexcept {
        return static_cast<u8>(std::lround(math::saturate(value) * 255.0f));
    }
}

f32 conjure::renderer::detail::srgb_to_linear(f32 encoded) noexcept {
    const f32 value = math::saturate(encoded);
    if (value <= 0.04045f) {
        return value / 12.92f;
    }
    return std::pow((value + 0.055f) / 1.055f, 2.4f);
}

f32 conjure::renderer::detail::linear_to_srgb(f32 linear) noexcept {
    const f32 value = math::saturate(linear);
    if (value <= 0.0031308f) {
        return value * 12.92f;
    }
    return 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
}

texture_mip conjure::renderer::detail::downsample_rgba8(const texture_mip &source, bool srgb) {
    texture_mip result = {};
    result.width = std::max<u32>(1, source.width / 2);
    result.height = std::max<u32>(1, source.height / 2);
    result.pixels.resize(static_cast<usize>(result.width) * result.height * 4);

    const std::array<f32, 256> &decode = srgb_decode_table();
    const auto sourcePixel = [&source](u32 x, u32 y) -> const std::byte * {
        const u32 clampedX = std::min(x, source.width - 1);
        const u32 clampedY = std::min(y, source.height - 1);
        return source.pixels.data() + (static_cast<usize>(clampedY) * source.width + clampedX) * 4;
    };

    for (u32 y = 0; y < result.height; ++y) {
        for (u32 x = 0; x < result.width; ++x) {
            std::array<f32, 4> sum = {0.0f, 0.0f, 0.0f, 0.0f};
            for (u32 dy = 0; dy < 2; ++dy) {
                for (u32 dx = 0; dx < 2; ++dx) {
                    const std::byte *texel = sourcePixel(x * 2 + dx, y * 2 + dy);
                    for (usize channel = 0; channel < 3; ++channel) {
                        const u8 encoded = static_cast<u8>(texel[channel]);
                        sum[channel] += srgb ? decode[encoded] : static_cast<f32>(encoded) / 255.0f;
                    }
                    sum[3] += static_cast<f32>(static_cast<u8>(texel[3])) / 255.0f;
                }
            }
            std::byte *destination = result.pixels.data() + (static_cast<usize>(y) * result.width + x) * 4;
            for (usize channel = 0; channel < 3; ++channel) {
                const f32 average = sum[channel] * 0.25f;
                destination[channel] = static_cast<std::byte>(to_byte(srgb ? linear_to_srgb(average) : average));
            }
            destination[3] = static_cast<std::byte>(to_byte(sum[3] * 0.25f));
        }
    }
    return result;
}

std::vector<texture_mip> conjure::renderer::detail::generate_rgba8_mips(
  u32 width,
  u32 height,
  std::span<const std::byte> pixels,
  bool srgb
) {
    std::vector<texture_mip> chain = {};
    texture_mip base = {};
    base.width = width;
    base.height = height;
    base.pixels.assign(pixels.begin(), pixels.end());
    chain.push_back(std::move(base));
    while (chain.back().width > 1 || chain.back().height > 1) {
        chain.push_back(downsample_rgba8(chain.back(), srgb));
    }
    return chain;
}
