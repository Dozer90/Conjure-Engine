#pragma once

#include "renderer/gpu_types.h"

#include "conjure/renderer/limits.h"

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <nvrhi/nvrhi.h>

#include <array>

// The persistent GPU buffers and textures every frame writes through ICommandList::writeBuffer
// (docs/design/02_renderer.md R6): instances, per-view lights / shadow tiles / tile lists / exposure
// state, sprite and debug vertex data, the volatile view constant buffer, the shadow maps and the
// BRDF lookup table. Buffers grow by recreation when a frame needs more; nothing is ring-buffered.

namespace conjure::renderer {

    /// <summary>
    /// Frames whose command lists may be in flight at once: a swapchain of up to three buffers plus
    /// the one being recorded.
    /// </summary>
    inline constexpr u32 MAX_FRAMES_IN_FLIGHT = 4;

    /// <summary>
    /// Versions of the volatile view constant buffer: renderer.cpp writes it exactly once per view
    /// (prepare_view decides every pass before the write), and every frame in flight keeps its own
    /// versions on Vulkan, so the pool holds one version per view per frame in flight.
    /// </summary>
    inline constexpr u32 VIEW_CONSTANT_WRITES_PER_VIEW = 1;
    inline constexpr u32 VIEW_CONSTANT_VERSIONS = MAX_VIEWS * VIEW_CONSTANT_WRITES_PER_VIEW * MAX_FRAMES_IN_FLIGHT;

    struct view_resources {
        nvrhi::BufferHandle lights = nullptr;           // MAX_LIGHTS_PER_VIEW gpu_light records
        nvrhi::BufferHandle shadowTiles = nullptr;      // atlas tiles then this view's cascades
        u32 shadowTileCapacity = 0;
        nvrhi::BufferHandle tileLists = nullptr;        // tile light lists (uints), sized to the viewport
        u32 tileListCapacity = 0;                       // uints
        nvrhi::BufferHandle histogram = nullptr;        // EXPOSURE_HISTOGRAM_BINS uints, UAV
        nvrhi::BufferHandle exposureState = nullptr;    // one gpu_exposure_state, UAV + SRV, persistent
    };

    class frame_resources {
        private:
            nvrhi::IDevice *mDevice = nullptr;
            nvrhi::BufferHandle mViewConstants = nullptr;
            nvrhi::BufferHandle mInstances = nullptr;
            u32 mInstanceCapacity = 0;
            nvrhi::BufferHandle mSpriteVertices = nullptr;
            u32 mSpriteVertexCapacity = 0;
            nvrhi::BufferHandle mSpriteIndices = nullptr;
            u32 mSpriteIndexCapacity = 0;
            nvrhi::BufferHandle mDebugVertices = nullptr;
            u32 mDebugVertexCapacity = 0;
            std::array<view_resources, MAX_VIEWS> mViews = {};
            nvrhi::TextureHandle mShadowCascades = nullptr;
            nvrhi::TextureHandle mShadowAtlas = nullptr;
            nvrhi::TextureHandle mBrdfLut = nullptr;
            u32 mCascadeResolution = 0;
            u32 mAtlasResolution = 0;
            u32 mBuffersCreated = 0;

        public:
            frame_resources();
            ~frame_resources();

            frame_resources(const frame_resources &) = delete;
            frame_resources &operator=(const frame_resources &) = delete;

        public:
            /// <summary>
            /// Creates the fixed-size resources (view constants, per-view lights / histogram / exposure
            /// state, shadow maps, BRDF LUT) and the initial capacities of the growing buffers. Fails
            /// with renderer_error::RESOURCE_CREATION_FAILED.
            /// </summary>
            [[nodiscard]] core::result<void> create(nvrhi::IDevice &device, u32 cascadeResolution, u32 atlasResolution);

            /// <summary>
            /// Makes the instance buffer hold at least 'count' records (recreating it when it grows).
            /// </summary>
            [[nodiscard]] bool ensure_instance_capacity(u32 count);
            [[nodiscard]] bool ensure_sprite_capacity(u32 vertexCount, u32 indexCount);
            [[nodiscard]] bool ensure_debug_capacity(u32 vertexCount);
            [[nodiscard]] bool ensure_tile_list_capacity(u32 viewIndex, u32 uintCount);
            [[nodiscard]] bool ensure_shadow_tile_capacity(u32 viewIndex, u32 tileCount);

            [[nodiscard]] nvrhi::IBuffer *view_constants() const noexcept {
                return mViewConstants.Get();
            }

            [[nodiscard]] nvrhi::IBuffer *instances() const noexcept {
                return mInstances.Get();
            }

            [[nodiscard]] u32 instance_capacity() const noexcept {
                return mInstanceCapacity;
            }

            [[nodiscard]] nvrhi::IBuffer *sprite_vertices() const noexcept {
                return mSpriteVertices.Get();
            }

            [[nodiscard]] nvrhi::IBuffer *sprite_indices() const noexcept {
                return mSpriteIndices.Get();
            }

            [[nodiscard]] nvrhi::IBuffer *debug_vertices() const noexcept {
                return mDebugVertices.Get();
            }

            [[nodiscard]] view_resources &view(u32 index) noexcept {
                return mViews[index];
            }

            [[nodiscard]] nvrhi::ITexture *shadow_cascades() const noexcept {
                return mShadowCascades.Get();
            }

            [[nodiscard]] nvrhi::ITexture *shadow_atlas() const noexcept {
                return mShadowAtlas.Get();
            }

            [[nodiscard]] nvrhi::ITexture *brdf_lut() const noexcept {
                return mBrdfLut.Get();
            }

            [[nodiscard]] u32 cascade_resolution() const noexcept {
                return mCascadeResolution;
            }

            [[nodiscard]] u32 atlas_resolution() const noexcept {
                return mAtlasResolution;
            }

            /// <summary>
            /// Buffers created since construction (initial plus growths).
            /// </summary>
            [[nodiscard]] u32 buffers_created() const noexcept {
                return mBuffersCreated;
            }

            /// <summary>
            /// Device loss: drops everything.
            /// </summary>
            void release();

        private:
            [[nodiscard]] nvrhi::BufferHandle create_structured(
              u64 byteSize,
              u32 stride,
              bool unorderedAccess,
              const char *name
            );
            [[nodiscard]] nvrhi::BufferHandle create_vertex(u64 byteSize, const char *name);
            [[nodiscard]] nvrhi::BufferHandle create_index(u64 byteSize, const char *name);
    };
}
