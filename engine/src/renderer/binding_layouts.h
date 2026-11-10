#pragma once

#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <nvrhi/nvrhi.h>

#include <unordered_map>

// The binding layouts of every pass (docs/design/02_renderer.md R2): classic NVRHI binding sets,
// registerSpaceIsDescriptorSet = true everywhere, register space 0 for per-view / per-pass resources
// plus the 16-byte push constant block, space 1 for the material set cached on the GPU material. The
// slots mirror the register declarations of the shaders (mesh_pass.hlsli and the pass sources).

namespace conjure::renderer {

    struct binding_layouts {
        nvrhi::BindingLayoutHandle meshView = nullptr;          // shadow_depth, depth_prepass, forward_pbr (space 0)
        nvrhi::BindingLayoutHandle material = nullptr;          // space 1 of the mesh passes
        // skybox, sprite, debug_draw, tonemap, blit, ibl_brdf_lut
        nvrhi::BindingLayoutHandle fullscreen = nullptr;
        nvrhi::BindingLayoutHandle lightCull = nullptr;
        nvrhi::BindingLayoutHandle ssaoGtao = nullptr;          // ssao_gtao (AO and half-resolution depth outputs)
        nvrhi::BindingLayoutHandle ssao = nullptr;              // ssao_blur
        nvrhi::BindingLayoutHandle exposureHistogram = nullptr;
        nvrhi::BindingLayoutHandle exposureAdapt = nullptr;
        nvrhi::BindingLayoutHandle bloom = nullptr;             // bloom_down and bloom_up
        nvrhi::BindingLayoutHandle imgui = nullptr;

        /// <summary>
        /// Creates every layout. Fails with renderer_error::PIPELINE_CREATION_FAILED naming the layout
        /// the device refused.
        /// </summary>
        [[nodiscard]] static core::result<binding_layouts> create(nvrhi::IDevice &device);
    };

    /// <summary>
    /// Binding sets keyed by their description so steady-state frames reuse them instead of creating
    /// descriptor tables every frame. A set keeps its resources alive; entries unused for 120 frames
    /// are released at end_frame.
    /// </summary>
    class binding_set_cache {
        private:
            struct entry {
                nvrhi::BindingSetHandle set = nullptr;
                nvrhi::BindingSetDesc desc = {};
                nvrhi::IBindingLayout *layout = nullptr;
                u64 lastUsedFrame = 0;
            };

            nvrhi::IDevice &mDevice;    // set by the only constructor; outlives the cache
            std::unordered_multimap<u64, entry> mEntries = {};
            u64 mFrameIndex = 0;
            u32 mHits = 0;
            u32 mMisses = 0;

        public:
            explicit binding_set_cache(nvrhi::IDevice &device);
            ~binding_set_cache();

            binding_set_cache(const binding_set_cache &) = delete;
            binding_set_cache &operator=(const binding_set_cache &) = delete;

        public:
            /// <summary>
            /// The set for this description and layout, created on first use; nullptr when the device
            /// refuses.
            /// </summary>
            [[nodiscard]] nvrhi::IBindingSet *get(nvrhi::IBindingLayout *layout, const nvrhi::BindingSetDesc &desc);

            void end_frame(u64 frameIndex);
            void clear();

            [[nodiscard]] usize size() const noexcept {
                return mEntries.size();
            }

            [[nodiscard]] u32 hits() const noexcept {
                return mHits;
            }

            [[nodiscard]] u32 misses() const noexcept {
                return mMisses;
            }

            [[nodiscard]] static u64 hash(nvrhi::IBindingLayout *layout, const nvrhi::BindingSetDesc &desc) noexcept;
    };
}
