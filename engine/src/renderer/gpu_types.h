#pragma once

#include "conjure/renderer/limits.h"

#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <array>
#include <cstddef>
#include <type_traits>

// C++ mirrors of engine/shaders/shared/gpu_types.hlsli (docs/design/02_renderer.md 2.5). Every struct
// is static_asserted against the byte size the HLSL header #defines, and tests/renderer/test_gpu_types.cpp
// re-reads the HLSL defines to prove both files agree. Matrices are glm column-major and uploaded
// verbatim; the shaders use mul(vector, matrix) under -Zpr (see the HLSL header).

namespace conjure::renderer::gpu {

    inline constexpr u32 VIEW_CONSTANTS_SIZE = 1024;
    inline constexpr u32 INSTANCE_SIZE = 96;
    inline constexpr u32 LIGHT_SIZE = 64;
    inline constexpr u32 SHADOW_TILE_SIZE = 96;
    inline constexpr u32 MATERIAL_CONSTANTS_SIZE = 64;
    inline constexpr u32 DRAW_CONSTANTS_SIZE = 16;
    inline constexpr u32 SPRITE_VERTEX_SIZE = 32;
    inline constexpr u32 DEBUG_VERTEX_SIZE = 16;
    inline constexpr u32 EXPOSURE_STATE_SIZE = 16;
    inline constexpr u32 TILE_LIST_STRIDE = 2 * (MAX_LIGHTS_PER_TILE + 1);
    inline constexpr u32 TILE_LIST_B_OFFSET = MAX_LIGHTS_PER_TILE + 1;

    inline constexpr u32 VIEW_FLAG_NO_POST_PROCESS = 1;
    inline constexpr u32 VIEW_FLAG_SHADOWS = 2;
    inline constexpr u32 VIEW_FLAG_SSAO = 4;
    inline constexpr u32 VIEW_FLAG_ENVIRONMENT = 8;
    inline constexpr u32 VIEW_FLAG_AUTO_EXPOSURE = 16;
    inline constexpr u32 VIEW_FLAG_BLOOM = 32;
    inline constexpr u32 VIEW_FLAG_DEBUG_AO = 64;              // render_view::debugMode AMBIENT_OCCLUSION

    inline constexpr u32 MATERIAL_FLAG_DOUBLE_SIDED = 1;
    inline constexpr u32 MATERIAL_FLAG_UNLIT = 2;
    inline constexpr u32 INSTANCE_FLAG_RECEIVES_SHADOWS = 1;

    inline constexpr u32 DRAW_FLAG_LIGHT_LIST_B = 1;
    inline constexpr u32 DRAW_FLAG_SCREEN_SPACE = 2;
    inline constexpr u32 DRAW_FLAG_POINT_SAMPLER = 4;
    inline constexpr u32 DRAW_FLAG_KARIS_AVERAGE = 8;

    /// <summary>
    /// Near plane of every local-light shadow projection (CONJURE_LOCAL_SHADOW_NEAR_PLANE in the HLSL
    /// header): the shaders linearise the reverse-Z atlas depths with it for the PCSS penumbra.
    /// </summary>
    inline constexpr f32 LOCAL_SHADOW_NEAR_PLANE = 0.05f;

    struct view_constants {
        math::mat4 viewFromWorld = math::mat4(1.0f);
        math::mat4 clipFromView = math::mat4(1.0f);
        math::mat4 clipFromWorld = math::mat4(1.0f);
        math::mat4 worldFromClip = math::mat4(1.0f);
        math::mat4 viewFromClip = math::mat4(1.0f);
        math::vec4 cameraPosition = math::vec4(0.0f);
        math::vec4 viewportSize = math::vec4(0.0f);
        math::vec4 nearFarExposure = math::vec4(0.0f);
        math::uvec4 counts = math::uvec4(0);
        math::uvec4 flags = math::uvec4(0);
        math::vec4 ssaoParams = math::vec4(0.0f);
        math::vec4 environmentParams = math::vec4(0.0f);
        math::vec4 shadowParams = math::vec4(0.0f);
        math::vec4 postProcess = math::vec4(0.0f);
        math::vec4 cascadeSplits = math::vec4(0.0f);
        std::array<math::mat4, MAX_SHADOW_CASCADES> cascadeClipFromWorld = {};
        std::array<math::vec4, 7> shCoefficients = {};
        math::vec4 exposureParams = math::vec4(0.0f);
        math::vec4 exposureParams2 = math::vec4(0.0f);
        math::vec4 shadowParams2 = math::vec4(0.0f);    // directional source radius (radians, PCSS), unused x3
        std::array<math::vec4, 8> reserved = {};
    };

    struct instance {
        math::vec4 world0 = math::vec4(0.0f);
        math::vec4 world1 = math::vec4(0.0f);
        math::vec4 world2 = math::vec4(0.0f);
        math::vec4 normal0 = math::vec4(0.0f);
        math::vec4 normal1 = math::vec4(0.0f);
        math::vec4 normal2 = math::vec4(0.0f);
    };

    struct light {
        math::vec4 positionRange = math::vec4(0.0f);
        math::vec4 directionCosOuter = math::vec4(0.0f);
        math::vec4 colourIntensity = math::vec4(0.0f);
        math::vec4 params = math::vec4(0.0f);
    };

    struct shadow_tile {
        math::mat4 clipFromWorld = math::mat4(1.0f);
        math::vec4 atlasRect = math::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        math::vec4 params = math::vec4(0.0f);
    };

    struct material_constants {
        math::vec4 baseColourFactor = math::vec4(1.0f);
        math::vec4 emissive = math::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        math::vec4 params = math::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        math::uvec4 flags = math::uvec4(0);
    };

    struct draw_constants {
        u32 firstInstance = 0;
        u32 flags = 0;
        f32 param0 = 0.0f;
        f32 param1 = 0.0f;
    };

    struct exposure_state {
        f32 exposure = 1.0f;
        f32 ev100 = 0.0f;
        f32 lastTargetEv100 = 0.0f;
        f32 pad = 0.0f;
    };

    static_assert(sizeof(view_constants) == VIEW_CONSTANTS_SIZE, "gpu_view_constants is 1024 bytes");
    static_assert(offsetof(view_constants, cameraPosition) == 320, "gpu_view_constants::cameraPosition offset");
    static_assert(offsetof(view_constants, counts) == 368, "gpu_view_constants::counts offset");
    static_assert(
      offsetof(view_constants, cascadeClipFromWorld) == 480,
      "gpu_view_constants::cascadeClipFromWorld offset"
    );
    static_assert(offsetof(view_constants, shCoefficients) == 736, "gpu_view_constants::shCoefficients offset");
    static_assert(offsetof(view_constants, exposureParams) == 848, "gpu_view_constants::exposureParams offset");
    static_assert(offsetof(view_constants, shadowParams2) == 880, "gpu_view_constants::shadowParams2 offset");
    static_assert(offsetof(view_constants, reserved) == 896, "gpu_view_constants::reserved offset");
    static_assert(sizeof(instance) == INSTANCE_SIZE, "gpu_instance is 96 bytes");
    static_assert(sizeof(light) == LIGHT_SIZE, "gpu_light is 64 bytes");
    static_assert(sizeof(shadow_tile) == SHADOW_TILE_SIZE, "gpu_shadow_tile is 96 bytes");
    static_assert(sizeof(material_constants) == MATERIAL_CONSTANTS_SIZE, "gpu_material_constants is 64 bytes");
    static_assert(sizeof(draw_constants) == DRAW_CONSTANTS_SIZE, "gpu_draw_constants is 16 bytes");
    static_assert(sizeof(exposure_state) == EXPOSURE_STATE_SIZE, "gpu_exposure_state is 16 bytes");
    static_assert(std::is_trivially_copyable_v<view_constants>, "view constants are uploaded verbatim");
    static_assert(std::is_trivially_copyable_v<instance>, "instances are uploaded verbatim");

    /// <summary>
    /// Fills the three row vectors of a world matrix and of its normal matrix (inverse transpose of
    /// the upper 3x3) into an instance record.
    /// </summary>
    [[nodiscard]] instance make_instance(const math::mat4 &worldFromLocal, u32 entityId, u32 flags) noexcept;

    /// <summary>
    /// Reinterprets an unsigned integer as the float that shares its bits (asfloat in HLSL).
    /// </summary>
    [[nodiscard]] f32 bits_as_float(u32 bits) noexcept;

    /// <summary>
    /// Inverse of bits_as_float (asuint in HLSL).
    /// </summary>
    [[nodiscard]] u32 float_as_bits(f32 value) noexcept;
}
