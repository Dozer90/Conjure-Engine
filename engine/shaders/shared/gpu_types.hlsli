// GPU-side structures shared by every pass (docs/design/02_renderer.md 2.5). engine/src/renderer/gpu_types.h
// mirrors each struct and static_asserts its size; tests/renderer/test_gpu_types.cpp reads the #defines
// below and compares them with the C++ constants, so the two files must change together.
//
// Matrices come from glm untouched (column-major memory). The shaders are compiled with -Zpr, which
// makes HLSL read that memory as the transposed matrix, so every transform here is written as
// mul(vector, matrix): v * glm^T equals glm * v. Instance matrices are stored as explicit rows and
// reconstructed with dot products so no convention is involved there.

#ifndef CONJURE_GPU_TYPES_HLSLI
#define CONJURE_GPU_TYPES_HLSLI

#include "common.hlsli"

// Limits (conjure/renderer/limits.h).
#define CONJURE_MAX_VIEWS 8
#define CONJURE_MAX_LIGHTS_PER_VIEW 1024
#define CONJURE_MAX_LIGHTS_PER_TILE 64
#define CONJURE_LIGHT_TILE_SIZE 16
#define CONJURE_MAX_SHADOW_CASCADES 4
#define CONJURE_MAX_SPRITES_PER_FRAME 65536
#define CONJURE_MAX_DEBUG_VERTICES 1048576
#define CONJURE_MAX_MATERIAL_SLOTS 8
#define CONJURE_INVALID_INDEX 0xFFFFFFFF
#define CONJURE_EXPOSURE_HISTOGRAM_BINS 256
#define CONJURE_BLOOM_MIP_COUNT 6

// Struct sizes in bytes (gpu_types.h).
#define CONJURE_GPU_VIEW_CONSTANTS_SIZE 1024
#define CONJURE_GPU_INSTANCE_SIZE 96
#define CONJURE_GPU_LIGHT_SIZE 64
#define CONJURE_GPU_SHADOW_TILE_SIZE 96
#define CONJURE_GPU_MATERIAL_CONSTANTS_SIZE 64
#define CONJURE_GPU_DRAW_CONSTANTS_SIZE 16
#define CONJURE_GPU_SPRITE_VERTEX_SIZE 32
#define CONJURE_GPU_DEBUG_VERTEX_SIZE 16
#define CONJURE_GPU_EXPOSURE_STATE_SIZE 16

// One tile holds two light lists (A: depth bounded, B: unbounded), each a count followed by indices.
#define CONJURE_TILE_LIST_STRIDE (2 * (CONJURE_MAX_LIGHTS_PER_TILE + 1))
#define CONJURE_TILE_LIST_B_OFFSET (CONJURE_MAX_LIGHTS_PER_TILE + 1)

// Enumerators (light.h, material_asset.h, render_settings.h).
#define CONJURE_LIGHT_DIRECTIONAL 0
#define CONJURE_LIGHT_POINT 1
#define CONJURE_LIGHT_SPOT 2
#define CONJURE_SHADOW_FILTER_HARD 1
#define CONJURE_SHADOW_FILTER_PCF 2
#define CONJURE_SHADOW_FILTER_PCSS 3
#define CONJURE_TONEMAP_ACES_FITTED 0
#define CONJURE_TONEMAP_REINHARD_EXTENDED 1
#define CONJURE_TONEMAP_AGX 2
#define CONJURE_TONEMAP_NONE 3

// gpu_view_constants::flags bits.
#define CONJURE_VIEW_FLAG_NO_POST_PROCESS 1
#define CONJURE_VIEW_FLAG_SHADOWS 2
#define CONJURE_VIEW_FLAG_SSAO 4
#define CONJURE_VIEW_FLAG_ENVIRONMENT 8
#define CONJURE_VIEW_FLAG_AUTO_EXPOSURE 16
#define CONJURE_VIEW_FLAG_BLOOM 32

// gpu_material_constants::flags bits.
#define CONJURE_MATERIAL_DOUBLE_SIDED 1
#define CONJURE_MATERIAL_UNLIT 2

// gpu_instance flags bits.
#define CONJURE_INSTANCE_RECEIVES_SHADOWS 1

// Near plane of the local-light shadow projections (gpu_types.h LOCAL_SHADOW_NEAR_PLANE); the far
// plane is the light's range. Reverse-Z atlas depths are linearised from the pair.
#define CONJURE_LOCAL_SHADOW_NEAR_PLANE 0.05

// Draw constants flags bits of the forward and sprite passes.
#define CONJURE_DRAW_FLAG_LIGHT_LIST_B 1
#define CONJURE_DRAW_FLAG_SCREEN_SPACE 2
#define CONJURE_DRAW_FLAG_POINT_SAMPLER 4
#define CONJURE_DRAW_FLAG_KARIS_AVERAGE 8

struct gpu_view_constants {
    float4x4 viewFromWorld;             // 0
    float4x4 clipFromView;              // 64
    float4x4 clipFromWorld;             // 128
    float4x4 worldFromClip;             // 192
    float4x4 viewFromClip;              // 256
    float4 cameraPosition;              // 320: xyz camera, w unused
    float4 viewportSize;                // 336: width, height, 1 / width, 1 / height
    float4 nearFarExposure;             // 352: near, far, manual exposure, delta seconds
    uint4 counts;                       // 368: tileCountX, tileCountY, lightCount, directionalLightCount
    uint4 flags;                        // 384: view flags, cascadeCount, frameIndex, shadow light index
    float4 ssaoParams;                  // 400: radius, intensity, thickness, unused
    float4 environmentParams;           // 416: ibl intensity, sky rotation, prefiltered mip count - 1, unused
    float4 shadowParams;                // 432: cascade texel size, pcf radius texels, normal offset texels, atlas texel size
    float4 postProcess;                 // 448: bloom intensity, operator, white point, agx look
    float4 cascadeSplits;               // 464: far view depth of each cascade
    float4x4 cascadeClipFromWorld[4];   // 480
    float4 shCoefficients[7];           // 736: 27 irradiance coefficients, xyz per entry (9 vec3 packed)
    float4 exposureParams;              // 848: compensation EV, speed up, speed down, min EV
    float4 exposureParams2;             // 864: max EV, unused x3
    float4 shadowParams2;               // 880: directional light source radius (radians, PCSS), unused x3
    float4 reserved[8];                 // 896 .. 1024
};

cbuffer view_constants : register(b0, space0) {
    gpu_view_constants gView;
};

CONJURE_PUSH_CONSTANTS cbuffer draw_constants : register(b1, space0) {
    uint gDrawFirstInstance;
    uint gDrawFlags;
    float gDrawParam0;
    float gDrawParam1;
};

struct gpu_instance {
    float4 world0;                      // rows of the 3x4 world-from-local matrix
    float4 world1;
    float4 world2;
    float4 normal0;                     // rows of the 3x3 normal matrix; .w = entity id bits
    float4 normal1;                     // .w = flags bits
    float4 normal2;                     // .w unused
};

struct gpu_light {
    float4 positionRange;               // xyz position, w range
    float4 directionCosOuter;           // xyz direction (pointing from the light), w cos(outer cone)
    float4 colourIntensity;             // rgb linear colour, w intensity
    float4 params;                      // x cos(inner cone), y type bits, z shadow tile bits (INVALID = none), w filter bits
};

struct gpu_shadow_tile {
    float4x4 clipFromWorld;             // the light's reverse-Z projection
    float4 atlasRect;                   // u0, v0, u1, v1 inside the atlas (cascades: slice index in x, 0..1 otherwise)
    float4 params;                      // x texel size in uv, y source radius (world units; radians for cascades),
                                        // z filter bits, w world texel size per unit distance (perspective tiles)
};

struct gpu_material_constants {
    float4 baseColourFactor;
    float4 emissive;                    // rgb factor, w strength
    float4 params;                      // metallic, roughness, normal scale, occlusion strength
    uint4 flags;                        // texture flags, material flags, uv set bits, alpha cutoff bits
};

struct gpu_sprite_vertex {
    float3 position;
    float2 uv;
    uint colour;
    uint2 pad;
};

struct gpu_debug_vertex {
    float3 position;
    uint colour;
};

struct gpu_exposure_state {
    float exposure;
    float ev100;
    float lastTargetEv100;
    float pad;
};

float3 transform_point(gpu_instance instance, float3 position) {
    float4 p = float4(position, 1.0);
    return float3(dot(instance.world0, p), dot(instance.world1, p), dot(instance.world2, p));
}

float3 transform_normal(gpu_instance instance, float3 normal) {
    return float3(dot(instance.normal0.xyz, normal), dot(instance.normal1.xyz, normal), dot(instance.normal2.xyz, normal));
}

// Surface directions such as tangents follow the world matrix; only normals use transform_normal.
float3 transform_direction(gpu_instance instance, float3 direction) {
    return float3(
      dot(instance.world0.xyz, direction),
      dot(instance.world1.xyz, direction),
      dot(instance.world2.xyz, direction)
    );
}

float4 unpack_rgba8(uint packed) {
    return float4(packed & 0xFF, (packed >> 8) & 0xFF, (packed >> 16) & 0xFF, (packed >> 24) & 0xFF) / 255.0;
}

#endif // CONJURE_GPU_TYPES_HLSLI
