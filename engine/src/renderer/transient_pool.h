#pragma once

#include "renderer/framebuffer_cache.h"

#include "conjure/core/types.h"
#include "conjure/rhi/texture_format.h"

#include <nvrhi/nvrhi.h>

#include <unordered_map>
#include <vector>

// Pooled transient resources of the render graph (docs/design/02_renderer.md R3, 2.7). Textures are
// keyed by the hash of their description without the debug name; one texture is never handed out
// twice within a frame; a texture unused for TRANSIENT_EVICTION_FRAMES frames is released at
// end_frame (and the framebuffer cache drops the framebuffers that referenced it). No aliasing.

namespace conjure::renderer {

    struct graph_texture_desc {
        u32 width = 1;
        u32 height = 1;
        u32 arraySize = 1;
        u32 mipLevels = 1;
        rhi::texture_format format = rhi::texture_format::RGBA8_UNORM;
        bool isRenderTarget = false;
        bool isUnorderedAccess = false;
        bool isCubemap = false;
        const char *debugName = "transient";

        /// <summary>
        /// Pool key: every field but the debug name.
        /// </summary>
        [[nodiscard]] u64 hash() const noexcept;

        /// <summary>
        /// Equality without the debug name.
        /// </summary>
        [[nodiscard]] bool same_storage(const graph_texture_desc &other) const noexcept;
    };

    struct graph_buffer_desc {
        u64 byteSize = 0;
        u32 structStride = 0;
        bool isUnorderedAccess = false;
        const char *debugName = "transient";

        [[nodiscard]] u64 hash() const noexcept;
        [[nodiscard]] bool same_storage(const graph_buffer_desc &other) const noexcept;
    };

    class transient_pool {
        private:
            struct pooled_texture {
                nvrhi::TextureHandle texture = nullptr;
                u64 lastUsedFrame = 0;
                u64 inUseFrame = 0;
                bool inUse = false;
            };

            struct pooled_buffer {
                nvrhi::BufferHandle buffer = nullptr;
                u64 lastUsedFrame = 0;
                bool inUse = false;
            };

            nvrhi::IDevice &mDevice;                        // set by the only constructor; outlives the pool
            framebuffer_cache *mFramebuffers = nullptr;     // notified on eviction; optional
            std::unordered_map<u64, std::vector<pooled_texture>> mTextures = {};
            std::unordered_map<u64, std::vector<pooled_buffer>> mBuffers = {};
            u32 mTexturesCreated = 0;
            u32 mBuffersCreated = 0;
            u32 mEvictionFrames = 120;

        public:
            transient_pool(nvrhi::IDevice &device, framebuffer_cache *framebuffers);
            ~transient_pool();

            transient_pool(const transient_pool &) = delete;
            transient_pool &operator=(const transient_pool &) = delete;

        public:
            /// <summary>
            /// A texture matching the description that is free this frame, created when none is.
            /// Returns nullptr when the device refuses the description.
            /// </summary>
            [[nodiscard]] nvrhi::ITexture *acquire(const graph_texture_desc &desc, u64 frameIndex);

            /// <summary>
            /// A buffer matching the description that is free this frame, created when none is.
            /// </summary>
            [[nodiscard]] nvrhi::IBuffer *acquire_buffer(const graph_buffer_desc &desc, u64 frameIndex);

            /// <summary>
            /// Releases every in-use mark and evicts resources unused for the eviction window.
            /// </summary>
            void end_frame(u64 frameIndex);

            /// <summary>
            /// Releases everything.
            /// </summary>
            void clear();

            /// <summary>
            /// Textures currently pooled (in use or free).
            /// </summary>
            [[nodiscard]] u32 live_texture_count() const noexcept;

            /// <summary>
            /// Textures created since the last reset_counters().
            /// </summary>
            [[nodiscard]] u32 textures_created() const noexcept {
                return mTexturesCreated;
            }

            [[nodiscard]] u32 buffers_created() const noexcept {
                return mBuffersCreated;
            }

            void reset_counters() noexcept {
                mTexturesCreated = 0;
                mBuffersCreated = 0;
            }

            /// <summary>
            /// Frames a resource may stay unused before eviction (TRANSIENT_EVICTION_FRAMES by default).
            /// </summary>
            void set_eviction_frames(u32 frames) noexcept {
                mEvictionFrames = frames;
            }

            /// <summary>
            /// The NVRHI description of a graph texture: keepInitialState with ShaderResource for
            /// render targets and depth, UnorderedAccess for compute outputs.
            /// </summary>
            [[nodiscard]] static nvrhi::TextureDesc to_nvrhi(const graph_texture_desc &desc);

            [[nodiscard]] static nvrhi::BufferDesc to_nvrhi(const graph_buffer_desc &desc);
    };
}
