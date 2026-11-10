#pragma once

#include "conjure/core/types.h"

// Fixed budgets of the renderer (docs/design/02_renderer.md 2.1). The same values are #defined in
// engine/shaders/shared/gpu_types.hlsli; tests/renderer/test_gpu_types.cpp compares both sides.

namespace conjure::renderer {

    inline constexpr u32 MAX_VIEWS = 8;
    inline constexpr u32 MAX_LIGHTS_PER_VIEW = 1024;
    inline constexpr u32 MAX_LIGHTS_PER_TILE = 64;
    inline constexpr u32 LIGHT_TILE_SIZE = 16;
    inline constexpr u32 MAX_SHADOW_CASCADES = 4;
    inline constexpr u32 MAX_SPRITES_PER_FRAME = 65536;
    inline constexpr u32 MAX_DEBUG_VERTICES = 1u << 20;
    inline constexpr u32 MAX_MATERIAL_SLOTS = 8;
    inline constexpr u32 INVALID_INDEX = 0xFFFFFFFFu;

    /// <summary>
    /// Number of texture slots of a material: base colour, metallic-roughness, normal, occlusion, emissive.
    /// </summary>
    inline constexpr u32 MATERIAL_TEXTURE_SLOTS = 5;

    /// <summary>
    /// Mip levels of the environment cubemaps (sky and prefiltered specular).
    /// </summary>
    inline constexpr u32 ENVIRONMENT_MIP_COUNT = 6;

    /// <summary>
    /// Faces of a cubemap, in the +X, -X, +Y, -Y, +Z, -Z order of D3D and Vulkan.
    /// </summary>
    inline constexpr u32 CUBEMAP_FACE_COUNT = 6;

    /// <summary>
    /// Bins of the auto-exposure luminance histogram.
    /// </summary>
    inline constexpr u32 EXPOSURE_HISTOGRAM_BINS = 256;

    /// <summary>
    /// Mip levels of the bloom chain.
    /// </summary>
    inline constexpr u32 BLOOM_MIP_COUNT = 6;

    /// <summary>
    /// Frames a transient texture stays pooled without use before it is released.
    /// </summary>
    inline constexpr u32 TRANSIENT_EVICTION_FRAMES = 120;

    /// <summary>
    /// Bytes of asset payload uploaded per frame at most; the remainder waits for the next frame.
    /// </summary>
    inline constexpr u64 UPLOAD_BUDGET_BYTES_PER_FRAME = u64{64} * 1024 * 1024;
}
