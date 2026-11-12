#pragma once

#include "conjure/rhi/device.h"
#include "conjure/rhi/native_window.h"
#include "conjure/rhi/texture_format.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <memory>

// The swapchain (docs/design/02_renderer.md 4.1). Back buffers are NVRHI textures wrapped around
// the API's buffers (createHandleForNativeTexture), in the Present state with keepInitialState,
// so the renderer's framebuffer cache and automatic barriers treat them like any other target.
//
// Threading: render thread only. The swapchain must be destroyed before its device.

namespace nvrhi {
    class ITexture;
}

namespace conjure::rhi {

    struct swapchain_desc {
        u32 width = 0;                                          // pixels; the window's framebuffer size
        u32 height = 0;
        u32 bufferCount = 3;                                    // 2..16
        texture_format format = texture_format::RGBA8_SRGB;     // RGBA8_UNORM, RGBA8_SRGB, BGRA8_SRGB or RGBA16_FLOAT
        bool vsync = true;
    };

    class CONJURE_API swapchain {
        private:
            struct impl;
            struct create_tag {};

            std::unique_ptr<impl> mImpl = nullptr;

        public:
            explicit swapchain(create_tag);
            ~swapchain();

            swapchain(const swapchain &) = delete;
            swapchain &operator=(const swapchain &) = delete;

        public:
            /// <summary>
            /// Creates a swapchain presenting to 'window' (ignored by the null device; a NONE handle on
            /// D3D12 creates a windowless composition swapchain). Fails with rhi_error::INVALID_DESC for a
            /// zero size, a buffer count outside 2..16 or a format the API cannot present,
            /// rhi_error::DEVICE_LOST on a lost device and rhi_error::SWAPCHAIN_CREATION_FAILED when the
            /// API refuses.
            /// </summary>
            [[nodiscard]] static core::result<std::unique_ptr<swapchain>> create(
              device &device,
              const native_window &window,
              const swapchain_desc &desc
            );

            /// <summary>
            /// Acquires the next back buffer and returns its index. Fails with rhi_error::DEVICE_LOST,
            /// rhi_error::OUT_OF_DATE (resize and try again: the surface changed on Vulkan, or a previous
            /// resize() failed and left no back buffers on any backend) or rhi_error::PRESENT_FAILED for
            /// any other refusal by the API.
            /// </summary>
            [[nodiscard]] core::result<u32> acquire();

            /// <summary>
            /// Presents the acquired back buffer. Fails with rhi_error::DEVICE_LOST (the device is then
            /// marked lost), rhi_error::OUT_OF_DATE (resize and retry: the surface changed on Vulkan, or a
            /// previous resize() failed and left no back buffers on any backend) or
            /// rhi_error::PRESENT_FAILED for any other refusal by the API.
            /// </summary>
            [[nodiscard]] core::result<void> present();

            /// <summary>
            /// Recreates the back buffers at a new size after waiting for the GPU; an image acquired but
            /// not yet presented is discarded. Fails with rhi_error::INVALID_DESC for a zero size,
            /// rhi_error::DEVICE_LOST, rhi_error::OUT_OF_DATE (Vulkan: the surface is currently empty,
            /// e.g. the window is minimised; retry later) or rhi_error::SWAPCHAIN_CREATION_FAILED. Every
            /// back_buffer() pointer is invalid afterwards; after a failure buffer_count() may be 0, and
            /// while it is, acquire() and present() fail with rhi_error::OUT_OF_DATE, until a later
            /// resize() succeeds.
            /// </summary>
            [[nodiscard]] core::result<void> resize(u32 width, u32 height);

            /// <summary>
            /// Non-owning back buffer texture, or nullptr for an index >= buffer_count().
            /// </summary>
            [[nodiscard]] nvrhi::ITexture *back_buffer(u32 index) const noexcept;

            /// <summary>
            /// Number of back buffers that exist right now: back_buffer(i) is non-null exactly for
            /// i < buffer_count(). Equals desc().bufferCount except after a failed resize().
            /// </summary>
            [[nodiscard]] u32 buffer_count() const noexcept;

            /// <summary>
            /// Index of the back buffer the next draws target (the last acquire() result).
            /// </summary>
            [[nodiscard]] u32 current_index() const noexcept;

            /// <summary>
            /// The current description: size after resize(), format and buffer count as created.
            /// </summary>
            [[nodiscard]] const swapchain_desc &desc() const noexcept;

            /// <summary>
            /// The device this swapchain was created on.
            /// </summary>
            [[nodiscard]] device &owner() const noexcept;
    };
}
