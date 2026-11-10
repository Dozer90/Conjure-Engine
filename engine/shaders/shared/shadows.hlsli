// Shadow filtering (docs/design/02_renderer.md R9, 3.4): HARD (one comparison tap), PCF (16-tap
// Poisson disk rotated by interleaved gradient noise) and PCSS (16-tap blocker search, penumbra
// estimate, 16-tap PCF with the estimated radius) for the directional cascades and the local-light
// atlas alike. Every shadow map is reverse-Z (near = 1). NVRHI's comparison samplers compare with
// Less on every backend, which under reverse-Z answers 1 when the receiver is farther from the light
// than the stored occluder, i.e. shadowed: shadow_tap() inverts that so a receiver passes when it is
// at least as close as the occluder. The receiver is pushed toward the light along its normal by
// normalOffsetTexels texels.
//
// PCSS penumbrae are estimated in world units and converted to texels:
//   cascades (orthographic sun of angular radius A): penumbra = (receiver - blocker distance) * A,
//     depths linear in the light volume (depth range = 1 / |z row of clipFromWorld|);
//   atlas tiles (perspective lights of source radius R): penumbra = (dReceiver - dBlocker) * R /
//     dBlocker with the reverse-Z depths linearised from the tile's near plane and the light's range.
// Both search and filter radii are clamped to CONJURE_PCSS_MAX_RADIUS_TEXELS.

#ifndef CONJURE_SHADOWS_HLSLI
#define CONJURE_SHADOWS_HLSLI

#include "gpu_types.hlsli"
#include "noise.hlsli"

static const float2 CONJURE_POISSON_DISK[16] = {
    float2(-0.94201624, -0.39906216), float2(0.94558609, -0.76890725), float2(-0.09418410, -0.92938870),
    float2(0.34495938, 0.29387760),   float2(-0.91588581, 0.45771432), float2(-0.81544232, -0.87912464),
    float2(-0.38277543, 0.27676845),  float2(0.97484398, 0.75648379),  float2(0.44323325, -0.97511554),
    float2(0.53742981, -0.47373420),  float2(-0.26496911, -0.41893023), float2(0.79197514, 0.19090188),
    float2(-0.24188840, 0.99706507),  float2(-0.81409955, 0.91437590), float2(0.19984126, 0.78641367),
    float2(0.14383161, -0.14100790)
};

#define CONJURE_PCSS_MAX_RADIUS_TEXELS 8.0
#define CONJURE_PCSS_BLOCKER_EPSILON 1e-4

// One comparison tap as a lit factor (1 = lit): the Less comparison answers 1 when shadowed.
float shadow_tap_array(Texture2DArray<float> cascades, SamplerComparisonState comparison, float3 location, float depth) {
    return 1.0 - cascades.SampleCmpLevelZero(comparison, location, depth);
}

float shadow_tap(Texture2D<float> atlas, SamplerComparisonState comparison, float2 location, float depth) {
    return 1.0 - atlas.SampleCmpLevelZero(comparison, location, depth);
}

float2 rotate_tap(float2 tap, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return float2(tap.x * c - tap.y * s, tap.x * s + tap.y * c);
}

// Light-space position (uv in [0, 1], reverse-Z depth) of a receiver pushed toward the light along
// its normal by normalOffsetTexels texels of texelWorldSize each.
float3 shadow_project(float4x4 clipFromWorld, float3 worldPosition, float3 worldNormal, float texelWorldSize, float normalOffsetTexels) {
    float3 offsetPosition = worldPosition + worldNormal * (texelWorldSize * normalOffsetTexels);
    float4 clip = mul(float4(offsetPosition, 1.0), clipFromWorld);
    float3 ndc = clip.xyz / max(clip.w, 1e-6);
    float2 uv = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
    return float3(uv, ndc.z);
}

bool inside_unit_square(float2 uv) {
    return uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0;
}

// Distance from a perspective light of a reverse-Z depth (near maps to 1, far to 0).
float linearise_local_depth(float reverseZ, float nearPlane, float farPlane) {
    return nearPlane * farPlane / max(reverseZ * (farPlane - nearPlane) + nearPlane, 1e-6);
}

// World extent of an orthographic light volume along its depth axis: the z row of the reverse-Z
// clip matrix scales by 1 / (far - near). HLSL reads the glm memory transposed (-Zpr), so the glm
// row is the column [i][2].
float orthographic_depth_range(float4x4 clipFromWorld) {
    return 1.0 / max(length(float3(clipFromWorld[0][2], clipFromWorld[1][2], clipFromWorld[2][2])), 1e-6);
}

// Cascaded directional shadows: slice 'cascade' of the array texture, uv in [0, 1] covers the slice.
// texelWorld is the world size of one cascade texel, depthRangeWorld the volume's depth extent and
// sourceRadius the sun's angular radius (PCSS only).
float sample_cascade_filtered(
  Texture2DArray<float> cascades,
  SamplerComparisonState comparison,
  SamplerState pointSampler,
  float cascade,
  float3 lightSpace,
  float2 pixel,
  float texelSize,
  float texelWorld,
  float depthRangeWorld,
  float sourceRadius,
  float pcfRadiusTexels,
  uint filterMode
) {
    if (filterMode == CONJURE_SHADOW_FILTER_HARD) {
        return shadow_tap_array(cascades, comparison, float3(lightSpace.xy, cascade), lightSpace.z);
    }
    float angle = interleaved_gradient_noise(pixel) * 2.0 * CONJURE_PI;
    float radiusTexels = pcfRadiusTexels;
    if (filterMode == CONJURE_SHADOW_FILTER_PCSS) {
        // Blockers lie between the volume's near plane (depth 1) and the receiver: the search cone
        // of the sun's angular radius covers that distance.
        float receiverDistance = saturate(1.0 - lightSpace.z) * depthRangeWorld;
        float searchTexels = clamp(sourceRadius * receiverDistance / max(texelWorld, 1e-6), 1.0, CONJURE_PCSS_MAX_RADIUS_TEXELS);
        float blockerSum = 0.0;
        float blockerCount = 0.0;
        [unroll]
        for (int search = 0; search < 16; ++search) {
            float2 offset = rotate_tap(CONJURE_POISSON_DISK[search], angle) * searchTexels * texelSize;
            float stored = cascades.SampleLevel(pointSampler, float3(lightSpace.xy + offset, cascade), 0.0);
            if (stored > lightSpace.z + CONJURE_PCSS_BLOCKER_EPSILON) {
                blockerSum += stored;
                blockerCount += 1.0;
            }
        }
        if (blockerCount <= 0.0) {
            return 1.0;
        }
        float blockerDistance = (blockerSum / blockerCount - lightSpace.z) * depthRangeWorld;
        float penumbraWorld = blockerDistance * sourceRadius;
        radiusTexels = clamp(penumbraWorld / max(texelWorld, 1e-6), 1.0, CONJURE_PCSS_MAX_RADIUS_TEXELS);
    }
    float radius = radiusTexels * texelSize;
    float sum = 0.0;
    [unroll]
    for (int tap = 0; tap < 16; ++tap) {
        float2 offset = rotate_tap(CONJURE_POISSON_DISK[tap], angle) * radius;
        sum += shadow_tap_array(cascades, comparison, float3(lightSpace.xy + offset, cascade), lightSpace.z);
    }
    return sum / 16.0;
}

// Atlas tiles: tile uv is remapped into the tile rectangle and clamped so a tile never reads its
// neighbour.
float2 atlas_uv(float4 rect, float2 tileUv) {
    float2 size = rect.zw - rect.xy;
    return rect.xy + clamp(tileUv, 0.0, 1.0) * size;
}

// Local-light tiles (perspective, near plane CONJURE_LOCAL_SHADOW_NEAR_PLANE, far plane = the
// light's range). tile.params: x texel size in tile uv, y source radius in world units, w world
// texel size per unit distance from the light.
float sample_atlas_filtered(
  Texture2D<float> atlas,
  SamplerComparisonState comparison,
  SamplerState pointSampler,
  gpu_shadow_tile tile,
  float3 lightSpace,
  float2 pixel,
  float farPlane,
  float pcfRadiusTexels,
  uint filterMode
) {
    float texelSize = tile.params.x;
    float2 tileTexel = float2(texelSize, texelSize);
    if (filterMode == CONJURE_SHADOW_FILTER_HARD) {
        return shadow_tap(atlas, comparison, atlas_uv(tile.atlasRect, lightSpace.xy), lightSpace.z);
    }

    float angle = interleaved_gradient_noise(pixel) * 2.0 * CONJURE_PI;
    float radiusTexels = pcfRadiusTexels;
    if (filterMode == CONJURE_SHADOW_FILTER_PCSS) {
        float nearPlane = CONJURE_LOCAL_SHADOW_NEAR_PLANE;
        float receiverDistance = linearise_local_depth(lightSpace.z, nearPlane, farPlane);
        float sourceRadius = tile.params.y;
        float texelPerDistance = max(tile.params.w, 1e-6);
        // The light disc seen from the receiver spans sourceRadius / receiverDistance radians; a
        // texel spans texelPerDistance radians.
        float searchTexels = clamp(sourceRadius / (receiverDistance * texelPerDistance), 1.0, CONJURE_PCSS_MAX_RADIUS_TEXELS);
        float blockerSum = 0.0;
        float blockerCount = 0.0;
        [unroll]
        for (int search = 0; search < 16; ++search) {
            float2 offset = rotate_tap(CONJURE_POISSON_DISK[search], angle) * searchTexels * tileTexel;
            float stored = atlas.SampleLevel(pointSampler, atlas_uv(tile.atlasRect, lightSpace.xy + offset), 0.0);
            if (stored > lightSpace.z + CONJURE_PCSS_BLOCKER_EPSILON) {
                blockerSum += stored;
                blockerCount += 1.0;
            }
        }
        if (blockerCount <= 0.0) {
            return 1.0;
        }
        float blockerDistance = linearise_local_depth(blockerSum / blockerCount, nearPlane, farPlane);
        // Design 3.4: penumbra = (dReceiver - dBlocker) * sourceRadius / dBlocker, in world units at
        // the receiver, where one texel covers texelPerDistance * dReceiver.
        float penumbraWorld = max(receiverDistance - blockerDistance, 0.0) * sourceRadius / max(blockerDistance, nearPlane);
        radiusTexels = clamp(penumbraWorld / (texelPerDistance * receiverDistance), 1.0, CONJURE_PCSS_MAX_RADIUS_TEXELS);
    }
    float sum = 0.0;
    [unroll]
    for (int tap = 0; tap < 16; ++tap) {
        float2 offset = rotate_tap(CONJURE_POISSON_DISK[tap], angle) * radiusTexels * tileTexel;
        sum += shadow_tap(atlas, comparison, atlas_uv(tile.atlasRect, lightSpace.xy + offset), lightSpace.z);
    }
    return sum / 16.0;
}

#endif // CONJURE_SHADOWS_HLSLI
