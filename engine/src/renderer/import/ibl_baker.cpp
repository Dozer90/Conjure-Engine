#include "renderer/import/ibl_baker.h"

#include "conjure/renderer/mesh_asset.h"

#include "conjure/core/assert.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <functional>

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::renderer::detail;

namespace {
    constexpr f32 SH_Y00 = 0.282095f;
    constexpr f32 SH_Y1 = 0.488603f;
    constexpr f32 SH_Y2_XY = 1.092548f;
    constexpr f32 SH_Y20 = 0.315392f;
    constexpr f32 SH_Y22 = 0.546274f;

    /// <summary>
    /// Runs body(face) for the six faces, on jobs when available.
    /// </summary>
    void for_each_face(core::jobs::job_system *jobs, const std::function<void(u32 face)> &body) {
        if (jobs == nullptr || jobs->worker_count() == 0) {
            for (u32 face = 0; face < CUBEMAP_FACE_COUNT; ++face) {
                body(face);
            }
            return;
        }
        const std::function<void(usize, usize)> range = [&body](usize begin, usize end) {
            for (usize face = begin; face < end; ++face) {
                body(static_cast<u32>(face));
            }
        };
        jobs->parallel_for(CUBEMAP_FACE_COUNT, 1, range);
    }

    /// <summary>
    /// Integral used for the exact solid angle of a cubemap texel.
    /// </summary>
    [[nodiscard]] f32 area_element(f32 x, f32 y) noexcept {
        return std::atan2(x * y, std::sqrt(x * x + y * y + 1.0f));
    }

    [[nodiscard]] f32 texel_solid_angle(u32 x, u32 y, u32 size) noexcept {
        const f32 inverse = 1.0f / static_cast<f32>(size);
        const f32 x0 = 2.0f * static_cast<f32>(x) * inverse - 1.0f;
        const f32 y0 = 2.0f * static_cast<f32>(y) * inverse - 1.0f;
        const f32 x1 = x0 + 2.0f * inverse;
        const f32 y1 = y0 + 2.0f * inverse;
        return area_element(x0, y0) - area_element(x0, y1) - area_element(x1, y0) + area_element(x1, y1);
    }

    [[nodiscard]] f32 radical_inverse(u32 bits) noexcept {
        bits = (bits << 16u) | (bits >> 16u);
        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
        return static_cast<f32>(bits) * 2.3283064365386963e-10f;
    }

    [[nodiscard]] f32 ggx_distribution(f32 nDotH, f32 alpha) noexcept {
        const f32 alpha2 = alpha * alpha;
        const f32 denominator = nDotH * nDotH * (alpha2 - 1.0f) + 1.0f;
        return alpha2 / (math::PI * denominator * denominator);
    }

    [[nodiscard]] math::vec3 importance_sample_ggx(const math::vec2 &xi, f32 alpha, const math::vec3 &normal) noexcept {
        const f32 phi = math::TWO_PI * xi.x;
        const f32 cosTheta = std::sqrt((1.0f - xi.y) / (1.0f + (alpha * alpha - 1.0f) * xi.y));
        const f32 sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
        const math::vec3 halfTangent(sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta);
        const math::vec3 up = math::absolute(
          normal.z
        ) < 0.999f ? math::vec3(0.0f, 0.0f, 1.0f) : math::vec3(1.0f, 0.0f, 0.0f);
        const math::vec3 tangentX = glm::normalize(glm::cross(up, normal));
        const math::vec3 tangentY = glm::cross(normal, tangentX);
        return glm::normalize(tangentX * halfTangent.x + tangentY * halfTangent.y + normal * halfTangent.z);
    }

    [[nodiscard]] math::vec3 bilinear_face(const cubemap_f32 &cubemap, u32 mip, u32 face, f32 s, f32 t) noexcept {
        const u32 size = cubemap.mip_size(mip);
        const f32 fx = math::clamp(s * static_cast<f32>(size) - 0.5f, 0.0f, static_cast<f32>(size - 1));
        const f32 fy = math::clamp(t * static_cast<f32>(size) - 0.5f, 0.0f, static_cast<f32>(size - 1));
        const u32 x0 = static_cast<u32>(fx);
        const u32 y0 = static_cast<u32>(fy);
        const u32 x1 = std::min(x0 + 1, size - 1);
        const u32 y1 = std::min(y0 + 1, size - 1);
        const f32 wx = fx - static_cast<f32>(x0);
        const f32 wy = fy - static_cast<f32>(y0);
        const math::vec3 top = cubemap.at(mip, face, x0, y0) * (1.0f - wx) + cubemap.at(mip, face, x1, y0) * wx;
        const math::vec3 bottom = cubemap.at(mip, face, x0, y1) * (1.0f - wx) + cubemap.at(mip, face, x1, y1) * wx;
        return top * (1.0f - wy) + bottom * wy;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// cubemap_f32

u32 cubemap_f32::mip_size(u32 mip) const noexcept {
    return std::max<u32>(1, size >> mip);
}

usize cubemap_f32::face_offset(u32 mip, u32 face) const noexcept {
    usize offset = 0;
    for (u32 level = 0; level < mip; ++level) {
        const usize edge = mip_size(level);
        offset += edge * edge * CUBEMAP_FACE_COUNT;
    }
    const usize edge = mip_size(mip);
    return offset + edge * edge * face;
}

const math::vec3 &cubemap_f32::at(u32 mip, u32 face, u32 x, u32 y) const noexcept {
    return texels[face_offset(mip, face) + static_cast<usize>(y) * mip_size(mip) + x];
}

math::vec3 &cubemap_f32::at(u32 mip, u32 face, u32 x, u32 y) noexcept {
    return texels[face_offset(mip, face) + static_cast<usize>(y) * mip_size(mip) + x];
}

cubemap_f32 cubemap_f32::allocate(u32 edge, u32 mips) {
    cubemap_f32 result = {};
    result.size = edge;
    result.mipCount = mips;
    usize total = 0;
    for (u32 level = 0; level < mips; ++level) {
        const usize levelEdge = result.mip_size(level);
        total += levelEdge * levelEdge * CUBEMAP_FACE_COUNT;
    }
    result.texels.assign(total, math::vec3(0.0f));
    return result;
}

/*===========================================================================*/
/*===========================================================================*/
// Lookups

void conjure::renderer::detail::direction_to_face(const math::vec3 &direction, u32 &face, f32 &s, f32 &t) noexcept {
    const math::vec3 absolute = glm::abs(direction);
    f32 u = 0.0f;
    f32 v = 0.0f;
    f32 major = 1.0f;
    if (absolute.x >= absolute.y && absolute.x >= absolute.z) {
        major = absolute.x;
        if (direction.x > 0.0f) {
            face = 0;
            u = -direction.z;
        } else {
            face = 1;
            u = direction.z;
        }
        v = -direction.y;
    } else if (absolute.y >= absolute.z) {
        major = absolute.y;
        u = direction.x;
        if (direction.y > 0.0f) {
            face = 2;
            v = direction.z;
        } else {
            face = 3;
            v = -direction.z;
        }
    } else {
        major = absolute.z;
        if (direction.z > 0.0f) {
            face = 4;
            u = direction.x;
        } else {
            face = 5;
            u = -direction.x;
        }
        v = -direction.y;
    }
    if (major <= 0.0f) {
        face = 4;
        s = 0.5f;
        t = 0.5f;
        return;
    }
    s = (u / major + 1.0f) * 0.5f;
    t = (v / major + 1.0f) * 0.5f;
}

math::vec3 conjure::renderer::detail::sample_cubemap(
  const cubemap_f32 &cubemap,
  u32 mip,
  const math::vec3 &direction
) noexcept {
    u32 face = 0;
    f32 s = 0.0f;
    f32 t = 0.0f;
    direction_to_face(direction, face, s, t);
    return bilinear_face(cubemap, std::min(mip, cubemap.mipCount - 1), face, s, t);
}

math::vec3 conjure::renderer::detail::sample_cubemap_lod(
  const cubemap_f32 &cubemap,
  f32 lod,
  const math::vec3 &direction
) noexcept {
    const f32 maxLod = static_cast<f32>(cubemap.mipCount - 1);
    const f32 clamped = math::clamp(lod, 0.0f, maxLod);
    const u32 lower = static_cast<u32>(clamped);
    const u32 upper = std::min(lower + 1, cubemap.mipCount - 1);
    const f32 weight = clamped - static_cast<f32>(lower);
    const math::vec3 low = sample_cubemap(cubemap, lower, direction);
    if (weight <= 0.0f || upper == lower) {
        return low;
    }
    const math::vec3 high = sample_cubemap(cubemap, upper, direction);
    return low * (1.0f - weight) + high * weight;
}

math::vec2 conjure::renderer::detail::hammersley(u32 index, u32 count) noexcept {
    return math::vec2(static_cast<f32>(index) / static_cast<f32>(std::max<u32>(count, 1)), radical_inverse(index));
}

u32 conjure::renderer::detail::environment_mip_count(u32 size) noexcept {
    return std::min<u32>(ENVIRONMENT_MIP_COUNT, static_cast<u32>(std::bit_width(std::max<u32>(size, 1))));
}

/*===========================================================================*/
/*===========================================================================*/
// Bake steps

cubemap_f32 conjure::renderer::detail::equirect_to_cubemap(
  const equirect_image &source,
  u32 size,
  u32 mipCount,
  core::jobs::job_system *jobs
) {
    cubemap_f32 result = cubemap_f32::allocate(size, mipCount);
    for_each_face(jobs, [&result, &source, size](u32 face) {
        for (u32 y = 0; y < size; ++y) {
            for (u32 x = 0; x < size; ++x) {
                result.at(0, face, x, y) = source.sample(cubemap_texel_direction(face, x, y, size));
            }
        }
    });
    return result;
}

void conjure::renderer::detail::build_mip_chain(cubemap_f32 &cubemap, core::jobs::job_system *jobs) {
    for (u32 mip = 1; mip < cubemap.mipCount; ++mip) {
        const u32 previousSize = cubemap.mip_size(mip - 1);
        const u32 currentSize = cubemap.mip_size(mip);
        for_each_face(jobs, [&cubemap, mip, previousSize, currentSize](u32 face) {
            for (u32 y = 0; y < currentSize; ++y) {
                for (u32 x = 0; x < currentSize; ++x) {
                    const u32 x0 = std::min(x * 2, previousSize - 1);
                    const u32 y0 = std::min(y * 2, previousSize - 1);
                    const u32 x1 = std::min(x * 2 + 1, previousSize - 1);
                    const u32 y1 = std::min(y * 2 + 1, previousSize - 1);
                    const math::vec3 sum = cubemap.at(mip - 1, face, x0, y0) + cubemap.at(mip - 1, face, x1, y0) +
                      cubemap.at(mip - 1, face, x0, y1) + cubemap.at(mip - 1, face, x1, y1);
                    cubemap.at(mip, face, x, y) = sum * 0.25f;
                }
            }
        });
    }
}

sh9 conjure::renderer::detail::project_sh9(const cubemap_f32 &cubemap, u32 mip) {
    sh9 coefficients = {};
    coefficients.fill(math::vec3(0.0f));
    const u32 level = std::min(mip, cubemap.mipCount - 1);
    const u32 size = cubemap.mip_size(level);
    f32 weightSum = 0.0f;
    for (u32 face = 0; face < CUBEMAP_FACE_COUNT; ++face) {
        for (u32 y = 0; y < size; ++y) {
            for (u32 x = 0; x < size; ++x) {
                const math::vec3 direction = cubemap_texel_direction(face, x, y, size);
                const f32 weight = texel_solid_angle(x, y, size);
                const math::vec3 radiance = cubemap.at(level, face, x, y) * weight;
                weightSum += weight;
                coefficients[0] += radiance * SH_Y00;
                coefficients[1] += radiance * (SH_Y1 * direction.y);
                coefficients[2] += radiance * (SH_Y1 * direction.z);
                coefficients[3] += radiance * (SH_Y1 * direction.x);
                coefficients[4] += radiance * (SH_Y2_XY * direction.x * direction.y);
                coefficients[5] += radiance * (SH_Y2_XY * direction.y * direction.z);
                coefficients[6] += radiance * (SH_Y20 * (3.0f * direction.z * direction.z - 1.0f));
                coefficients[7] += radiance * (SH_Y2_XY * direction.x * direction.z);
                coefficients[8] += radiance * (SH_Y22 * (direction.x * direction.x - direction.y * direction.y));
            }
        }
    }
    // The texel solid angles sum to 4 pi up to rounding; normalise so the projection is exact for a
    // constant environment.
    const f32 normalisation = weightSum > 0.0f ? (4.0f * math::PI) / weightSum : 1.0f;
    for (math::vec3 &coefficient : coefficients) {
        coefficient *= normalisation;
    }
    return coefficients;
}

cubemap_f32 conjure::renderer::detail::prefilter_ggx(
  const cubemap_f32 &sky,
  u32 size,
  u32 mipCount,
  u32 sampleCount,
  core::jobs::job_system *jobs
) {
    cubemap_f32 result = cubemap_f32::allocate(size, mipCount);
    const f32 skyTexelSolidAngle = (4.0f * math::PI) / (6.0f * static_cast<f32>(sky.size) * static_cast<f32>(sky.size));
    const f32 skyLodBias = std::log2(static_cast<f32>(sky.size) / static_cast<f32>(size));

    for (u32 mip = 0; mip < mipCount; ++mip) {
        const f32 roughness = mipCount > 1 ? static_cast<f32>(mip) / static_cast<f32>(mipCount - 1) : 0.0f;
        const f32 alpha = std::max(roughness * roughness, 1.0e-4f);
        const u32 mipSize = result.mip_size(mip);
        for_each_face(jobs, [&](u32 face) {
            for (u32 y = 0; y < mipSize; ++y) {
                for (u32 x = 0; x < mipSize; ++x) {
                    const math::vec3 normal = cubemap_texel_direction(face, x, y, mipSize);
                    if (mip == 0) {
                        result.at(mip, face, x, y) = sample_cubemap_lod(sky, skyLodBias, normal);
                        continue;
                    }
                    math::vec3 sum(0.0f);
                    f32 weight = 0.0f;
                    for (u32 sample = 0; sample < sampleCount; ++sample) {
                        const math::vec2 xi = hammersley(sample, sampleCount);
                        const math::vec3 halfVector = importance_sample_ggx(xi, alpha, normal);
                        const f32 vDotH = glm::dot(normal, halfVector);
                        const math::vec3 light = halfVector * (2.0f * vDotH) - normal;
                        const f32 nDotL = glm::dot(normal, light);
                        if (nDotL <= 0.0f) {
                            continue;
                        }
                        // With V = N the half vector angle equals the view angle, so pdf = D / 4.
                        const f32 pdf = ggx_distribution(std::max(vDotH, 0.0f), alpha) * 0.25f;
                        const f32 sampleSolidAngle = 1.0f / (static_cast<f32>(sampleCount) * pdf + 1.0e-4f);
                        const f32 lod = 0.5f * std::log2(sampleSolidAngle / skyTexelSolidAngle) + 1.0f;
                        sum += sample_cubemap_lod(sky, lod, light) * nDotL;
                        weight += nDotL;
                    }
                    result.at(mip, face, x, y) = weight > 0.0f ? sum / weight : sample_cubemap(sky, 0, normal);
                }
            }
        });
    }
    return result;
}

cubemap_data conjure::renderer::detail::pack_cubemap(const cubemap_f32 &cubemap) {
    cubemap_data packed = {};
    packed.size = cubemap.size;
    packed.mipCount = cubemap.mipCount;
    packed.texels.resize(cubemap.texels.size() * 4);
    const u16 one = pack_half(1.0f);
    for (usize index = 0; index < cubemap.texels.size(); ++index) {
        const math::vec3 &texel = cubemap.texels[index];
        packed.texels[index * 4] = pack_half(texel.x);
        packed.texels[index * 4 + 1] = pack_half(texel.y);
        packed.texels[index * 4 + 2] = pack_half(texel.z);
        packed.texels[index * 4 + 3] = one;
    }
    return packed;
}
