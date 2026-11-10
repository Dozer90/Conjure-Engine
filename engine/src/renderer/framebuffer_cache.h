#pragma once

#include "conjure/core/types.h"

#include <nvrhi/nvrhi.h>

#include <unordered_map>

// Framebuffers keyed by their attachments (docs/design/02_renderer.md 2.7): the graph asks for one per
// executed pass and the cache hands back the same object while the attachment textures live. A
// framebuffer keeps its textures alive, so a pooled transient texture is released only after the
// cache dropped every framebuffer that references it (remove_texture, called by the transient pool).

namespace conjure::renderer {

    class framebuffer_cache {
        private:
            // set by the only constructor; the renderer owns both and destroys the cache first
            nvrhi::IDevice &mDevice;
            std::unordered_map<u64, nvrhi::FramebufferHandle> mFramebuffers = {};
            u32 mHits = 0;
            u32 mMisses = 0;

        public:
            explicit framebuffer_cache(nvrhi::IDevice &device);
            ~framebuffer_cache();

            framebuffer_cache(const framebuffer_cache &) = delete;
            framebuffer_cache &operator=(const framebuffer_cache &) = delete;

        public:
            /// <summary>
            /// The framebuffer for these attachments, created on first use. Returns nullptr when the
            /// device refuses (an empty description, for example).
            /// </summary>
            [[nodiscard]] nvrhi::IFramebuffer *get(const nvrhi::FramebufferDesc &desc);

            /// <summary>
            /// Drops every framebuffer that references the texture.
            /// </summary>
            void remove_texture(nvrhi::ITexture *texture);

            /// <summary>
            /// Drops every framebuffer.
            /// </summary>
            void clear();

            [[nodiscard]] usize size() const noexcept {
                return mFramebuffers.size();
            }

            [[nodiscard]] u32 hits() const noexcept {
                return mHits;
            }

            [[nodiscard]] u32 misses() const noexcept {
                return mMisses;
            }

            /// <summary>
            /// The hash the cache keys a description by (attachment textures, subresources, formats).
            /// </summary>
            [[nodiscard]] static u64 hash(const nvrhi::FramebufferDesc &desc) noexcept;
    };
}
